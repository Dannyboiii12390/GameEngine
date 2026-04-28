#include "FlatBufferScene.h"
#include "../Managers/ResourceManager.h"
#include "../../IMGUI/imgui.h"
#include "../Systems/SystemSwapBuffers.h"
#include "../../../PhysicsEngine/Networking/TCPSocket.h"
#include "../../../PhysicsEngine/Shapes/Sphere.h"
#include "../../../PhysicsEngine/Shapes/Capsule.h"
#include "../../../PhysicsEngine/Shapes/Cylinder.h"
#include "../../../PhysicsEngine/Shapes/Plane.h"
#include "../Components/ComponentCollision.h"

#include <fstream>
#include <iostream>
#include <vector>
#include <filesystem>
#include <string>
#include <random>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <array>
#include <cmath> // added for volume/pow calculations

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <iphlpapi.h>

#include "../Systems/SystemVelocity.h"
#include "../Systems/SystemPhysics.h"
#include "../Systems/SystemCollision.h"
#include "../Systems/SystemFlocking.h"
#include "BallDropScene.h"
#include "PanningScene.h"
#include "TemplateScene.h"
#include "../Managers/SceneManager.h"
#include "../Components/ComponentNetwork.h"

// Undefine Windows macros breaking flatbuffer generation
#if defined(near)
#undef near
#endif

#if defined(far)
#undef far
#endif

#include "../../Assets/Scene_generated.h"
#include <flatbuffers/flatbuffers.h>
#include "../../DebugUtils.h"
#include "../Systems/SystemNetworkSync.h"
#include "../Components/ComponentAnimation.h"
#include "AnimationScene.h"


namespace
{
	enum class ThreadRole
	{
		Graphics,
		Network,
		Simulation
	};

	constexpr float kPI = 3.14159265358979323846f;

	DWORD_PTR BuildMaskFromRange(uint32_t start, uint32_t endInclusive, uint32_t coreCount)
	{
		if (coreCount == 0)
			return 0;

		start = std::min(start, coreCount - 1);
		endInclusive = std::min(endInclusive, coreCount - 1);

		if (start > endInclusive)
			return 0;

		DWORD_PTR mask = 0;
		for (uint32_t i = start; i <= endInclusive; ++i)
		{
			mask |= (static_cast<DWORD_PTR>(1) << i);
		}
		return mask;
	}

	void ApplyThreadAffinity(ThreadRole role)
	{
		const uint32_t coreCount = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
		DWORD_PTR mask = 0;

		switch (role)
		{
		case ThreadRole::Graphics:
			mask = BuildMaskFromRange(0, 0, coreCount);
			break;
		case ThreadRole::Network:
			mask = BuildMaskFromRange(1, 2, coreCount);
			break;
		case ThreadRole::Simulation:
			mask = BuildMaskFromRange(3, coreCount > 0 ? coreCount - 1 : 0, coreCount);
			break;
		}

		if (mask == 0)
		{
			mask = BuildMaskFromRange(0, coreCount > 0 ? coreCount - 1 : 0, coreCount);
		}

		if (mask != 0)
		{
			SetThreadAffinityMask(GetCurrentThread(), mask);
		}

	}
	// 3) Add helper functions in anonymous namespace:
	static PeerID OwnerTypeToPeerId(Simulation::ObjectOwnerType owner)
	{
		switch (owner)
		{
		case Simulation::ObjectOwnerType_ONE: return 0;
		case Simulation::ObjectOwnerType_TWO: return 1;
		case Simulation::ObjectOwnerType_THREE: return 2;
		case Simulation::ObjectOwnerType_FOUR: return 3;
		default: return 0;
		}
	}

	static Simulation::ObjectOwnerType PeerIdToOwnerType(PeerID ownerId)
	{
		switch (ownerId)
		{
		case 0: return Simulation::ObjectOwnerType_ONE;
		case 1: return Simulation::ObjectOwnerType_TWO;
		case 2: return Simulation::ObjectOwnerType_THREE;
		case 3: return Simulation::ObjectOwnerType_FOUR;
		default: return Simulation::ObjectOwnerType_ONE;
		}
	}

	static ObjectType BehaviourToObjectType(Simulation::Behaviour b)
	{
		switch (b)
		{
		case Simulation::Behaviour_StaticObject: return ObjectType::Static;
		case Simulation::Behaviour_AnimatedObject: return ObjectType::Animated;
		case Simulation::Behaviour_SimulatedObject:
		default: return ObjectType::Simulated;
		}
	}
	static glm::vec4 OwnerToColor(PeerID ownerId)
	{
		switch (ownerId)
		{
		case 0: return glm::vec4(1.0f, 0.15f, 0.15f, 1.0f); // Red
		case 1: return glm::vec4(0.15f, 1.0f, 0.15f, 1.0f); // Green
		case 2: return glm::vec4(0.15f, 0.35f, 1.0f, 1.0f); // Blue
		case 3: return glm::vec4(1.0f, 1.0f, 0.15f, 1.0f); // Yellow
		default: return glm::vec4(0.75f, 0.75f, 0.75f, 1.0f); // ALL_PEERS / fallback
		}
	}

	static Texture CreateOwnerTexture(VulkanRHI* rhi, PeerID ownerId)
	{
		const glm::vec4 c = OwnerToColor(ownerId);
		const unsigned char px[4] = {
			static_cast<unsigned char>(std::clamp(c.r, 0.0f, 1.0f) * 255.0f),
			static_cast<unsigned char>(std::clamp(c.g, 0.0f, 1.0f) * 255.0f),
			static_cast<unsigned char>(std::clamp(c.b, 0.0f, 1.0f) * 255.0f),
			static_cast<unsigned char>(std::clamp(c.a, 0.0f, 1.0f) * 255.0f)
		};

		if (auto tex = Texture::CreateFromMemory(rhi, px, 1, 1, 4, TextureType::Albedo, false))
			return *tex;

		// fallback if memory texture creation fails
		return Texture(rhi, "Assets/red_brick_diff_1k.jpg", TextureType::Albedo, true);
	}

	static constexpr PeerID kRuntimePeerCount = 3;
	static PeerID RemapOwnerForRuntime(PeerID ownerId)
	{
		if (ownerId == ALL_PEERS)
			return ALL_PEERS;
		return ownerId % kRuntimePeerCount;
	}

	// add these helper mappings inside anonymous namespace

	static RuntimeSpawner::SpawnType ToRuntimeSpawnType(uint8_t spawnType)
	{
		switch (static_cast<Simulation::SpawnType>(spawnType))
		{
		case Simulation::SpawnType_SingleBurstSpawn: return RuntimeSpawner::SpawnType::SingleBurst;
		case Simulation::SpawnType_RepeatingSpawn:   return RuntimeSpawner::SpawnType::Repeating;
		default:                                     return RuntimeSpawner::SpawnType::None;
		}
	}

	static RuntimeSpawner::SpawnLocation ToRuntimeSpawnLocation(uint8_t locationType)
	{
		switch (static_cast<Simulation::SpawnLocation>(locationType))
		{
		case Simulation::SpawnLocation_FixedLocation: return RuntimeSpawner::SpawnLocation::Fixed;
		case Simulation::SpawnLocation_RandomBox:     return RuntimeSpawner::SpawnLocation::Box;
		case Simulation::SpawnLocation_RandomSphere:  return RuntimeSpawner::SpawnLocation::Sphere;
		default:                                      return RuntimeSpawner::SpawnLocation::None;
		}
	}

	static RuntimeSpawner::SpawnerShapeType ToRuntimeSpawnerShape(uint8_t spawnerType)
	{
		switch (static_cast<Simulation::SpawnerType>(spawnerType))
		{
		case Simulation::SpawnerType_CylinderSpawner: return RuntimeSpawner::SpawnerShapeType::Cylinder;
		case Simulation::SpawnerType_CapsuleSpawner:  return RuntimeSpawner::SpawnerShapeType::Capsule;
		case Simulation::SpawnerType_CuboidSpawner:   return RuntimeSpawner::SpawnerShapeType::Cuboid;
		case Simulation::SpawnerType_SphereSpawner:
		default:                                      return RuntimeSpawner::SpawnerShapeType::Sphere;
		}
	}

	static Simulation::Shape SpawnerTypeToShape(uint8_t spawnerType)
	{
		switch (static_cast<Simulation::SpawnerType>(spawnerType))
		{
		case Simulation::SpawnerType_CylinderSpawner: return Simulation::Shape_Cylinder;
		case Simulation::SpawnerType_CapsuleSpawner:  return Simulation::Shape_Capsule;
		case Simulation::SpawnerType_CuboidSpawner:   return Simulation::Shape_Cuboid;
		case Simulation::SpawnerType_SphereSpawner:
		default:                                      return Simulation::Shape_Sphere;
		}
	}

	static PeerID SpawnerOwnerToPeerId(Simulation::SpawnerOwnerType owner)
	{
		switch (owner)
		{
		case Simulation::SpawnerOwnerType_ONE:   return 0;
		case Simulation::SpawnerOwnerType_TWO:   return 1;
		case Simulation::SpawnerOwnerType_THREE: return 2;
		case Simulation::SpawnerOwnerType_FOUR:  return 3;
		default:                                 return 0;
		}
	}

	// Add near existing mapping helpers in anonymous namespace:
	static Simulation::Behaviour ObjectTypeToBehaviour(ObjectType type)
	{
		switch (type)
		{
		case ObjectType::Static:    return Simulation::Behaviour_StaticObject;
		case ObjectType::Animated:  return Simulation::Behaviour_AnimatedObject;
		case ObjectType::Simulated:
		default:                    return Simulation::Behaviour_SimulatedObject;
		}
	}

	// New helper: lookup density from material table (default 1.0f if not found or invalid)
	static float LookupMaterialDensity(const std::vector<MaterialData>& mats, const std::string& name)
	{
		if (name.empty()) return 1.0f;
		for (const auto& m : mats)
		{
			if (m.name == name)
				return (m.density > 0.0f) ? m.density : 1.0f;
		}
		return 1.0f;
	}

	// New helper: compute approximate volume for common shapes using passed dimensions.
	static float ComputeVolumeForShape(Simulation::Shape shape, const glm::vec3& scale, float radius, float height)
	{
		switch (shape)
		{
		case Simulation::Shape_Sphere:
		{
			// radius is taken directly
			float r = std::max(0.0f, radius);
			return (4.0f / 3.0f) * kPI * r * r * r;
		}
		case Simulation::Shape_Cylinder:
		{
			float r = std::max(0.0f, radius);
			float h = std::max(0.0f, height);
			return kPI * r * r * h;
		}
		case Simulation::Shape_Capsule:
		{
			float r = std::max(0.0f, radius);
			float h = std::max(0.0f, height);
			// cylinder portion + two hemispheres
			return kPI * r * r * h + (4.0f / 3.0f) * kPI * r * r * r;
		}
		case Simulation::Shape_Plane:
		{
			// Plane has effectively no volume - return small value to avoid zero-mass issues.
			return 0.0f;
		}
		case Simulation::Shape_Cuboid:
		default:
		{
			// treat scale as full size (width x height x depth)
			float sx = std::max(0.0f, scale.x);
			float sy = std::max(0.0f, scale.y);
			float sz = std::max(0.0f, scale.z);
			return sx * sy * sz;
		}
		}
	}
}

// Call once at startup (before OpenMP work)
void LimitOpenMPCores()
{
	// Core 0 = bit 0, Core 1 = bit 1, etc.
	// Disable cores 1,2,3 -> mask excludes bits 1,2,3
	const DWORD_PTR allowedMask = ~((1ull << 1) | (1ull << 2) | (1ull << 3));
	SetProcessAffinityMask(GetCurrentProcess(), allowedMask);
}

FlatBufferScene::FlatBufferScene(Window& p_window, VulkanRHI* rhi, GUI* p_gui) :
	m_window(&p_window), m_inputHandler(p_window), m_vulkanRHI(rhi),
	m_gui(p_gui), m_systemManager(rhi, 2)
{
	LimitOpenMPCores();

	m_instanceId = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());

	// Host defaults to id 0. Clients are reassigned by host during handshake.
	m_networkRole = NetworkRole::Host;
	m_localPeerId.store(0);

	// Camera, vulkan, and imgui Initialisation
	m_cameras.reserve(100);
	m_paused = false;

	// Ensure sensible defaults for scene-level properties so missing flatbuffer fields
	// fall back to explicit defaults.
	m_sceneName = "";
	m_sceneDescription = "";
	m_gravityOn = true;
	m_selectedEntityIndex = -1;

	// 1. Ensure the file exists before attempting to load
	if (!std::filesystem::exists("scenes/Level1.bin"))
	{
		std::cout << "Bin file not found, creation a default one...\n";
		SerializeState();
	}

	// 2. Load properties directly into the scene
	DeserializeState();

	if (m_cameras.empty())
	{
		float aspect = 16.0f / 9.0f;
		if (m_vulkanRHI)
		{
			auto extent = m_vulkanRHI->GetSwapchainExtent();
			if (extent.height != 0)
				aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
		}
		m_cameras.emplace_back(60.0f, aspect, 0.1f, 1000.0f);
	}

	m_activeCamera = &m_cameras[0];
	if (m_vulkanRHI)
	{
		m_vulkanRHI->SetActiveCamera(m_activeCamera);
	}

	// Lambda now accepts the texture to apply as an explicit parameter.
	auto createBoid = [&](glm::vec3 pos, const Texture& tex)
	{
		Entity boid;
		boid.AddComponent(EComponentType::Component_Transform, pos, glm::vec3(0.0f), glm::vec3(1.0f));
		boid.AddComponent(EComponentType::Component_Velocity, glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(0.0f));
		boid.AddComponent(EComponentType::Component_Physics);
		auto* phys = boid.GetComponent<ComponentPhysics>(EComponentType::Component_Physics);
		phys->SetAffectedByGravity(false);
		boid.AddComponent(EComponentType::Component_Geometry);
		ComponentGeometry* geom = boid.GetComponent<ComponentGeometry>(EComponentType::Component_Geometry);
		//if (geom)
		{
			MeshData meshData = ResourceManager::CreateSphereMesh(); // Fallback visible mesh
			auto [verts, indices] = meshData;
			geom->InitializeMesh(m_vulkanRHI, verts, indices);
			geom->InitializePipeline(m_vulkanRHI, m_vulkanRHI->GetRenderPass(), m_vulkanRHI->GetSwapchainExtent(), "SHADERS/object.vert.spv", "SHADERS/object.frag.spv");
			// Note: Requires a valid texture
			geom->AddTexture(m_vulkanRHI, tex);
		}
		m_entities.push_back(std::move(boid));
	};

	// default texture to pass into the lambda
	//const Texture defaultTex(m_vulkanRHI, "Assets/red_brick_diff_1k.jpg", TextureType::Albedo, true);
	//for(int i = 0; i < num_boids; ++i)
	//{
	//	glm::vec3 pos = glm::vec3(
	//		static_cast<float>(rand() % 20 - 10),
	//		static_cast<float>(rand() % 20 - 10),
	//		static_cast<float>(rand() % 20 - 10)
	//	);
	//	createBoid(pos, defaultTex);
	//}

	{
		auto address = GetClientAddress();
		std::cout << "Server listening on " << address.getIP() << ":" << address.getPort() << std::endl;
	}

	PeerID localPeerId = m_localPeerId.load();

	Entity floor;
	const glm::vec3 floorPos(0.0f, -5.0f, 0.0f);

	floor.AddComponent(
		EComponentType::Component_Transform,
		floorPos,
		glm::vec3(-90.0f, 0.0f, 0.0f),
		glm::vec3(100.0f, 1.0f, 100.0f));

	floor.AddComponent(EComponentType::Component_Geometry);
	if (auto* geom = floor.GetComponent<ComponentGeometry>(EComponentType::Component_Geometry))
	{
		MeshData meshData = ResourceManager::CreatePlaneMesh(1.0f, 100.0f, 100.0f, 1, 1);
		auto [verts, indices] = meshData;

		geom->InitializeMesh(m_vulkanRHI, verts, indices);
		geom->InitializePipeline(
			m_vulkanRHI,
			m_vulkanRHI->GetRenderPass(),
			m_vulkanRHI->GetSwapchainExtent(),
			"SHADERS/object.vert.spv",
			"SHADERS/object.frag.spv");

		const unsigned char purple[4] = { 128, 0, 128, 255 };
		if (auto tex = Texture::CreateFromMemory(m_vulkanRHI, purple, 1, 1, 4, TextureType::Albedo, false))
		{
			geom->AddTexture(m_vulkanRHI, *tex);
		}
		else
		{
			Texture fallback(m_vulkanRHI, "Assets/red_brick_diff_1k.jpg", TextureType::Albedo, true);
			geom->AddTexture(m_vulkanRHI, fallback);
		}
	}

	floor.AddComponent(EComponentType::Component_Collision);
	if (auto* collision = floor.GetComponent<ComponentCollision>(EComponentType::Component_Collision))
	{
		collision->SetCollisionRole(CollisionRole::Container);

		// Match BallDropScene: start with XY plane basis.
		// The transform rotation (-90,0,0) applied by SystemCollision rotates this to a horizontal floor.
		collision->SetCollider(std::make_unique<Physics::Plane>(
			floorPos,
			glm::vec3(1.0f, 0.0f, 0.0f),
			glm::vec3(0.0f, 1.0f, 0.0f)));
	}

	m_entities.push_back(std::move(floor));


	// 4. Register the required systems so objects are rendered
	m_systemManager.RegisterSystem(std::make_unique<SystemVelocity>());
	m_systemManager.RegisterSystem(std::make_unique<SystemFlocking>(m_vulkanRHI, &m_localPeerId));
	m_systemManager.RegisterSystem(std::make_unique<SystemPhysics>(&m_localPeerId));
	m_systemManager.RegisterSystem(std::make_unique<SystemCollision>(m_entities.size(), m_vulkanRHI, &m_localPeerId));
	m_systemManager.RegisterSystem(std::make_unique<SystemSwapBuffers>());

	m_networkData = std::make_shared<SharedNetworkData>();
	m_systemManager.RegisterSystem(std::make_unique<SystemNetworkSync>(&m_localPeerId, m_networkData));
}

// REPLACE GetClientAddress() completely
Networking::Address FlatBufferScene::GetClientAddress()
{
	// Manual overrides (set these for campus demos)
	// - Leave empty/0 to use normal UDP discovery + random port.
	static constexpr const char* kManualHost = "";  // e.g. "192.168.1.50"
	static constexpr uint16_t kManualHostPort = 0;  // e.g. 12664
	static constexpr uint16_t kFixedTcpPort = 0;    // e.g. 12664 (host only)

	static std::mt19937 rng{ std::random_device{}() };
	static std::uniform_int_distribution<int> dist(10000, 19999);

	const int random = (kFixedTcpPort != 0) ? static_cast<int>(kFixedTcpPort) : dist(rng);

	if (kFixedTcpPort != 0)
	{
		std::cout << "Using fixed TCP port " << kFixedTcpPort << "\n";
	}

	Networking::Address bindAddr("0.0.0.0", random);
	m_tcpListener = std::make_unique<Networking::ListeningSocket>(bindAddr);
	m_tcpListener->SetNonBlocking(true);

	m_networkRunning = true;
	m_networkThread = std::thread([this, random]() {
		ApplyThreadAffinity(ThreadRole::Network);

		SOCKET udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (udpSocket != INVALID_SOCKET) {
			char broadcastEnable = 1;
			setsockopt(udpSocket, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));

			int reuse = 1;
			setsockopt(udpSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

			sockaddr_in listenAddr{};
			listenAddr.sin_family = AF_INET;
			listenAddr.sin_port = htons(8888);
			listenAddr.sin_addr.s_addr = INADDR_ANY;
			bind(udpSocket, reinterpret_cast<sockaddr*>(&listenAddr), sizeof(listenAddr));

			u_long mode = 1;
			ioctlsocket(udpSocket, FIONBIO, &mode);
		}

		auto buildBroadcastTargets = []()
		{
			std::vector<sockaddr_in> targets;

			auto addTarget = [&targets](const sockaddr_in& addr)
			{
				for (const auto& t : targets)
				{
					if (t.sin_addr.s_addr == addr.sin_addr.s_addr && t.sin_port == addr.sin_port)
						return;
				}
				targets.push_back(addr);
			};

			auto makeTarget = [](uint32_t addrNetworkOrder)
			{
				sockaddr_in target{};
				target.sin_family = AF_INET;
				target.sin_port = htons(8888);
				target.sin_addr.s_addr = addrNetworkOrder;
				return target;
			};

			// Global broadcast (works on many home/LANs).
			addTarget(makeTarget(INADDR_BROADCAST));

			// Enumerate adapters for directed broadcasts (campus-friendly).
			ULONG size = 0;
			if (GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER, nullptr, nullptr, &size) == ERROR_BUFFER_OVERFLOW)
			{
				std::vector<uint8_t> buffer(size);
				auto* adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
				if (GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER, nullptr, adapters, &size) == NO_ERROR)
				{
					for (auto* adapter = adapters; adapter; adapter = adapter->Next)
					{
						if (adapter->OperStatus != IfOperStatusUp)
							continue;

						if (adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK)
							continue;

						for (auto* uni = adapter->FirstUnicastAddress; uni; uni = uni->Next)
						{
							if (!uni->Address.lpSockaddr || uni->Address.lpSockaddr->sa_family != AF_INET)
								continue;

							const auto* sa = reinterpret_cast<const sockaddr_in*>(uni->Address.lpSockaddr);
							const uint32_t prefix = std::min<uint32_t>(uni->OnLinkPrefixLength, 32);
							const uint32_t ipHost = ntohl(sa->sin_addr.s_addr);

							const uint32_t maskHost = (prefix == 0) ? 0u : (0xFFFFFFFFu << (32 - prefix));
							const uint32_t broadcastHost = (ipHost & maskHost) | (~maskHost);

							addTarget(makeTarget(htonl(broadcastHost)));
						}
					}
				}
			}

			return targets;
		};

		std::vector<sockaddr_in> broadcastTargets = buildBroadcastTargets();

		const bool hasHostOverride = kManualHost[0] != '\0' && kManualHostPort != 0;

		if (hasHostOverride)
		{
			std::cout << "Using host override " << kManualHost << ":" << kManualHostPort << "\n";
			m_networkRole = NetworkRole::Client;
		}

		auto lastDirectAttempt = std::chrono::steady_clock::now() - std::chrono::seconds(5);

		struct HelloMsg {
			uint32_t magic;
			char instanceId[64];
		};

		struct WelcomeMsg {
			uint32_t magic;
			PeerID assignedPeerId;
			PeerID runtimePeerCount;
		};

		static constexpr uint32_t kHelloMagic = 0x48454C4F;   // HELO
		static constexpr uint32_t kWelcomeMagic = 0x57454C43; // WELC

		struct ClientInfo {
			SOCKET socket = INVALID_SOCKET;
			PeerID assignedPeerId = ALL_PEERS;
			bool handshakeDone = false;
			std::string peerInstanceId;
			std::vector<uint8_t> rxBuffer; // NEW: stream reassembly buffer
		};

		std::vector<ClientInfo> clientSockets;
		PeerID nextAssignedPeerId = 1; // host keeps 0
		std::string connectedHostInstanceId;

		std::vector<uint8_t> hostRxBuffer; // NEW: client-side stream reassembly
		bool clientGotWelcome = false;     // NEW

		auto lastBroadcastTime = std::chrono::steady_clock::now();

		while (m_networkRunning) {
			const auto now = std::chrono::steady_clock::now();

			if (hasHostOverride && !m_peerConnected &&
				std::chrono::duration_cast<std::chrono::seconds>(now - lastDirectAttempt).count() >= 1)
			{
				lastDirectAttempt = now;
				try
				{
					m_tcpClient = std::make_unique<Networking::TCPSocket>(
						Networking::Address(kManualHost, kManualHostPort));

					u_long mode = 1;
					ioctlsocket(m_tcpClient->native_handle(), FIONBIO, &mode);

					m_networkRole = NetworkRole::Client;
					m_peerConnected = true;

					HelloMsg hello{};
					hello.magic = kHelloMagic;
					strncpy_s(hello.instanceId, sizeof(hello.instanceId), m_instanceId.c_str(), sizeof(hello.instanceId) - 1);
					hello.instanceId[sizeof(hello.instanceId) - 1] = '\0';

					send(m_tcpClient->native_handle(),
						reinterpret_cast<const char*>(&hello),
						static_cast<int>(sizeof(hello)),
						0);

					std::cout << "Connected to host " << kManualHost << ":" << kManualHostPort << "\n";
				}
				catch (const std::exception& ex)
				{
					std::cerr << "Host connect failed: " << ex.what() << "\n";
					m_peerConnected = false;
					m_tcpClient.reset();
				}
			}

			// Broadcast presence
			if (!hasHostOverride && udpSocket != INVALID_SOCKET &&
				std::chrono::duration_cast<std::chrono::seconds>(now - lastBroadcastTime).count() >= 1) {
				const std::string msg = "SERVER_DISCOVERY:" + std::to_string(random) + ":" + m_instanceId;

				for (const auto& target : broadcastTargets)
				{
					sendto(udpSocket, msg.c_str(), static_cast<int>(msg.size()), 0,
						reinterpret_cast<const sockaddr*>(&target), sizeof(target));
				}

				lastBroadcastTime = now;
			}

			// Discovery + host election (lowest instanceId becomes host)
			if (!hasHostOverride && udpSocket != INVALID_SOCKET && !m_peerConnected) {
				char recvBuf[256]{};
				sockaddr_in from{};
				int fromLen = sizeof(from);
				const int recvBytes = recvfrom(udpSocket, recvBuf, sizeof(recvBuf) - 1, 0,
					reinterpret_cast<sockaddr*>(&from), &fromLen);

				if (recvBytes > 0) {
					recvBuf[recvBytes] = '\0';
					const std::string msg(recvBuf);

					const std::string prefix = "SERVER_DISCOVERY:";
					if (msg.rfind(prefix, 0) == 0) {
						const std::string payload = msg.substr(prefix.size());
						const size_t sep = payload.find(':');
						if (sep != std::string::npos) {
							const int peerPort = std::stoi(payload.substr(0, sep));
							const std::string peerInstanceId = payload.substr(sep + 1);

							if (!peerInstanceId.empty() && peerInstanceId != m_instanceId && peerPort != random) {
								// If we discover a "smaller" instanceId, we must be a client of that host.
								if (peerInstanceId < m_instanceId) {
									try {
										char ipStr[INET_ADDRSTRLEN]{};
										inet_ntop(AF_INET, &from.sin_addr, ipStr, sizeof(ipStr));

										m_tcpClient = std::make_unique<Networking::TCPSocket>(
											Networking::Address(ipStr, static_cast<uint16_t>(peerPort)));

										u_long mode = 1;
										ioctlsocket(m_tcpClient->native_handle(), FIONBIO, &mode);

										m_networkRole = NetworkRole::Client;
										m_peerConnected = true;
										connectedHostInstanceId = peerInstanceId;

										HelloMsg hello{};
										hello.magic = kHelloMagic;
										strncpy_s(hello.instanceId, sizeof(hello.instanceId), m_instanceId.c_str(), sizeof(hello.instanceId) - 1);
										hello.instanceId[sizeof(hello.instanceId) - 1] = '\0';

										send(m_tcpClient->native_handle(),
											reinterpret_cast<const char*>(&hello),
											static_cast<int>(sizeof(hello)),
											0);

										std::cout << "Connected to host " << ipStr << ":" << peerPort << "\n";
									}
									catch (const std::exception& ex) {
										std::cerr << "Host connect failed: " << ex.what() << "\n";
										m_peerConnected = false;
										m_tcpClient.reset();
									}
								}
							}
						}
					}
				}
			}

			// Host: accept up to runtimePeerCount-1 clients
			if (m_networkRole == NetworkRole::Host &&
				static_cast<PeerID>(clientSockets.size()) < (kRuntimePeerCount - 1)) {
				Networking::Address clientAddr;
				SOCKET newClient = m_tcpListener->Accept(clientAddr);
				if (newClient != INVALID_SOCKET) {
					u_long mode = 1;
					ioctlsocket(newClient, FIONBIO, &mode);

					ClientInfo ci{};
					ci.socket = newClient;
					clientSockets.push_back(ci);

					std::cout << "Client connected from: "
						<< clientAddr.getIP() << ":" << clientAddr.getPort() << "\n";
				}
			}

			// Pull outgoing packets from simulation/system layer
			std::vector<SyncPacket> outgoing;
			if (m_networkData) {
				std::lock_guard<std::mutex> lock(m_networkData->outgoingMutex);
				outgoing = m_networkData->outgoingPackets;
				m_networkData->outgoingPackets.clear();
			}

			// Host path: send local outgoing to clients + receive client state and relay
			if (m_networkRole == NetworkRole::Host) {
				// Host always id 0
				m_localPeerId.store(0);

				// Send host-local outgoing to all handshaken clients
				for (auto& ci : clientSockets) {
					if (!ci.handshakeDone || ci.socket == INVALID_SOCKET) continue;
					for (const auto& packet : outgoing) {
						int sent = send(ci.socket, reinterpret_cast<const char*>(&packet), sizeof(SyncPacket), 0);
						if (sent == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK) {
							closesocket(ci.socket);
							ci.socket = INVALID_SOCKET;
							break;
						}
					}
				}

				for (auto it = clientSockets.begin(); it != clientSockets.end();) {
					ClientInfo& ci = *it;
					if (ci.socket == INVALID_SOCKET) {
						it = clientSockets.erase(it);
						continue;
					}

					bool clientOk = true;
					char recvBuf[512]{};
					int bytes = 0;

					do {
						bytes = recv(ci.socket, recvBuf, sizeof(recvBuf), 0);

							if (bytes > 0) {
						 const uint8_t* begin = reinterpret_cast<const uint8_t*>(recvBuf);
						 ci.rxBuffer.insert(ci.rxBuffer.end(), begin, begin + bytes);
						}

						// Parse as stream: first handshake, then sync packets
						for (;;)
						{
							if (!ci.handshakeDone)
							{
								if (ci.rxBuffer.size() < sizeof(HelloMsg))
									break;

								HelloMsg hello{};
								std::memcpy(&hello, ci.rxBuffer.data(), sizeof(HelloMsg));
								ci.rxBuffer.erase(ci.rxBuffer.begin(), ci.rxBuffer.begin() + static_cast<std::ptrdiff_t>(sizeof(HelloMsg)));

								if (hello.magic != kHelloMagic)
								{
									clientOk = false;
									break;
								}

								ci.peerInstanceId = hello.instanceId;

								if (nextAssignedPeerId >= kRuntimePeerCount) {
									clientOk = false;
									break;
								}

								ci.assignedPeerId = nextAssignedPeerId++;
								ci.handshakeDone = true;

								WelcomeMsg welcome{};
								welcome.magic = kWelcomeMagic;
								welcome.assignedPeerId = ci.assignedPeerId;
								welcome.runtimePeerCount = kRuntimePeerCount;

								send(ci.socket,
									reinterpret_cast<const char*>(&welcome),
									static_cast<int>(sizeof(welcome)),
									0);

								std::cout << "Assigned peerId " << ci.assignedPeerId
									<< " to " << ci.peerInstanceId << "\n";

								continue;
							}

							if (ci.rxBuffer.size() < sizeof(SyncPacket))
								break;

							SyncPacket packet{};
							std::memcpy(&packet, ci.rxBuffer.data(), sizeof(SyncPacket));
							ci.rxBuffer.erase(ci.rxBuffer.begin(), ci.rxBuffer.begin() + static_cast<std::ptrdiff_t>(sizeof(SyncPacket)));

							// Host enforces true source id from socket assignment
							packet.sourcePeerId = ci.assignedPeerId;

							if (m_networkData) {
								std::lock_guard<std::mutex> lock(m_networkData->incomingMutex);
								m_networkData->incomingPackets.push_back(packet);
							}

							// Relay to all other connected clients (star topology)
							for (auto& target : clientSockets) {
								if (!target.handshakeDone || target.socket == INVALID_SOCKET) continue;
								if (target.socket == ci.socket) continue;

								int sent = send(target.socket, reinterpret_cast<const char*>(&packet), sizeof(SyncPacket), 0);
								if (sent == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK) {
									closesocket(target.socket);
									target.socket = INVALID_SOCKET;
								}
							}
						}

						if (!clientOk)
							break;

					} while (bytes > 0);

					if (bytes == 0 || (bytes == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK) || !clientOk) {
						closesocket(ci.socket);
						it = clientSockets.erase(it);
					}
					else {
						++it;
					}
				}
			}

			// Client path: send outgoing to host + receive relayed state + receive welcome
			if (m_networkRole == NetworkRole::Client && m_peerConnected && m_tcpClient) {
				SOCKET hostSock = m_tcpClient->native_handle();
				bool ok = true;

				for (const auto& packet : outgoing) {
					int sent = send(hostSock, reinterpret_cast<const char*>(&packet), sizeof(SyncPacket), 0);
					if (sent == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK) {
						ok = false;
						break;
					}
				}

				char recvBuf[512]{};
				int bytes = 0;
				do {
					bytes = recv(hostSock, recvBuf, sizeof(recvBuf), 0);

					if (bytes > 0) {
						const uint8_t* begin = reinterpret_cast<const uint8_t*>(recvBuf);
						hostRxBuffer.insert(hostRxBuffer.end(), begin, begin + bytes);
					}

					for (;;)
					{
						// First packet expected is welcome
						if (!clientGotWelcome)
						{
							if (hostRxBuffer.size() < sizeof(WelcomeMsg))
								break;

							WelcomeMsg welcome{};
							std::memcpy(&welcome, hostRxBuffer.data(), sizeof(WelcomeMsg));
							hostRxBuffer.erase(
								hostRxBuffer.begin(),
								hostRxBuffer.begin() + static_cast<std::ptrdiff_t>(sizeof(WelcomeMsg)));

							if (welcome.magic != kWelcomeMagic) {
								ok = false;
								break;
							}

							m_localPeerId.store(welcome.assignedPeerId);
							m_runtimePeerCount.store(welcome.runtimePeerCount);
							clientGotWelcome = true;
							continue;
						}

						// Then parse as many SyncPacket entries as available
						if (hostRxBuffer.size() < sizeof(SyncPacket))
							break;

						SyncPacket packet{};
						std::memcpy(&packet, hostRxBuffer.data(), sizeof(SyncPacket));
						hostRxBuffer.erase(
							hostRxBuffer.begin(),
							hostRxBuffer.begin() + static_cast<std::ptrdiff_t>(sizeof(SyncPacket)));

						if (m_networkData) {
							std::lock_guard<std::mutex> lock(m_networkData->incomingMutex);
							m_networkData->incomingPackets.push_back(packet);
						}
					}

					if (!ok)
						break;

				} while (bytes > 0);

				if (!ok || bytes == 0 || (bytes == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)) {
					std::cout << "Disconnected from host.\n";
					m_peerConnected = false;
					m_tcpClient.reset();
					m_networkRole = NetworkRole::Host;
					m_localPeerId.store(0);
				}
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}

		if (udpSocket != INVALID_SOCKET) closesocket(udpSocket);
		for (ClientInfo& ci : clientSockets) {
			if (ci.socket != INVALID_SOCKET) closesocket(ci.socket);
		}
		});

	std::cout << "Network server started on port " << random << std::endl;
	return bindAddr;
}
FlatBufferScene::~FlatBufferScene()
{
	// Stop network thread first
	if (m_vulkanRHI && m_vulkanRHI->GetActiveCamera() == m_activeCamera)
	{
		m_vulkanRHI->SetActiveCamera(nullptr);
	}

	m_networkRunning = false;
	if (m_networkThread.joinable())
	{
		m_networkThread.join();
	}

	m_systemManager.StopThreads();

	if (m_vulkanRHI)
	{
		m_vulkanRHI->WaitIdle();

		for (auto& entity : m_entities)
			entity.Destroy();

		m_systemManager.Shutdown();
	}
}

void FlatBufferScene::Destroy()
{
	if (m_vulkanRHI && m_vulkanRHI->GetActiveCamera() == m_activeCamera)
	{
		m_vulkanRHI->SetActiveCamera(nullptr);
	}

	// Stop network thread
	m_networkRunning = false;
	if (m_networkThread.joinable())
	{
		m_networkThread.join();
	}

	m_systemManager.StopThreads();

	if (m_vulkanRHI)
	{
		m_vulkanRHI->WaitIdle();

		for (auto& entity : m_entities)
			entity.Destroy();

		m_systemManager.Shutdown();
	}
}

void FlatBufferScene::Start(float deltaTime)
{
	// Critical for scene-swap stability: bind valid camera before any graphics-thread Draw()/EndFrame()
	if (m_vulkanRHI)
	{
		if (!m_activeCamera && !m_cameras.empty())
			m_activeCamera = &m_cameras[0];
		m_vulkanRHI->SetActiveCamera(m_activeCamera);
	}

	m_systemManager.StartSimulationThread(m_entities, m_sceneMutex, m_paused, m_physicsHz);
	m_systemManager.StartGraphicsThread([this]() { Draw(); }, m_graphicsHz);
}

void FlatBufferScene::Stop()
{
	m_systemManager.StopThreads();
}
void FlatBufferScene::FixedUpdate()
{
	// Not used since we have a separate simulation thread with its own timing
}

void FlatBufferScene::Update(float deltaTime)
{
	m_window->PollEvents();
	m_deltaTime = deltaTime;

	if (m_paused.load())
		deltaTime = 0.0f;

	UpdateSpawnerRuntime(deltaTime);

	if (!m_systemManager.IsSimulationThreadRunning())
	{
		m_systemManager.Update(m_entities, deltaTime);
	}
}

void FlatBufferScene::Draw()
{
	if (m_systemManager.IsGraphicsThreadRunning() &&
		std::this_thread::get_id() != m_systemManager.GetGraphicsThreadId())
	{
		return;
	}

	static auto lastFrameTime = std::chrono::steady_clock::now();
	const auto now = std::chrono::steady_clock::now();
	const float renderDelta = std::chrono::duration<float>(now - lastFrameTime).count();
	lastFrameTime = now;
	m_renderDeltaTime.store(renderDelta);

	// Ensure Graphics Thread safely reads the updated physical state
	std::lock_guard<std::mutex> lock(m_sceneMutex);

	m_vulkanRHI->BeginFrame();

	VkCommandBuffer cmd = m_vulkanRHI->GetCurrentCommandBuffer();
	if (cmd != VK_NULL_HANDLE)
	{
		m_systemManager.Render(cmd, m_entities);

		if (m_gui)
		{
			m_gui->NewFrame(*m_window);

			static bool show_demo_window = false;
			static bool show_about = true;

			auto applyTransform = [](ComponentTransform* transform, const glm::vec3& pos, const glm::vec3& rot, const glm::vec3& scale)
			{
				if (!transform) return;
				Transform* write0 = transform->WriteBuffer();
				write0->Position = pos;
				write0->Rotation = rot;
				write0->Scale = scale;

				transform->SwapBuffers();

				Transform* write1 = transform->WriteBuffer();
				write1->Position = pos;
				write1->Rotation = rot;
				write1->Scale = scale;
			};

			// Global UI to switch between scenes
			if (ImGui::BeginMainMenuBar())
			{
				if (ImGui::BeginMenu("Scenes"))
				{
					if (ImGui::MenuItem("Animation Demo"))
						SceneManager::Instance().RequestReplaceScene(std::make_unique<AnimationScene>(*m_window, m_vulkanRHI, m_gui));

					if (ImGui::MenuItem("Ball Drop"))
						SceneManager::Instance().RequestReplaceScene(std::make_unique<BallDropScene>(*m_window, m_vulkanRHI, m_gui));

					if (ImGui::MenuItem("Panning"))
						SceneManager::Instance().RequestReplaceScene(std::make_unique<PanningScene>(*m_window, m_vulkanRHI, m_gui));

					if (ImGui::MenuItem("Template"))
						SceneManager::Instance().RequestReplaceScene(std::make_unique<TemplateScene>(*m_window, m_vulkanRHI, m_gui));

					if (ImGui::MenuItem("Reload FlatBuffer Scene"))
						SceneManager::Instance().RequestReplaceScene(std::make_unique<FlatBufferScene>(*m_window, m_vulkanRHI, m_gui));

					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("Entities"))
				{
					for (int i = 0; i < static_cast<int>(m_entities.size()); ++i)
					{
						auto& entity = m_entities[i];
						auto* transform = entity.GetComponent<ComponentTransform>(EComponentType::Component_Transform);
						auto* netComp = entity.GetComponent<ComponentNetwork>(EComponentType::Component_Network);

						if (!transform || !netComp || !netComp->IsOwnedByMe(m_localPeerId))
							continue;

						const std::string label = "Entity " + std::to_string(i) + " (NetId " + std::to_string(netComp->networkId) + ")";
						const bool isSelected = (m_selectedEntityIndex == i);
						if (ImGui::MenuItem(label.c_str(), nullptr, isSelected))
						{
							m_selectedEntityIndex = i;
						}
					}
					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("Cameras"))
				{
					// List available cameras and allow selecting an active one
					for (size_t i = 0; i < m_cameras.size(); ++i)
					{
						std::string label = "Camera " + std::to_string(i);
						bool isActive = (m_activeCamera == &m_cameras[i]);
						if (ImGui::MenuItem(label.c_str(), nullptr, isActive))
						{
							m_activeCamera = &m_cameras[i];
							if (m_vulkanRHI) m_vulkanRHI->SetActiveCamera(m_activeCamera);
						}
					}

					if (ImGui::MenuItem("Add Camera"))
					{
						float aspect = 16.0f / 9.0f;
						if (m_vulkanRHI)
						{
							auto extent = m_vulkanRHI->GetSwapchainExtent();
							if (extent.height != 0)
								aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
						}
						m_cameras.emplace_back(60.0f, aspect, 0.1f, 1000.0f);
						m_activeCamera = &m_cameras.back();
						if (m_vulkanRHI) m_vulkanRHI->SetActiveCamera(m_activeCamera);
					}

					if (!m_cameras.empty())
					{
						ImGui::Separator();
						ImGui::Text("Active Camera");

						// Show editable position / rotation / projection for the active camera
						if (m_activeCamera)
						{
							// Position
							glm::vec3 pos = m_activeCamera->GetPosition();
							float posArr[3] = { pos.x, pos.y, pos.z };
							if (ImGui::DragFloat3("Position", posArr, 0.1f))
							{
								m_activeCamera->SetPosition(glm::vec3(posArr[0], posArr[1], posArr[2]));
								m_activeCamera->MarkDirty();
							}

							// Rotation (pitch,x) (yaw,y) (roll,z)
							glm::vec3 rot = m_activeCamera->GetRotation();
							float rotArr[3] = { rot.x, rot.y, rot.z };
							if (ImGui::DragFloat3("Rotation (deg)", rotArr, 1.0f))
							{
								m_activeCamera->SetRotation(glm::vec3(rotArr[0], rotArr[1], rotArr[2]));
								m_activeCamera->MarkDirty();
							}

							// --- UI camera section: replace the previous 'Projection parameters for perspective cameras' block ---
							// Show/edit projection type and appropriate parameters
							{
								// Projection type selector
								int projIdx = static_cast<int>(m_activeCamera->GetProjectionType());
								const char* projItems = "Perspective\0Orthographic\0";
								if (ImGui::Combo("Projection", &projIdx, projItems))
								{
									if (projIdx == 0 && m_activeCamera->GetProjectionType() != Camera::ProjectionType::Perspective)
									{
										// switch to perspective: keep reasonable defaults
										m_activeCamera->SetPerspective(m_activeCamera->GetFovDeg(), m_activeCamera->GetAspect(), m_activeCamera->GetNear(), m_activeCamera->GetFar());
										m_activeCamera->MarkDirty();
									}
									else if (projIdx == 1 && m_activeCamera->GetProjectionType() != Camera::ProjectionType::Orthographic)
									{
										// switch to orthographic: use existing ortho size if present, otherwise default
										float size = m_activeCamera->GetOrthoSize();
										m_activeCamera->SetOrthographic(size, m_activeCamera->GetAspect(), m_activeCamera->GetNear(), m_activeCamera->GetFar());
										m_activeCamera->MarkDirty();
									}
								}

								if (m_activeCamera->GetProjectionType() == Camera::ProjectionType::Perspective)
								{
									float fov = m_activeCamera->GetFovDeg();
									if (ImGui::DragFloat("FOV (deg)", &fov, 0.25f, 1.0f, 179.0f))
									{
										m_activeCamera->SetPerspective(fov, m_activeCamera->GetAspect(), m_activeCamera->GetNear(), m_activeCamera->GetFar());
										m_activeCamera->MarkDirty();
									}
								}
								else // Orthographic
								{
									float size = m_activeCamera->GetOrthoSize();
									if (ImGui::DragFloat("Ortho Size", &size, 0.1f, 0.01f, 100000.0f))
									{
										m_activeCamera->SetOrthographic(size, m_activeCamera->GetAspect(), m_activeCamera->GetNear(), m_activeCamera->GetFar());
										m_activeCamera->MarkDirty();
									}
								}

								float nearVal = m_activeCamera->GetNear();
								float farVal = m_activeCamera->GetFar();
								if (ImGui::DragFloat("Near", &nearVal, 0.01f, 0.001f, farVal - 0.001f))
								{
									m_activeCamera->SetNearFar(nearVal, farVal);
									m_activeCamera->MarkDirty();
								}
								if (ImGui::DragFloat("Far", &farVal, 1.0f, nearVal + 0.1f, 100000.0f))
								{
									m_activeCamera->SetNearFar(nearVal, farVal);
									m_activeCamera->MarkDirty();
								}
							}
							// --- end UI camera section ---

							ImGui::Spacing();
							if (ImGui::Button("Save Cameras to Scene"))
							{
								SerializeState();
							}
							ImGui::SameLine();
							if (ImGui::Button("Remove Active"))
							{
								// remove selected camera; reset active to first if possible
								ptrdiff_t idx = m_activeCamera ? (m_activeCamera - &m_cameras[0]) : -1;
								if (idx >= 0 && idx < (ptrdiff_t)m_cameras.size())
								{
									m_cameras.erase(m_cameras.begin() + idx);
									if (!m_cameras.empty())
										m_activeCamera = &m_cameras[0];
									else
									{
										// ensure at least one camera
										float aspect = 16.0f / 9.0f;
										if (m_vulkanRHI)
										{
											auto extent = m_vulkanRHI->GetSwapchainExtent();
											if (extent.height != 0)
												aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
										}
										m_cameras.emplace_back(60.0f, aspect, 0.1f, 1000.0f);
										m_activeCamera = &m_cameras[0];
									}
									if (m_vulkanRHI) m_vulkanRHI->SetActiveCamera(m_activeCamera);
								}
							}
						}
					}

					ImGui::EndMenu();
				}

				ImGui::EndMainMenuBar();
			}

			if (m_selectedEntityIndex >= 0 && m_selectedEntityIndex < static_cast<int>(m_entities.size()))
			{
				auto& entity = m_entities[m_selectedEntityIndex];
				auto* transform = entity.GetComponent<ComponentTransform>(EComponentType::Component_Transform);
				auto* netComp = entity.GetComponent<ComponentNetwork>(EComponentType::Component_Network);

				if (transform && netComp && netComp->IsOwnedByMe(m_localPeerId))
				{
					ImGui::Begin("Entity Transform");
					glm::vec3 pos = transform->Position();
					glm::vec3 rot = transform->Rotation();
					glm::vec3 scale = transform->Scale();

					float posArr[3] = { pos.x, pos.y, pos.z };
					float rotArr[3] = { rot.x, rot.y, rot.z };
					float scaleArr[3] = { scale.x, scale.y, scale.z };

					bool changed = false;
					changed |= ImGui::DragFloat3("Position", posArr, 0.1f);
					changed |= ImGui::DragFloat3("Rotation (deg)", rotArr, 1.0f);
					changed |= ImGui::DragFloat3("Scale", scaleArr, 0.1f, 0.001f, 1000.0f);

					if (changed)
					{
						applyTransform(transform,
							glm::vec3(posArr[0], posArr[1], posArr[2]),
							glm::vec3(rotArr[0], rotArr[1], rotArr[2]),
							glm::vec3(scaleArr[0], scaleArr[1], scaleArr[2]));
					}
					ImGui::End();
				}
			}

			if (m_selectedEntityIndex >= 0 && m_selectedEntityIndex < static_cast<int>(m_entities.size()))
			{
				auto& selected = m_entities[m_selectedEntityIndex];
				auto* net = selected.GetComponent<ComponentNetwork>(EComponentType::Component_Network);
				auto* col = selected.GetComponent<ComponentCollision>(EComponentType::Component_Collision);

				if (net)
				{
					ImGui::Separator();
					ImGui::Text("Selected Entity: %d", m_selectedEntityIndex);
					ImGui::Text("OwnerId: %u", net->ownerId);
					ImGui::Text("LocalSimulates: %s", net->IsOwnedByMe(m_localPeerId) && net->IsSimulated() ? "YES" : "NO");
					ImGui::Text("Behaviour: %s",
						Simulation::EnumNameBehaviour(static_cast<Simulation::Behaviour>(net->behaviourType)));
					ImGui::Text("Shape: %s",
						Simulation::EnumNameShape(static_cast<Simulation::Shape>(net->shapeType)));
					ImGui::Text("CollisionType: %s",
						static_cast<Simulation::CollisionType>(net->collisionType) == Simulation::CollisionType_CONTAINER
						? "CONTAINER" : "SOLID");
					ImGui::Text("Material: %s", net->materialName.c_str());

					if (col)
					{
						ImGui::Text("CollisionRole(runtime): %s",
							col->GetCollisionRole() == CollisionRole::Container ? "Container" : "Solid");
					}
				}
			}

			{
				ImGui::Begin("Spawners");
				bool rebuildSpawners = false;
				int removeIndex = -1;

				if (ImGui::Button("Add Spawner"))
				{
					SpawnerData sd;
					sd.name = "Spawner_" + std::to_string(m_spawners.size());
					sd.spawnType = static_cast<uint8_t>(Simulation::SpawnType_SingleBurstSpawn);
					sd.locationType = static_cast<uint8_t>(Simulation::SpawnLocation_FixedLocation);
					sd.spawnerType = static_cast<uint8_t>(Simulation::SpawnerType_SphereSpawner);
					sd.owner = static_cast<int>(Simulation::SpawnerOwnerType_ONE);
					m_spawners.push_back(sd);
					rebuildSpawners = true;
				}

				ImGui::SameLine();
				if (ImGui::Button("Save Spawners"))
				{
					SerializeState();
				}

				for (int i = 0; i < static_cast<int>(m_spawners.size()); ++i)
				{
					SpawnerData& sp = m_spawners[i];
					const std::string nodeLabel = (sp.name.empty() ? ("Spawner_" + std::to_string(i)) : sp.name) + "##" + std::to_string(i);

					if (!ImGui::TreeNode(nodeLabel.c_str()))
						continue;

					char nameBuf[128]{};
					std::snprintf(nameBuf, sizeof(nameBuf), "%s", sp.name.c_str());
					if (ImGui::InputText(("Name##" + std::to_string(i)).c_str(), nameBuf, sizeof(nameBuf)))
					{
						sp.name = nameBuf;
						rebuildSpawners = true;
					}

					rebuildSpawners |= ImGui::DragFloat(("Start Time##" + std::to_string(i)).c_str(), &sp.startTime, 0.01f, 0.0f, 3600.0f);

					int spawnIdx = (sp.spawnType == static_cast<uint8_t>(Simulation::SpawnType_RepeatingSpawn)) ? 1 : 0;
					if (ImGui::Combo(("Spawn Type##" + std::to_string(i)).c_str(), &spawnIdx, "SingleBurst\0Repeating\0"))
					{
						sp.spawnType = (spawnIdx == 0)
							? static_cast<uint8_t>(Simulation::SpawnType_SingleBurstSpawn)
							: static_cast<uint8_t>(Simulation::SpawnType_RepeatingSpawn);
						rebuildSpawners = true;
					}

					int locIdx = 0;
					if (sp.locationType == static_cast<uint8_t>(Simulation::SpawnLocation_RandomBox)) locIdx = 1;
					if (sp.locationType == static_cast<uint8_t>(Simulation::SpawnLocation_RandomSphere)) locIdx = 2;
					if (ImGui::Combo(("Location##" + std::to_string(i)).c_str(), &locIdx, "Fixed\0Box\0Sphere\0"))
					{
						sp.locationType = (locIdx == 0) ? static_cast<uint8_t>(Simulation::SpawnLocation_FixedLocation)
							: (locIdx == 1) ? static_cast<uint8_t>(Simulation::SpawnLocation_RandomBox)
							: static_cast<uint8_t>(Simulation::SpawnLocation_RandomSphere);
						rebuildSpawners = true;
					}

					int shapeIdx = 0;
					if (sp.spawnerType == static_cast<uint8_t>(Simulation::SpawnerType_CylinderSpawner)) shapeIdx = 1;
					if (sp.spawnerType == static_cast<uint8_t>(Simulation::SpawnerType_CapsuleSpawner)) shapeIdx = 2;
					if (sp.spawnerType == static_cast<uint8_t>(Simulation::SpawnerType_CuboidSpawner)) shapeIdx = 3;
					if (ImGui::Combo(("Shape##" + std::to_string(i)).c_str(), &shapeIdx, "Sphere\0Cylinder\0Capsule\0Cuboid\0"))
					{
						sp.spawnerType = (shapeIdx == 0) ? static_cast<uint8_t>(Simulation::SpawnerType_SphereSpawner)
							: (shapeIdx == 1) ? static_cast<uint8_t>(Simulation::SpawnerType_CylinderSpawner)
							: (shapeIdx == 2) ? static_cast<uint8_t>(Simulation::SpawnerType_CapsuleSpawner)
							: static_cast<uint8_t>(Simulation::SpawnerType_CuboidSpawner);
						rebuildSpawners = true;
					}

					int ownerIdx = std::clamp(sp.owner, 0, 4);
					if (ImGui::Combo(("Owner##" + std::to_string(i)).c_str(), &ownerIdx, "ONE\0TWO\0THREE\0FOUR\0SEQUENTIAL\0"))
					{
						sp.owner = ownerIdx;
						rebuildSpawners = true;
					}

					char matBuf[128]{};
					std::snprintf(matBuf, sizeof(matBuf), "%s", sp.material.c_str());
					if (ImGui::InputText(("Material##" + std::to_string(i)).c_str(), matBuf, sizeof(matBuf)))
					{
						sp.material = matBuf;
						rebuildSpawners = true;
					}

					rebuildSpawners |= ImGui::DragFloat3(("Linear Vel Min##" + std::to_string(i)).c_str(), &sp.linearVelMin.x, 0.05f);
					rebuildSpawners |= ImGui::DragFloat3(("Linear Vel Max##" + std::to_string(i)).c_str(), &sp.linearVelMax.x, 0.05f);
					rebuildSpawners |= ImGui::DragFloat3(("Angular Vel Min##" + std::to_string(i)).c_str(), &sp.angularVelMin.x, 0.05f);
					rebuildSpawners |= ImGui::DragFloat3(("Angular Vel Max##" + std::to_string(i)).c_str(), &sp.angularVelMax.x, 0.05f);

					if (sp.locationType == static_cast<uint8_t>(Simulation::SpawnLocation_FixedLocation))
					{
						rebuildSpawners |= ImGui::DragFloat3(("Fixed Pos##" + std::to_string(i)).c_str(), &sp.fixedPosition.x, 0.05f);
					}
					else if (sp.locationType == static_cast<uint8_t>(Simulation::SpawnLocation_RandomBox))
					{
						rebuildSpawners |= ImGui::DragFloat3(("Box Min##" + std::to_string(i)).c_str(), &sp.boxMin.x, 0.05f);
						rebuildSpawners |= ImGui::DragFloat3(("Box Max##" + std::to_string(i)).c_str(), &sp.boxMax.x, 0.05f);
					}
					else
					{
						rebuildSpawners |= ImGui::DragFloat3(("Sphere Center##" + std::to_string(i)).c_str(), &sp.sphereCenter.x, 0.05f);
						rebuildSpawners |= ImGui::DragFloat(("Sphere Radius##" + std::to_string(i)).c_str(), &sp.sphereRadius, 0.05f, 0.01f, 1000.0f);
					}

					if (sp.spawnType == static_cast<uint8_t>(Simulation::SpawnType_SingleBurstSpawn))
					{
						int burst = static_cast<int>(sp.singleBurstCount);
						if (ImGui::DragInt(("Burst Count##" + std::to_string(i)).c_str(), &burst, 1.0f, 1, 100000))
						{
							sp.singleBurstCount = static_cast<uint32_t>(std::max(1, burst));
							rebuildSpawners = true;
						}
					}
					else
					{
						rebuildSpawners |= ImGui::DragFloat(("Repeat Interval##" + std::to_string(i)).c_str(), &sp.repeatingInterval, 0.01f, 0.01f, 1000.0f);
						int maxCount = static_cast<int>(sp.repeatingMaxCount);
						if (ImGui::DragInt(("Repeat Max Count##" + std::to_string(i)).c_str(), &maxCount, 1.0f, 1, 100000))
						{
							sp.repeatingMaxCount = static_cast<uint32_t>(std::max(1, maxCount));
							rebuildSpawners = true;
						}
					}

					rebuildSpawners |= ImGui::DragFloat(("Radius Min##" + std::to_string(i)).c_str(), &sp.radiusMin, 0.01f, 0.01f, 1000.0f);
					rebuildSpawners |= ImGui::DragFloat(("Radius Max##" + std::to_string(i)).c_str(), &sp.radiusMax, 0.01f, 0.01f, 1000.0f);
					rebuildSpawners |= ImGui::DragFloat(("Height Min##" + std::to_string(i)).c_str(), &sp.heightMin, 0.01f, 0.01f, 1000.0f);
					rebuildSpawners |= ImGui::DragFloat(("Height Max##" + std::to_string(i)).c_str(), &sp.heightMax, 0.01f, 0.01f, 1000.0f);
					rebuildSpawners |= ImGui::DragFloat3(("Size Min##" + std::to_string(i)).c_str(), &sp.sizeMin.x, 0.01f);
					rebuildSpawners |= ImGui::DragFloat3(("Size Max##" + std::to_string(i)).c_str(), &sp.sizeMax.x, 0.01f);

					if (ImGui::Button(("Remove##" + std::to_string(i)).c_str()))
					{
						removeIndex = i;
					}

					ImGui::TreePop();
				}

				if (removeIndex >= 0)
				{
					m_spawners.erase(m_spawners.begin() + removeIndex);
					rebuildSpawners = true;
				}

				if (rebuildSpawners)
				{
					RebuildSpawnerRuntime();
				}

				ImGui::End();
			}

			if (show_demo_window)
				ImGui::ShowDemoWindow(&show_demo_window);

			if (show_about)
			{
				ImGui::Begin("Debug Diagnostics");
				ImGui::Text("File: FlatBuffer Scene Loaded");
				const float fpsDelta = m_renderDeltaTime.load();
				ImGui::Text("FPS: %.1f", fpsDelta > 0.0f ? (1.0f / fpsDelta) : 0.0f);
				ImGui::Text("Object Count: %d", m_entities.size());
				ImGui::DragInt("Physics iterations per second: %d", reinterpret_cast<int*>(&m_physicsHz));
				ImGui::DragInt("Render frames per second: %d", reinterpret_cast<int*>(&m_graphicsHz));
				if (m_physicsHz <= 0) m_physicsHz = 1;
				if (m_graphicsHz <= 0) m_graphicsHz = 1;
				
				if (ImGui::Button("Start/Stop Simulation"))
					m_paused = !m_paused;
				if(ImGui::Button("Save State")) SerializeState();
				ImGui::End();
			}

			m_gui->Render(cmd);
		}
		m_vulkanRHI->EndFrame();
		m_vulkanRHI->Present();
	}
}

void FlatBufferScene::HandleInput(float deltaTime)
{
	if (m_inputHandler.isKeyPressed(GLFW_KEY_ESCAPE))
		glfwSetWindowShouldClose(m_window->getGLFWwindow(), true);

	if (!m_activeCamera)
		return;

	float cameraMoveSpeed = 5.0f;
	if (m_inputHandler.isKeyHeld(GLFW_KEY_LEFT_SHIFT)) cameraMoveSpeed *= 2.0f;

	if (m_inputHandler.isKeyHeld(GLFW_KEY_W)) m_activeCamera->Translate(m_activeCamera->Forward() * cameraMoveSpeed * deltaTime);
	if (m_inputHandler.isKeyHeld(GLFW_KEY_S)) m_activeCamera->Translate(-m_activeCamera->Forward() * cameraMoveSpeed * deltaTime);
	if (m_inputHandler.isKeyHeld(GLFW_KEY_A)) m_activeCamera->Translate(-m_activeCamera->Right() * cameraMoveSpeed * deltaTime);
	if (m_inputHandler.isKeyHeld(GLFW_KEY_D)) m_activeCamera->Translate(m_activeCamera->Right() * cameraMoveSpeed * deltaTime);
	if (m_inputHandler.isKeyHeld(GLFW_KEY_LEFT_CONTROL)) m_activeCamera->Translate(-m_activeCamera->Up() * cameraMoveSpeed * deltaTime);
	if (m_inputHandler.isKeyHeld(GLFW_KEY_SPACE)) m_activeCamera->Translate(m_activeCamera->Up() * cameraMoveSpeed * deltaTime);

	if (m_inputHandler.isKeyHeld(GLFW_KEY_J)) m_activeCamera->Rotate(glm::vec3(0.0f, -90.0f * deltaTime, 0.0f));
	if (m_inputHandler.isKeyHeld(GLFW_KEY_L)) m_activeCamera->Rotate(glm::vec3(0.0f, 90.0f * deltaTime, 0.0f));
	if (m_inputHandler.isKeyHeld(GLFW_KEY_K)) m_activeCamera->Rotate(glm::vec3(-90.0f * deltaTime, 0.0f, 0.0f));
	if (m_inputHandler.isKeyHeld(GLFW_KEY_I)) m_activeCamera->Rotate(glm::vec3(90.0f * deltaTime, 0.0f, 0.0f));

	m_vulkanRHI->SetActiveCamera(m_activeCamera);
}

void FlatBufferScene::SerializeState()
{
	LOG_DEBUG("Serializing scene state to flatbuffer...");

	flatbuffers::FlatBufferBuilder builder(4096);

	// Scene metadata
	auto sceneName = builder.CreateString(m_sceneName.empty() ? "Level1" : m_sceneName.c_str());
	auto sceneDesc = builder.CreateString(m_sceneDescription.empty() ? "Saved scene containing cube entities and camera state." : m_sceneDescription.c_str());

	// Cameras - save current active camera (m_camera)
	{
		// Build camera flatbuffer list from all cameras
		std::vector<flatbuffers::Offset<Simulation::Camera>> cams;
		for (size_t i = 0; i < m_cameras.size(); ++i)
		{
			const Camera& cam = m_cameras[i];
			glm::vec3 camPos = cam.GetPosition();
			glm::vec3 camRot = cam.GetRotation(); // x = pitch, y = yaw, z = roll

			Simulation::Vec3 camPosStruct(camPos.x, camPos.y, camPos.z);
			Simulation::RotationEuler camOrientStruct(camRot.y /*yaw*/, camRot.x /*pitch*/, camRot.z /*roll*/);
			Simulation::Vec3 camScaleStruct(1.0f, 1.0f, 1.0f);
			Simulation::Transform camTransform(camPosStruct, camOrientStruct, camScaleStruct);

			auto camName = builder.CreateString(("Camera" + std::to_string(i)).c_str());

			// Choose projection serialization based on Camera::ProjectionType
			flatbuffers::Offset<void> projUnion = 0;
			Simulation::CameraType camTypeEnum = Simulation::CameraType_PerspectiveCamera;

			if (cam.GetProjectionType() == Camera::ProjectionType::Perspective)
			{
				camTypeEnum = Simulation::CameraType_PerspectiveCamera;
				auto perspectiveOffset = Simulation::CreatePerspectiveCamera(
					builder,
					cam.GetFovDeg(),
					cam.GetNear(),
					cam.GetFar()
				);
				projUnion = perspectiveOffset.Union();
			}
			else // Orthographic
			{
				camTypeEnum = Simulation::CameraType_OrthographicCamera;
				auto orthoOffset = Simulation::CreateOrthographicCamera(
					builder,
					cam.GetOrthoSize(),
					cam.GetNear(),
					cam.GetFar()
				);
				projUnion = orthoOffset.Union();
			}

			auto camOffset = Simulation::CreateCamera(
				builder,
				camName,
				&camTransform,
				camTypeEnum,
				projUnion
			);

			cams.push_back(camOffset);
		}

		auto camerasVec = builder.CreateVector(cams);

		// Objects vector will be created below; temporarily collect objects first.
		// Build objects
		std::vector<flatbuffers::Offset<Simulation::Object>> objectsVecOffsets;
		for (auto &e : m_entities)
		{
			// Save only entities that have a transform + geometry (likely visible objects)
			if (!e.HasComponent(EComponentType::Component_Geometry) || !e.HasComponent(EComponentType::Component_Transform))
				continue;

			auto* xf = e.GetComponent<ComponentTransform>(EComponentType::Component_Transform);
			if (!xf) continue;

			// Read transform from component (read buffer values)
			glm::vec3 pos = xf->Position();
			glm::vec3 rot = xf->Rotation(); // pitch(x), yaw(y), roll(z)
			glm::vec3 scale = xf->Scale();

			Simulation::Vec3 posStruct(pos.x, pos.y, pos.z);
			Simulation::RotationEuler orientStruct(rot.y /*yaw*/, rot.x /*pitch*/, rot.z /*roll*/);
			Simulation::Vec3 scaleStruct(scale.x, scale.y, scale.z);
			Simulation::Transform objTransform(posStruct, orientStruct, scaleStruct);

			// Represent geometry as a cuboid with 'size' equal to entity scale.
			auto cuboidOffset = Simulation::CreateCuboid(builder, &scaleStruct);

			// Name (optional)
			auto name = builder.CreateString("cube");

			Simulation::Behaviour behaviourType = Simulation::Behaviour_StaticObject;
			flatbuffers::Offset<void> behaviourUnion = 0;

			auto* netComp = e.GetComponent<ComponentNetwork>(EComponentType::Component_Network);
			if (netComp)
			{
				if (netComp->type == ObjectType::Simulated)
				{
					behaviourType = Simulation::Behaviour_SimulatedObject;

					Simulation::Vec3 lv(0.0f, 0.0f, 0.0f);
					Simulation::Vec3 av(0.0f, 0.0f, 0.0f);

					if (auto* vel = e.GetComponent<ComponentVelocity>(EComponentType::Component_Velocity))
					{
						const glm::vec3 lvv = vel->GetPositionVelocity();
						const glm::vec3 avv = vel->GetRotationalVelocity();
						lv = Simulation::Vec3(lvv.x, lvv.y, lvv.z);
						av = Simulation::Vec3(avv.x, avv.y, avv.z);
					}

					Simulation::PhysicsState initState(lv, av);
					auto simObj = Simulation::CreateSimulatedObject(builder, &initState, PeerIdToOwnerType(netComp->ownerId));
					behaviourUnion = simObj.Union();
				}
				else if (netComp->type == ObjectType::Animated)
				{
					behaviourType = Simulation::Behaviour_AnimatedObject;
					auto animObj = Simulation::CreateAnimatedObject(builder, 0, 0.0f, Simulation::EasingType_LINEAR, Simulation::PathMode_STOP);
					behaviourUnion = animObj.Union();
				}
				else
				{
					behaviourType = Simulation::Behaviour_StaticObject;
					auto staticObj = Simulation::CreateStaticObject(builder);
					behaviourUnion = staticObj.Union();
				}
			}
			else
			{
				auto staticObj = Simulation::CreateStaticObject(builder);
				behaviourUnion = staticObj.Union();
			}

			Simulation::CollisionType fbCollisionType = Simulation::CollisionType_SOLID;
			if (auto* collision = e.GetComponent<ComponentCollision>(EComponentType::Component_Collision))
			{
				fbCollisionType = (collision->GetCollisionRole() == CollisionRole::Container)
					? Simulation::CollisionType_CONTAINER
					: Simulation::CollisionType_SOLID;
			}

			auto objOffset = Simulation::CreateObject(
				builder,
				name,
				&objTransform,
				0,
				Simulation::Shape_Cuboid,
				cuboidOffset.Union(),
				behaviourType,
				behaviourUnion,
				fbCollisionType
			);

			objectsVecOffsets.push_back(objOffset);
		}

		auto objectsVec = builder.CreateVector(objectsVecOffsets);

		// Materials vector (if any)
		flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<Simulation::Material>>> materialsVec = 0;
		if (!m_materials.empty())
		{
			std::vector<flatbuffers::Offset<Simulation::Material>> mats;
			mats.reserve(m_materials.size());
			for (auto &m : m_materials)
			{
				auto nameOff = builder.CreateString(m.name.c_str());
				mats.push_back(Simulation::CreateMaterial(builder, nameOff, m.density));
			}
			materialsVec = builder.CreateVector(mats);
		}

		// Material interactions vector (if any)
		flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<Simulation::MaterialInteraction>>> interactionsVec = 0;
		if (!m_materialInteractions.empty())
		{
			std::vector<flatbuffers::Offset<Simulation::MaterialInteraction>> inters;
			inters.reserve(m_materialInteractions.size());
			for (auto &mi : m_materialInteractions)
			{
				auto a = builder.CreateString(mi.materialA.c_str());
				auto b = builder.CreateString(mi.materialB.c_str());
				inters.push_back(Simulation::CreateMaterialInteraction(builder, a, b, mi.restitution, mi.staticFriction, mi.dynamicFriction));
			}
			interactionsVec = builder.CreateVector(inters);
		}

		// Finish the scene with cameras and objects filled.
		auto sceneOffset = Simulation::CreateScene(
			builder,
			sceneName,
			sceneDesc,
			m_gravityOn,     // gravity_on
			camerasVec,
			objectsVec,
			0,              // spawners_type (not serializing spawners in full yet)
			0,              // spawners
			materialsVec,
			interactionsVec
		);

		builder.Finish(sceneOffset);

		// Ensure directory exists and write buffer to disk
		std::filesystem::create_directories("scenes");
		std::ofstream outfile("scenes/Level1.bin", std::ios::binary);
		if (!outfile.is_open())
		{
			std::cerr << "SerializeState: failed to open scenes/Level1.bin for writing\n";
			return;
		}
		outfile.write(reinterpret_cast<const char*>(builder.GetBufferPointer()), static_cast<std::streamsize>(builder.GetSize()));
		outfile.close();
	}
}

void FlatBufferScene::DeserializeState()
{
	m_entities.clear();
	m_entities.clear();

	uint32_t objectIndex = 0; // moved to function scope

	std::ifstream infile("scenes/Level1.bin", std::ios::binary);
	if (!infile.is_open())
	{
		std::cerr << "DeserializeState: Failed to open scene binary data.\n";
		return;
	}

	std::vector<char> buffer((std::istreambuf_iterator<char>(infile)), std::istreambuf_iterator<char>());

	if (buffer.empty()) return;

	// Verify and parse scene
	auto scene = Simulation::GetScene(buffer.data());
	if (!scene)
	{
		std::cerr << "DeserializeState: invalid scene data\n";
		return;
	}

	// Scene metadata: name + description + gravity
	if (scene->name())
		m_sceneName = scene->name()->str();
	else
		m_sceneName = "Scene_" + m_instanceId;

	if (scene->description())
		m_sceneDescription = scene->description()->str();
	else
		m_sceneDescription = "";

	m_gravityOn = scene->gravity_on();

	// Clear any existing cameras loaded previously
	m_cameras.clear();

	// Load all cameras stored in the flatbuffer
	if (scene->cameras() && scene->cameras()->size() > 0)
	{
		for (auto camFlat : *scene->cameras())
		{
			if (!camFlat) continue;

			// Create a new Camera instance
			Camera cam;

			// Read transform if present
			if (camFlat->transform())
			{
				const Simulation::Transform* t = camFlat->transform();
				glm::vec3 camPos(t->position().x(), t->position().y(), t->position().z());
				// Transform orientation stored yaw,pitch,roll -> Camera expects pitch(x), yaw(y), roll(z)
				glm::vec3 camRot(t->orientation().pitch(), t->orientation().yaw(), t->orientation().roll());
				cam.SetPosition(camPos);
				cam.SetRotation(camRot);
			}

			// Determine projection type and apply projection parameters if available
			if (camFlat->camera_type_type() == Simulation::CameraType_PerspectiveCamera)
			{
				auto persp = camFlat->camera_type_as_PerspectiveCamera();
				if (persp)
				{
					float fov = persp->fov();
					float near = persp->near();
					float far = persp->far();

					// Compute aspect ratio from swapchain if available, otherwise fallback
					float aspect = 16.0f / 9.0f;
					if (m_vulkanRHI)
					{
						auto extent = m_vulkanRHI->GetSwapchainExtent();
						if (extent.height != 0)
							aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
					}

					cam.SetPerspective(fov, aspect, near, far);
				}
			}
			else if (camFlat->camera_type_type() == Simulation::CameraType_OrthographicCamera)
			{
				auto ortho = camFlat->camera_type_as_OrthographicCamera();
				if (ortho)
				{
					float size = ortho->size();
					float near = ortho->near();
					float far = ortho->far();

					// Camera class may not have explicit orthographic API; keep perspective defaults but mark projection if you extend Camera.
					// For now set near/far and keep FOV-derived behaviour.
					float aspect = 16.0f / 9.0f;
					if (m_vulkanRHI)
					{
						auto extent = m_vulkanRHI->GetSwapchainExtent();
						if (extent.height != 0)
							aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
					}
					cam.SetPerspective(60.0f, aspect, near, far);
				}
			}

			m_cameras.push_back(std::move(cam));
		}
	}

	// Ensure at least one camera exists
	if (m_cameras.empty())
	{
		float aspect = 16.0f / 9.0f;
		if (m_vulkanRHI)
		{
			auto extent = m_vulkanRHI->GetSwapchainExtent();
			if (extent.height != 0)
				aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
		}
		m_cameras.emplace_back(60.0f, aspect, 0.1f, 1000.0f);
	}

	// Set active camera pointer to first camera
	m_activeCamera = &m_cameras[0];
	if (m_vulkanRHI) m_vulkanRHI->SetActiveCamera(m_activeCamera);

	// Load materials (if present)
	m_materials.clear();
	if (scene->materials())
	{
		for (auto mat : *scene->materials())
		{
			if (!mat) continue;
			MaterialData m;
			if (mat->name()) m.name = mat->name()->str();
			m.density = mat->density();
			m_materials.push_back(std::move(m));
		}
	}

	// Load material interactions (if present)
	m_materialInteractions.clear();
	if (scene->interactions())
	{
		for (auto inter : *scene->interactions())
		{
			if (!inter) continue;
			MaterialInteractionData mi;
			if (inter->material_a()) mi.materialA = inter->material_a()->str();
			if (inter->material_b()) mi.materialB = inter->material_b()->str();
			mi.restitution = inter->restitution();
			mi.staticFriction = inter->static_friction();
			mi.dynamicFriction = inter->dynamic_friction();
			m_materialInteractions.push_back(std::move(mi));
		}
	}

	// Register interaction table for collision response usage
	ClearMaterialInteractions();
	for (const auto& mi : m_materialInteractions)
	{
		MaterialInteractionCoefficients coeffs;
		coeffs.restitution = mi.restitution;
		coeffs.staticFriction = mi.staticFriction;
		coeffs.dynamicFriction = mi.dynamicFriction;
		RegisterMaterialInteraction(mi.materialA, mi.materialB, coeffs);
	}

	// Load spawners (if present) -- comprehensive parsing into SpawnerData
	m_spawners.clear();
	if (scene->spawners() && scene->spawners_type() && scene->spawners()->size() == scene->spawners_type()->size())
	{
		auto typesVec = scene->spawners_type();
		auto spawnersVec = scene->spawners();
		for (size_t i = 0; i < spawnersVec->size(); ++i)
		{
			uint8_t stype = typesVec->Get(static_cast<flatbuffers::uoffset_t>(i));
			const void* raw = spawnersVec->Get(static_cast<flatbuffers::uoffset_t>(i));
			if (!raw) continue;

			SpawnerData sd; // defaults already set in struct

			// First, extract concrete spawner-specific ranges where available
			switch (static_cast<Simulation::SpawnerType>(stype))
			{
			case Simulation::SpawnerType_SphereSpawner:
			{
				auto sp = reinterpret_cast<const Simulation::SphereSpawner*>(raw);
				if (sp && sp->base())
				{
					auto b = sp->base();
					if (b->name()) sd.name = b->name()->str();
					sd.startTime = b->start_time();
					if (b->material()) sd.material = b->material()->str();
					sd.owner = b->owner();
					sd.spawnType = static_cast<uint8_t>(b->spawn_type_type());
					sd.locationType = static_cast<uint8_t>(b->location_type());
				}
				if (sp->radius_range())
				{
					// average radius as a simple representation
					sd.sphereRadius = (sp->radius_range()->min() + sp->radius_range()->max()) * 0.5f;
				}
				break;
			}
			case Simulation::SpawnerType_CuboidSpawner:
			{
				auto sp = reinterpret_cast<const Simulation::CuboidSpawner*>(raw);
				if (sp && sp->base())
				{
					auto b = sp->base();
					if (b->name()) sd.name = b->name()->str();
					sd.startTime = b->start_time();
					if (b->material()) sd.material = b->material()->str();
					sd.owner = b->owner();
					sd.spawnType = static_cast<uint8_t>(b->spawn_type_type());
					sd.locationType = static_cast<uint8_t>(b->location_type());
				}
				if (sp->size_range())
				{
					// Vec3Range::min()/max() returns a struct reference; capture by reference/value
					const auto &min = sp->size_range()->min();
					const auto &max = sp->size_range()->max();
					sd.boxMin = glm::vec3(min.x(), min.y(), min.z());
					sd.boxMax = glm::vec3(max.x(), max.y(), max.z());
				}
				break;
			}
			case Simulation::SpawnerType_CapsuleSpawner:
			case Simulation::SpawnerType_CylinderSpawner:
			{
				// handle cylinder / capsule generically via base
				auto sp = reinterpret_cast<const Simulation::CylinderSpawner*>(raw);
				if (sp && sp->base())
				{
					auto b = sp->base();
					if (b->name()) sd.name = b->name()->str();
					sd.startTime = b->start_time();
					if (b->material()) sd.material = b->material()->str();
					sd.owner = b->owner();
					sd.spawnType = static_cast<uint8_t>(b->spawn_type_type());
					sd.locationType = static_cast<uint8_t>(b->location_type());
				}
				break;
			}
			default:
			{
				// Unknown or NONE: attempt to parse as BaseSpawner if possible
				auto basePtr = reinterpret_cast<const Simulation::BaseSpawner*>(raw);
				if (basePtr)
				{
					if (basePtr->name()) sd.name = basePtr->name()->str();
					sd.startTime = basePtr->start_time();
					if (basePtr->material()) sd.material = basePtr->material()->str();
					sd.owner = basePtr->owner();
					sd.spawnType = static_cast<uint8_t>(basePtr->spawn_type_type());
					sd.locationType = static_cast<uint8_t>(basePtr->location_type());
				}
				break;
			}
			}

			// If name still empty, assign a predictable fallback
			if (sd.name.empty())
			 sd.name = "spawner_" + std::to_string(i);

			// --- Parse spawn_type union details (SingleBurst / Repeating) using the base accessor where possible ---
			{
				// Try to retrieve a BaseSpawner pointer robustly:
				const Simulation::BaseSpawner* basePtr = nullptr;
				// If the raw is actually a BaseSpawner-derived table with a 'base' accessor, use it; otherwise cast directly.
				switch (static_cast<Simulation::SpawnerType>(stype))
				{
				case Simulation::SpawnerType_SphereSpawner:
					basePtr = reinterpret_cast<const Simulation::SphereSpawner*>(raw)->base();
					break;
				case Simulation::SpawnerType_CuboidSpawner:
					basePtr = reinterpret_cast<const Simulation::CuboidSpawner*>(raw)->base();
					break;
				case Simulation::SpawnerType_CylinderSpawner:
					basePtr = reinterpret_cast<const Simulation::CylinderSpawner*>(raw)->base();
					break;
				case Simulation::SpawnerType_CapsuleSpawner:
					basePtr = reinterpret_cast<const Simulation::CapsuleSpawner*>(raw)->base();
					break;
				default:
					basePtr = reinterpret_cast<const Simulation::BaseSpawner*>(raw);
					break;
				}

				if (basePtr)
				{
					// Spawn type specifics
					if (basePtr->spawn_type_type() == Simulation::SpawnType_SingleBurstSpawn)
					{
						auto sb = basePtr->spawn_type_as_SingleBurstSpawn();
						if (sb)
						{
							sd.singleBurstCount = sb->count();
						}
					}
					else if (basePtr->spawn_type_type() == Simulation::SpawnType_RepeatingSpawn)
					{
						auto rs = basePtr->spawn_type_as_RepeatingSpawn();
						if (rs)
						{
							sd.repeatingInterval = rs->interval();
							sd.repeatingMaxCount = rs->max_count();
						}
					}

					// Location specifics
					if (basePtr->location_type() == Simulation::SpawnLocation_FixedLocation)
					{
						auto fl = basePtr->location_as_FixedLocation();
						if (fl && fl->transform())
						{
							const auto &t = fl->transform()->position();
							sd.fixedPosition = glm::vec3(t.x(), t.y(), t.z());
						}
					}
					else if (basePtr->location_type() == Simulation::SpawnLocation_RandomBox)
					{
						auto rb = basePtr->location_as_RandomBox();
						if (rb)
						{
							// RandomBox::min()/max() return pointers to Simulation::Vec3 — capture pointer and use '->' accessors.
							const Simulation::Vec3* min = rb->min();
							const Simulation::Vec3* max = rb->max();
							if (min) sd.boxMin = glm::vec3(min->x(), min->y(), min->z());
							if (max) sd.boxMax = glm::vec3(max->x(), max->y(), max->z());
						}
					}
					else if (basePtr->location_type() == Simulation::SpawnLocation_RandomSphere)
					{
						auto rs = basePtr->location_as_RandomSphere();
						if (rs)
						{
							const Simulation::Vec3* c = rs->center();
							if (c) sd.sphereCenter = glm::vec3(c->x(), c->y(), c->z());
							sd.sphereRadius = rs->radius();
						}
					}
				}
			}

			// Push spawner data populated into in-memory list
			m_spawners.push_back(std::move(sd));
		}
	}

	// Load objects
	if (scene->objects())
	{
		// uint32_t objectIndex = 0; // remove this inner declaration
		for (auto objFlat : *scene->objects())
		{
			if (!objFlat) continue;
			const Simulation::Transform* t = objFlat->transform();
			if (!t) continue;

			glm::vec3 pos(t->position().x(), t->position().y(), t->position().z());
			glm::vec3 rot(t->orientation().pitch(), t->orientation().yaw(), t->orientation().roll());
			glm::vec3 scale(t->scale().x(), t->scale().y(), t->scale().z());

			Entity entity;
			entity.AddComponent(EComponentType::Component_Transform, pos, rot, scale);
			entity.AddComponent(EComponentType::Component_Geometry);

			std::string objName;
			if (objFlat->name())
				objName = objFlat->name()->str();
			else
				objName = "object_" + std::to_string(objectIndex);

			std::string materialName;
			if (objFlat->material())
				materialName = objFlat->material()->str();

			Simulation::Shape shapeType = objFlat->shape_type();
			if (shapeType == Simulation::Shape_NONE)
				shapeType = Simulation::Shape_Cuboid;

			Simulation::Behaviour behaviourType = objFlat->behaviour_type();
			if (behaviourType == Simulation::Behaviour_NONE)
				behaviourType = Simulation::Behaviour_SimulatedObject;

			const ObjectType objectType = BehaviourToObjectType(behaviourType);

			glm::vec3 linearVel(0.0f);
			glm::vec3 angularVel(0.0f);

			PeerID assignedOwnerId = ALL_PEERS;
			PeerID ownerColorId = ALL_PEERS;

			if (objectType == ObjectType::Simulated)
			{
				if (auto* simBehaviour = objFlat->behaviour_as_SimulatedObject())
				{
					ownerColorId = OwnerTypeToPeerId(simBehaviour->owner());
					assignedOwnerId = RemapOwnerForRuntime(ownerColorId);

					if (simBehaviour->initial_state())
					{
						const auto& lv = simBehaviour->initial_state()->linear_velocity();
						const auto& av = simBehaviour->initial_state()->angular_velocity();
						linearVel = glm::vec3(lv.x(), lv.y(), lv.z());
						angularVel = glm::vec3(av.x(), av.y(), av.z());
					}
				}
				else
				{
					ownerColorId = static_cast<PeerID>(objectIndex % 4);
					assignedOwnerId = RemapOwnerForRuntime(ownerColorId);
				}
			}

			const Simulation::CollisionType fbCollisionType = objFlat->collision_type();

			entity.AddComponent(
				EComponentType::Component_Network,
				objectIndex,
				objectType,
				assignedOwnerId,
				materialName,
				static_cast<uint8_t>(shapeType),
				static_cast<uint8_t>(behaviourType),
				static_cast<uint8_t>(fbCollisionType));

			if (objectType == ObjectType::Simulated)
			{
				entity.AddComponent(EComponentType::Component_Velocity, linearVel, angularVel, glm::vec3(0.0f));
				entity.AddComponent(EComponentType::Component_Physics);
				if (auto* phys = entity.GetComponent<ComponentPhysics>(EComponentType::Component_Physics))
				{
					// defer setting mass until we have precise geometry below; still ensure gravity flag now
					phys->SetAffectedByGravity(m_gravityOn);
				}
			}

			// Shape parameters from FB
			float shapeRadius = 0.5f;
			float shapeHeight = 1.0f;
			glm::vec3 planeNormal(0.0f, 1.0f, 0.0f);

			if (auto* s = objFlat->shape_as_Sphere())
			{
				shapeRadius = std::max(0.01f, s->radius());
			}
			else if (auto* c = objFlat->shape_as_Capsule())
			{
				shapeRadius = std::max(0.01f, c->radius());
				shapeHeight = std::max(0.01f, c->height());
			}
			else if (auto* c = objFlat->shape_as_Cylinder())
			{
				shapeRadius = std::max(0.01f, c->radius());
				shapeHeight = std::max(0.01f, c->height());
			}
			else if (auto* p = objFlat->shape_as_Plane())
			{
				if (p->normal())
				{
					planeNormal = glm::normalize(glm::vec3(
						p->normal()->x(),
						p->normal()->y(),
						p->normal()->z()));
				}
			}

			// Geometry by shape
			MeshData meshData;
			switch (shapeType)
			{
			case Simulation::Shape_Sphere:
				meshData = ResourceManager::CreateSphereMesh(1.0f, 24, 16);
				break;
			case Simulation::Shape_Cylinder:
				meshData = ResourceManager::CreateCylinderMesh(1.0f, shapeRadius, shapeHeight, 24);
				break;
			case Simulation::Shape_Capsule:
				meshData = ResourceManager::CreateCapsuleMesh(1.0f, shapeRadius, shapeHeight, 24, 16);
				break;
			case Simulation::Shape_Plane:
				meshData = ResourceManager::CreatePlaneMesh(1.0f, std::max(1.0f, scale.x), std::max(1.0f, scale.z), 1, 1);
				break;
			case Simulation::Shape_Cuboid:
			default:
				meshData = ResourceManager::CreateCubeMesh();
				break;
			}

			const auto [verts, indices] = meshData;
			ComponentGeometry* geom = entity.GetComponent<ComponentGeometry>(EComponentType::Component_Geometry);
			if (geom)
			{
				if (!geom->InitializeMesh(m_vulkanRHI, verts, indices))
				{
					std::cerr << "DeserializeState: failed to initialize mesh for entity\n";
				}
				else
				{
					if (!geom->InitializePipeline(
						m_vulkanRHI,
						m_vulkanRHI->GetRenderPass(),
						m_vulkanRHI->GetSwapchainExtent(),
						"SHADERS/object.vert.spv",
						"SHADERS/object.frag.spv"))
					{
						std::cerr << "DeserializeState: failed to initialize pipeline for entity\n";
					}

					// Owner color coding: red/green/blue/yellow
					const Texture ownerTex = CreateOwnerTexture(m_vulkanRHI, ownerColorId);
					geom->AddTexture(m_vulkanRHI, ownerTex);
				}
			}

			// Collision role + collider from shape
			entity.AddComponent(EComponentType::Component_Collision);
			if (auto* collision = entity.GetComponent<ComponentCollision>(EComponentType::Component_Collision))
			{
				collision->SetCollisionRole(
					fbCollisionType == Simulation::CollisionType_CONTAINER
					? CollisionRole::Container
					: CollisionRole::Solid);

				switch (shapeType)
				{
				case Simulation::Shape_Sphere:
				{
					collision->SetCollider(std::make_unique<Physics::Sphere>(pos, shapeRadius));
					break;
				}
				case Simulation::Shape_Capsule:
				{
					const glm::vec3 a = pos + glm::vec3(0.0f, shapeHeight * 0.5f, 0.0f);
					const glm::vec3 b = pos - glm::vec3(0.0f, shapeHeight * 0.5f, 0.0f);
					collision->SetCollider(std::make_unique<Physics::Capsule>(a, b, shapeRadius));
					break;
				}
				case Simulation::Shape_Cylinder:
				{
					const glm::vec3 a = pos + glm::vec3(0.0f, shapeHeight * 0.5f, 0.0f);
					const glm::vec3 b = pos - glm::vec3(0.0f, shapeHeight * 0.5f, 0.0f);
					collision->SetCollider(std::make_unique<Physics::Cylinder>(a, b, shapeRadius));
					break;
				}
				case Simulation::Shape_Plane:
				{
					glm::vec3 up = std::abs(glm::dot(planeNormal, glm::vec3(0.0f, 1.0f, 0.0f))) > 0.98f
						? glm::vec3(1.0f, 0.0f, 0.0f)
						: glm::vec3(0.0f, 1.0f, 0.0f);
					glm::vec3 u = glm::normalize(glm::cross(up, planeNormal));
					glm::vec3 v = glm::normalize(glm::cross(planeNormal, u));
					collision->SetCollider(std::make_unique<Physics::Plane>(pos, u, v));
					break;
				}
				case Simulation::Shape_Cuboid:
				default:
				{
					// Fallback collider for cuboid: bounding sphere
					const float r = std::max(0.01f, 0.5f * glm::length(scale));
					collision->SetCollider(std::make_unique<Physics::Sphere>(pos, r));
					break;
				}
				};

				if (auto* collider = collision->GetCollider())
				{
					collider->setRotation(rot);
					collider->setScale(scale);
				}
			}

			// Compute mass from material density and object volume for simulated objects
			if (objectType == ObjectType::Simulated)
			{
				float density = LookupMaterialDensity(m_materials, materialName);
				float volume = ComputeVolumeForShape(shapeType, scale, shapeRadius, shapeHeight);

				// If volume ended up zero (plane or unknown), fall back to small default volume to avoid zero mass
				if (volume <= 0.0f)
					volume = 1.0f; // conservative fallback

				const float mass = density * volume;

				if (auto* phys = entity.GetComponent<ComponentPhysics>(EComponentType::Component_Physics))
				{
					phys->SetMass(std::max(0.0001f, mass));
					phys->SetAffectedByGravity(m_gravityOn);
				}
			}

			// Add this section in the DeserializeState() method where animated objects are loaded
			// Find the section where behaviourType is checked and add this for Animated objects:

			if (objectType == ObjectType::Animated)
			{
				entity.AddComponent(EComponentType::Component_Collision);
				// Load animation component if the object is animated
				if (auto* animBehaviour = objFlat->behaviour_as_AnimatedObject())
				{
					entity.AddComponent(EComponentType::Component_Animation);
					if (auto* animComp = entity.GetComponent<ComponentAnimation>(EComponentType::Component_Animation))
					{
						// Set easing and path modes
						animComp->SetEasingType(static_cast<EasingType>(animBehaviour->easing()));
						animComp->SetPathMode(static_cast<PathMode>(animBehaviour->path_mode()));
						animComp->SetTotalDuration(animBehaviour->total_duration());

						// Load waypoints
						if (animBehaviour->waypoints())
						{
							for (auto wpFlat : *animBehaviour->waypoints())
							{
								if (!wpFlat) continue;

								glm::vec3 wpPos(0.0f);
								glm::vec3 wpRot(0.0f);
								float wpTime = 0.0f;

								if (wpFlat->position())
								{
									const auto& pos = wpFlat->position();
									wpPos = glm::vec3(pos->x(), pos->y(), pos->z());
								}

								if (wpFlat->rotation())
								{
									const auto& rot = wpFlat->rotation();
									// Convert from yaw, pitch, roll to pitch, yaw, roll
									wpRot = glm::vec3(rot->pitch(), rot->yaw(), rot->roll());
								}

								wpTime = wpFlat->time();

								animComp->AddWaypoint(wpPos, wpRot, wpTime);
							}
						}

						animComp->Play();
					}
				}
			}

			m_entities.push_back(std::move(entity));
			objectIndex++;
		}
	}

	// Set next network ID for spawners and rebuild runtime spawners
	m_nextSpawnNetworkId = objectIndex;
	RebuildSpawnerRuntime();

	// Apply gravity flag to all existing physics components in scene
	for (auto &e : m_entities)
	{
		if (e.HasComponent(EComponentType::Component_Physics))
		{
			auto* phys = e.GetComponent<ComponentPhysics>(EComponentType::Component_Physics);
			if (phys) phys->SetAffectedByGravity(m_gravityOn);
		}
	}
}

void FlatBufferScene::AddEntity(Entity&& entity)
{
	m_entities.push_back(std::move(entity));
}

void FlatBufferScene::RemoveEntity(int index)
{
	if (index >= 0 && index < m_entities.size())
		m_entities.erase(m_entities.begin() + index);
}

void FlatBufferScene::RebuildSpawnerRuntime()
{
	m_runtimeSpawners.clear();

	std::vector<PeerID> peers;
	for (PeerID i = 0; i < kRuntimePeerCount; ++i)
	{
		peers.push_back(i);
	}

	m_runtimeSpawners.reserve(m_spawners.size());

	for (const SpawnerData& sd : m_spawners)
	{
		SpawnerConfig cfg;
		cfg.name = sd.name;
		cfg.startTime = sd.startTime;
		cfg.spawnType = ToRuntimeSpawnType(sd.spawnType);
		cfg.locationType = ToRuntimeSpawnLocation(sd.locationType);
		cfg.shapeType = ToRuntimeSpawnerShape(sd.spawnerType);

		cfg.fixedPosition = sd.fixedPosition;
		cfg.boxMin = sd.boxMin;
		cfg.boxMax = sd.boxMax;
		cfg.sphereCenter = sd.sphereCenter;
		cfg.sphereRadius = sd.sphereRadius;

		cfg.linearVelMin = sd.linearVelMin;
		cfg.linearVelMax = sd.linearVelMax;
		cfg.angularVelMin = sd.angularVelMin;
		cfg.angularVelMax = sd.angularVelMax;

		cfg.radiusMin = std::min(sd.radiusMin, sd.radiusMax);
		cfg.radiusMax = std::max(sd.radiusMin, sd.radiusMax);
		cfg.heightMin = std::min(sd.heightMin, sd.heightMax);
		cfg.heightMax = std::max(sd.heightMin, sd.heightMax);
		cfg.sizeMin = glm::min(sd.sizeMin, sd.sizeMax);
		cfg.sizeMax = glm::max(sd.sizeMin, sd.sizeMax);

		cfg.material = sd.material;
		cfg.ownerSequential = (sd.owner == static_cast<int>(Simulation::SpawnerOwnerType_SEQUENTIAL));
		cfg.owner = cfg.ownerSequential
			? 0
			: RemapOwnerForRuntime(SpawnerOwnerToPeerId(static_cast<Simulation::SpawnerOwnerType>(std::clamp(sd.owner, 0, 3))));

		cfg.burstCount = std::max(1u, sd.singleBurstCount);
		cfg.repeatInterval = std::max(0.01f, sd.repeatingInterval);
		cfg.repeatMaxCount = std::max(1u, sd.repeatingMaxCount);

		Spawner runtimeSpawner(cfg);
		runtimeSpawner.SetPeers(peers);
		runtimeSpawner.Start();
		runtimeSpawner.SetActive(true);

		runtimeSpawner.SetSpawnCallback(
			[this, sd](const glm::vec3& pos,
				const glm::vec3& linearVel,
				const glm::vec3& angularVel,
				const glm::vec3& randomSize,
				float radius,
				float height,
				PeerID owner)
			{
				SpawnEntityFromSpawner(sd, pos, linearVel, angularVel, randomSize, radius, height, RemapOwnerForRuntime(owner));
			});

		m_runtimeSpawners.push_back(std::move(runtimeSpawner));
	}
}

void FlatBufferScene::UpdateSpawnerRuntime(float deltaTime)
{
	if (deltaTime <= 0.0f || m_runtimeSpawners.empty())
		return;

	m_sceneTime += deltaTime;

	std::lock_guard<std::mutex> lock(m_sceneMutex);
	for (Spawner& spawner : m_runtimeSpawners)
	{
		if (spawner.Update(deltaTime, m_sceneTime))
		{
			spawner.Spawn(m_sceneTime);
		}
	}
}

void FlatBufferScene::SpawnEntityFromSpawner(
	const SpawnerData& spawner,
	const glm::vec3& position,
	const glm::vec3& linearVelocity,
	const glm::vec3& angularVelocity,
	const glm::vec3& randomSize,
	float radius,
	float height,
	PeerID ownerId)
{
	Entity entity;

	const Simulation::Shape shape = SpawnerTypeToShape(spawner.spawnerType);
	const Simulation::Behaviour behaviour = ObjectTypeToBehaviour(spawner.objectType);
	const Simulation::CollisionType collisionType = spawner.spawnAsSolid
		? Simulation::CollisionType_SOLID
		: Simulation::CollisionType_CONTAINER;

	glm::vec3 scale(1.0f);
	if (shape == Simulation::Shape_Cuboid)
	{
		scale = glm::max(randomSize, glm::vec3(0.01f));
	}
	else if (shape == Simulation::Shape_Sphere)
	{
		const float d = std::max(0.02f, radius * 2.0f);
		scale = glm::vec3(d);
	}

	entity.AddComponent(EComponentType::Component_Transform, position, glm::vec3(0.0f), scale);
	entity.AddComponent(EComponentType::Component_Geometry);

	entity.AddComponent(
		EComponentType::Component_Network,
		m_nextSpawnNetworkId++,
		spawner.objectType,
		ownerId,
		spawner.material,
		static_cast<uint8_t>(shape),
		static_cast<uint8_t>(behaviour),
		static_cast<uint8_t>(collisionType));

	if (spawner.objectType == ObjectType::Simulated)
	{
		entity.AddComponent(EComponentType::Component_Velocity, linearVelocity, angularVelocity, glm::vec3(0.0f));
		entity.AddComponent(EComponentType::Component_Physics);
		if (auto* phys = entity.GetComponent<ComponentPhysics>(EComponentType::Component_Physics))
		{
			// Defer mass assignment until we compute exact geometry below; ensure gravity flag
			phys->SetAffectedByGravity(m_gravityOn);
		}
	}

	MeshData meshData;
	switch (shape)
	{
	case Simulation::Shape_Sphere:
		meshData = ResourceManager::CreateSphereMesh(1.0f, 24, 16);
		break;
	case Simulation::Shape_Cylinder:
		meshData = ResourceManager::CreateCylinderMesh(1.0f, std::max(0.01f, radius), std::max(0.01f, height), 24);
		break;
	case Simulation::Shape_Capsule:
		meshData = ResourceManager::CreateCapsuleMesh(1.0f, std::max(0.01f, radius), std::max(0.01f, height), 24, 16);
		break;
	case Simulation::Shape_Cuboid:
	default:
		meshData = ResourceManager::CreateCubeMesh();
		break;
	}

	if (auto* geom = entity.GetComponent<ComponentGeometry>(EComponentType::Component_Geometry))
	{
		const auto [verts, indices] = meshData;
		geom->InitializeMesh(m_vulkanRHI, verts, indices);
		geom->InitializePipeline(
			m_vulkanRHI,
			m_vulkanRHI->GetRenderPass(),
			m_vulkanRHI->GetSwapchainExtent(),
			"SHADERS/object.vert.spv",
			"SHADERS/object.frag.spv");

		const Texture ownerTex = CreateOwnerTexture(m_vulkanRHI, ownerId);
		geom->AddTexture(m_vulkanRHI, ownerTex);
	}

	entity.AddComponent(EComponentType::Component_Collision);
	if (auto* collision = entity.GetComponent<ComponentCollision>(EComponentType::Component_Collision))
	{
		collision->SetCollisionRole(spawner.spawnAsSolid ? CollisionRole::Solid : CollisionRole::Container);

		switch (shape)
		{
		case Simulation::Shape_Sphere:
			collision->SetCollider(std::make_unique<Physics::Sphere>(position, std::max(0.01f, radius)));
			break;

		case Simulation::Shape_Cylinder:
		{
			const float h = std::max(0.01f, height);
			const glm::vec3 a = position + glm::vec3(0.0f, h * 0.5f, 0.0f);
			const glm::vec3 b = position - glm::vec3(0.0f, h * 0.5f, 0.0f);
			collision->SetCollider(std::make_unique<Physics::Cylinder>(a, b, std::max(0.01f, radius)));
			break;
		}
		case Simulation::Shape_Capsule:
		{
			const float h = std::max(0.01f, height);
			const glm::vec3 a = position + glm::vec3(0.0f, h * 0.5f, 0.0f);
			const glm::vec3 b = position - glm::vec3(0.0f, h * 0.5f, 0.0f);
			collision->SetCollider(std::make_unique<Physics::Capsule>(a, b, std::max(0.01f, radius)));
			break;
		}
		case Simulation::Shape_Cuboid:
		default:
		{
			const float r = std::max(0.01f, 0.5f * glm::length(scale));
			collision->SetCollider(std::make_unique<Physics::Sphere>(position, r));
			break;
		}
		}
	}

	// Compute and assign mass based on material density and shape volume
	if (spawner.objectType == ObjectType::Simulated)
	{
		float density = LookupMaterialDensity(m_materials, spawner.material);
		float volume = ComputeVolumeForShape(shape, scale, radius, height);

		if (volume <= 0.0f)
			volume = 1.0f; // fallback to conservative default

		const float mass = density * volume;

		if (auto* phys = entity.GetComponent<ComponentPhysics>(EComponentType::Component_Physics))
		{
			phys->SetMass(std::max(0.0001f, mass));
			phys->SetAffectedByGravity(m_gravityOn);
		}
	}

	m_entities.push_back(std::move(entity));
}