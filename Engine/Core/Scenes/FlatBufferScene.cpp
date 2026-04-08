#include "FlatBufferScene.h"
#include "../Managers/ResourceManager.h"
#include "../../IMGUI/imgui.h"
#include "../Systems/SystemSwapBuffers.h"
#include "../../../PhysicsEngine/Networking/TCPSocket.h"

#include <fstream>
#include <iostream>
#include <vector>
#include <filesystem>
#include <string>
#include <random>

#include "../Systems/SystemVelocity.h"
#include "../Systems/SystemPhysics.h"
#include "../Systems/SystemCollision.h"
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


FlatBufferScene::FlatBufferScene(Window& p_window, VulkanRHI* rhi, GUI* p_gui) :
	m_window(&p_window), m_inputHandler(p_window), m_vulkanRHI(rhi),
	m_gui(p_gui), m_systemManager(rhi, 2)
{
	m_instanceId = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
	m_localPeerId = static_cast<PeerID>(std::hash<std::string>{}(m_instanceId) % 2);

	// Camera, vulkan, and imgui Initialisation
	m_cameras.reserve(100);
	m_paused = false;

	// Ensure sensible defaults for scene-level properties so missing flatbuffer fields
	// fall back to explicit defaults.
	m_sceneName = "";
	m_sceneDescription = "";
	m_gravityOn = true; // default to gravity enabled unless scene explicitly disables it
	m_selectedEntityIndex = -1;

	// 1. Ensure the file exists before attempting to load
	if (!std::filesystem::exists("scenes/Level1.bin"))
	{
		std::cout << "Bin file not found, creating a default one...\n";
		SerializeState();
	}

	// 2. Load properties directly into the scene
	DeserializeState();
	m_activeCamera = &m_cameras[0];

	//// 3. TEMPORARY: Manually Add a Cube Entity so we have something to render
	auto createEntity = [&](glm::vec3 pos)
		{
			Entity cube;
			cube.AddComponent(EComponentType::Component_Transform, pos, glm::vec3(0.0f), glm::vec3(1.0f));
			cube.AddComponent(EComponentType::Component_Geometry);

			ComponentGeometry* geom = cube.GetComponent<ComponentGeometry>(EComponentType::Component_Geometry);
			//if (geom)
			{
				// You can swap this with a pure cube mesh if your ResourceManager supports it
				MeshData meshData = ResourceManager::CreateCubeMesh(); // Fallback visible mesh
				auto [verts, indices] = meshData;
				geom->InitializeMesh(m_vulkanRHI, verts, indices);
				geom->InitializePipeline(m_vulkanRHI, m_vulkanRHI->GetRenderPass(), m_vulkanRHI->GetSwapchainExtent(), "SHADERS/object.vert.spv", "SHADERS/object.frag.spv");

				// Note: Requires a valid texture
				const Texture defaultTex(m_vulkanRHI, "Assets/red_brick_diff_1k.jpg", TextureType::Albedo, true);
				geom->AddTexture(m_vulkanRHI, defaultTex);
			}
			m_entities.push_back(std::move(cube));
		};
	//createEntity(glm::vec3(-5.0f, 0.0f, 0.0f));
	//createEntity(glm::vec3(5.0f, 0.0f, 0.0f));

	// 4. Register the required systems so objects are rendered
	m_systemManager.RegisterSystem(std::make_unique<SystemVelocity>());
	m_systemManager.RegisterSystem(std::make_unique<SystemCollision>(m_entities.size(), m_vulkanRHI));
	m_systemManager.RegisterSystem(std::make_unique<SystemSwapBuffers>());



	{
		auto address = GetClientAddress();
		std::cout << "Server listening on " << address.getIP() << ":" << address.getPort() << std::endl;
	}

	PeerID localPeerId = m_localPeerId;

	// 4. Register the required systems so objects are rendered
	m_systemManager.RegisterSystem(std::make_unique<SystemVelocity>());
	m_systemManager.RegisterSystem(std::make_unique<SystemPhysics>(localPeerId));
	m_systemManager.RegisterSystem(std::make_unique<SystemCollision>(m_entities.size(), m_vulkanRHI));
	m_systemManager.RegisterSystem(std::make_unique<SystemSwapBuffers>());
	m_networkData = std::make_shared<SharedNetworkData>();
	m_systemManager.RegisterSystem(std::make_unique<SystemNetworkSync>(m_localPeerId, m_networkData));
}

Networking::Address FlatBufferScene::GetClientAddress()
{
	static std::mt19937 rng{ std::random_device{}() };
	static std::uniform_int_distribution<int> dist(10000, 19999);
	const int random = dist(rng);

	// Listen on 0.0.0.0 to support discovery broadcast and external clients
	Networking::Address bindAddr("0.0.0.0", random);
	m_tcpListener = std::make_unique<Networking::ListeningSocket>(bindAddr);
	m_tcpListener->SetNonBlocking(true); // Don't block when accepting

	m_networkRunning = true;
	m_networkThread = std::thread([this, random]() {

		// 1. Setup UDP Broadcast Socket for Discovery (send + receive)
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

		sockaddr_in broadcastAddr{};
		broadcastAddr.sin_family = AF_INET;
		broadcastAddr.sin_port = htons(8888); // An agreed port for clients to listen for broadcasts
		inet_pton(AF_INET, "255.255.255.255", &broadcastAddr.sin_addr);

		std::vector<SOCKET> clientSockets;
		auto lastBroadcastTime = std::chrono::steady_clock::now();

		while (m_networkRunning) {
			// Broadcast presence (e.g. every 1 second)
			auto now = std::chrono::steady_clock::now();
			if (udpSocket != INVALID_SOCKET && std::chrono::duration_cast<std::chrono::seconds>(now - lastBroadcastTime).count() >= 1) {
				std::string msg = "SERVER_DISCOVERY:" + std::to_string(random) + ":" + m_instanceId;
				sendto(udpSocket, msg.c_str(), static_cast<int>(msg.size()), 0,
					reinterpret_cast<sockaddr*>(&broadcastAddr), sizeof(broadcastAddr));
				lastBroadcastTime = now;
			}

			// Listen for discovery broadcasts and connect to the first peer we see
			if (!m_peerConnected && udpSocket != INVALID_SOCKET) {
				char recvBuf[256]{};
				sockaddr_in from{};
				int fromLen = sizeof(from);
				int recvBytes = recvfrom(udpSocket, recvBuf, sizeof(recvBuf) - 1, 0,
					reinterpret_cast<sockaddr*>(&from), &fromLen);

				if (recvBytes > 0) {
					recvBuf[recvBytes] = '\0';
					const std::string msg(recvBuf);

					const std::string prefix = "SERVER_DISCOVERY:";
					if (msg.rfind(prefix, 0) == 0) {
						const std::string payload = msg.substr(prefix.size());
						const size_t sep = payload.find(':');
						if (sep != std::string::npos) {
							const int port = std::stoi(payload.substr(0, sep));
							const std::string peerId = payload.substr(sep + 1);

							if (peerId != m_instanceId && port != random) {
								char ipStr[INET_ADDRSTRLEN]{};
								inet_ntop(AF_INET, &from.sin_addr, ipStr, sizeof(ipStr));

								try {
									m_tcpClient = std::make_unique<Networking::TCPSocket>(Networking::Address(ipStr, static_cast<uint16_t>(port)));
									m_peerConnected = true;
									
									u_long mode = 1;
									ioctlsocket(m_tcpClient->native_handle(), FIONBIO, &mode); // Set the client to non-blocking
									
									std::cout << "Connected to peer at " << ipStr << ":" << port << "\n";
								}
								catch (const std::exception& ex) {
									std::cerr << "Peer connect failed: " << ex.what() << "\n";
								}
							}
						}
					}
				}
			}

			// 2. Accept TCP connections
			Networking::Address clientAddr;
			SOCKET newClient = m_tcpListener->Accept(clientAddr);
			if (newClient != INVALID_SOCKET) {
				u_long mode = 1;
				ioctlsocket(newClient, FIONBIO, &mode); // Set the client to non-blocking
				clientSockets.push_back(newClient);
				std::cout << "Peer to peer client connected from: " << clientAddr.getIP() << ":" << clientAddr.getPort() << std::endl;
			}

			// Extract outgoing packets safely
			std::vector<SyncPacket> outgoing;
			if (m_networkData)
			{
				std::lock_guard<std::mutex> lock(m_networkData->outgoingMutex);
				outgoing = m_networkData->outgoingPackets;
				m_networkData->outgoingPackets.clear();
			}

			// 3. Send/Receive data and gracefully handle disconnects
			for (auto it = clientSockets.begin(); it != clientSockets.end(); ) {
				SOCKET client = *it;
				bool clientOk = true;

				// Send outgoing data
				for (const auto& packet : outgoing) {
					int sent = send(client, reinterpret_cast<const char*>(&packet), sizeof(SyncPacket), 0);
					if (sent == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK) {
						clientOk = false;
						break;
					}
				}

				SyncPacket incomPacket;
				int bytes;
				do {
					bytes = recv(client, reinterpret_cast<char*>(&incomPacket), sizeof(SyncPacket), 0);
					if (bytes == sizeof(SyncPacket)) {
						if (m_networkData) {
							std::lock_guard<std::mutex> lock(m_networkData->incomingMutex);
							m_networkData->incomingPackets.push_back(incomPacket);
						}
					}
				} while (bytes > 0);

				if (bytes == 0 || (bytes == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)) {
					std::cout << "Client disconnected.\n";
					closesocket(client);
					it = clientSockets.erase(it);
				}
				else if (!clientOk) {
					std::cout << "Client transmission failed.\n";
					closesocket(client);
					it = clientSockets.erase(it);
				}
				else {
					++it;
				}
			}
			
			// 4. Do the same for m_tcpClient if connected out to a peer
			if (m_peerConnected && m_tcpClient) {
				SOCKET client = m_tcpClient->native_handle();
				bool clientOk = true;

				for (const auto& packet : outgoing) {
					int sent = send(client, reinterpret_cast<const char*>(&packet), sizeof(SyncPacket), 0);
					if (sent == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK) {
						clientOk = false;
						break;
					}
				}

				SyncPacket incomPacket;
				int bytes;
				do {
					bytes = recv(client, reinterpret_cast<char*>(&incomPacket), sizeof(SyncPacket), 0);
					if (bytes == sizeof(SyncPacket)) {
						if (m_networkData) {
							std::lock_guard<std::mutex> lock(m_networkData->incomingMutex);
							m_networkData->incomingPackets.push_back(incomPacket);
						}
					}
				} while (bytes > 0);

				if (bytes == 0 || (bytes == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK) || !clientOk) {
					std::cout << "Disconnected from server peer.\n";
					m_peerConnected = false;
					m_tcpClient.reset();
				}
			}

			// Sleep briefly to prevent 100% CPU thread pegging
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}

		// Loop shut down, cleanup
		if (udpSocket != INVALID_SOCKET) closesocket(udpSocket);
		for (SOCKET client : clientSockets) closesocket(client);
		});

	std::cout << "Network server started on port " << random << std::endl;
	return bindAddr;
}

FlatBufferScene::~FlatBufferScene()
{
	m_networkRunning = false;
	if (m_networkThread.joinable())
	{
		m_networkThread.join();
	}

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
	m_networkRunning = false;
	if (m_networkThread.joinable())
	{
		m_networkThread.join();
	}

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
	// 1. Start the physical network sockets/connections...
	
	// 2. Launch Dedicated Physics Thread
	m_isPhysicsRunning = true;
	m_physicsThread = std::thread([this]() {
		auto lastTime = std::chrono::high_resolution_clock::now();
		while (m_isPhysicsRunning) {
			auto currentTime = std::chrono::high_resolution_clock::now();
			float dt = std::chrono::duration<float>(currentTime - lastTime).count();
			lastTime = currentTime;

			// Lock the state while simulating to prevent tearing with the rendering thread
			{
				std::lock_guard<std::mutex> lock(m_sceneMutex);
				
				// Only run Physics and Network Sync on this thread!
				// m_systemManager.GetSystem<SystemPhysics>()->OnUpdate(m_entities, dt);
				// m_systemManager.GetSystem<SystemNetworkSync>()->OnUpdate(m_entities, dt);
			}

			// Sleep to maintain a fixed physics tick rate (e.g., 60Hz)
			std::this_thread::sleep_for(std::chrono::milliseconds(16));
		}
	});

	// add diagnostics only once
	static bool diagCreated = false;
	if (!diagCreated) {
		CreateDiagnosticCube();
		diagCreated = true;
	}
}
void FlatBufferScene::Stop() {}

void FlatBufferScene::Update(float deltaTime)
{
	m_window->PollEvents();
	m_deltaTime = deltaTime;
	deltaTime = m_paused ? 0.0f : deltaTime;

	m_systemManager.Update(m_entities, deltaTime);
}

void FlatBufferScene::FixedUpdate() {}

void FlatBufferScene::Draw()
{
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

							// Projection parameters for perspective cameras
							float fov = m_activeCamera->GetFovDeg();
							if (ImGui::DragFloat("FOV (deg)", &fov, 0.25f, 1.0f, 179.0f))
							{
								m_activeCamera->SetPerspective(fov, m_activeCamera->GetAspect(), m_activeCamera->GetNear(), m_activeCamera->GetFar());
								m_activeCamera->MarkDirty();
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

			if (show_demo_window)
				ImGui::ShowDemoWindow(&show_demo_window);

			if (show_about)
			{
				ImGui::Begin("Debug Diagnostics");
				ImGui::Text("File: FlatBuffer Scene Loaded");
				ImGui::Text("FPS: %.1f", m_deltaTime > 0 ? (1.0f / m_deltaTime) : 0);
				ImGui::Text("Object Count: %d", m_entities.size());
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
	if (m_inputHandler.isKeyPressed(GLFW_KEY_ESCAPE)) glfwSetWindowShouldClose(m_window->getGLFWwindow(), true);

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
			Simulation::Vec3 camScaleStruct(1.0f, 1.0f, 1.0f); // cameras don't use scale but Transform requires it
			Simulation::Transform camTransform(camPosStruct, camOrientStruct, camScaleStruct);

			auto camName = builder.CreateString(("Camera" + std::to_string(i)).c_str());

			// Use perspective camera type and write projection params
			auto perspectiveOffset = Simulation::CreatePerspectiveCamera(
				builder,
				cam.GetFovDeg(),
				cam.GetNear(),
				cam.GetFar()
			);

			auto camOffset = Simulation::CreateCamera(
				builder,
				camName,
				&camTransform,
				Simulation::CameraType_PerspectiveCamera,
				perspectiveOffset.Union()
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

			// Create the Object table. Use Shape_Cuboid and default behaviour/collision.
			auto objOffset = Simulation::CreateObject(
				builder,
				name,
				&objTransform,
				0, // material
				Simulation::Shape_Cuboid,
				cuboidOffset.Union(), // union as void offset (Offset<T> convertible)
				Simulation::Behaviour_NONE,
				0,
				Simulation::CollisionType_SOLID
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
				// Orthographic camera in schema exists — for now, read and apply basic orthographic params if present.
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
		uint32_t objectIndex = 0; // Use an index to distribute ownership

		for (auto objFlat : *scene->objects())
		{
			if (!objFlat) continue;
			const Simulation::Transform* t = objFlat->transform();
			if (!t) continue;

			glm::vec3 pos(t->position().x(), t->position().y(), t->position().z());
			glm::vec3 rot(t->orientation().pitch(), t->orientation().yaw(), t->orientation().roll());
			glm::vec3 scale(t->scale().x(), t->scale().y(), t->scale().z());

			// Create entity and components
			Entity entity;
			entity.AddComponent(EComponentType::Component_Transform, pos, rot, scale);
			entity.AddComponent(EComponentType::Component_Geometry);

			// Assign a name if present, otherwise generate unique name (store in m_sceneName context or logs)
			std::string objName;
			if (objFlat->name())
				objName = objFlat->name()->str();
			else
				objName = "object_" + std::to_string(objectIndex);

			// --- DISTRIBUTED OWNERSHIP LOGIC ---
			PeerID assignedOwnerId = static_cast<PeerID>(objectIndex % 2);
			bool isOwnedByMe = (assignedOwnerId == m_localPeerId);

			// Add network ownership so systems + UI can detect ownership
			entity.AddComponent(EComponentType::Component_Network, objectIndex, ObjectType::Simulated, assignedOwnerId);

			// Load texture based on ownership, but prefer material name from object if present
			std::string texturePath;
			if (objFlat->material() && objFlat->material()->c_str()[0] != '\0') {
				// simple heuristic: if material name ends with common texture name use it, else fallback to ownership colour
				std::string matName = objFlat->material()->str();
				// If a material exists in m_materials with that name and density, we won't infer texture path here.
				texturePath = isOwnedByMe ? "Assets/red_brick_diff_1k.jpg" : "Assets/mossy_cobblestone_diff_1k.jpg";
			} else {
				texturePath = isOwnedByMe ? "Assets/red_brick_diff_1k.jpg" : "Assets/mossy_cobblestone_diff_1k.jpg";
			}
			const Texture objectTex(m_vulkanRHI, texturePath, TextureType::Albedo, true);
			// -----------------------------------

			ComponentGeometry* geom = entity.GetComponent<ComponentGeometry>(EComponentType::Component_Geometry);
			if (geom)
			{
				// For cuboid shapes we use the cube mesh and let the transform scale it.
				MeshData meshData = ResourceManager::CreateCubeMesh();
				auto [verts, indices] = meshData;
				if (!geom->InitializeMesh(m_vulkanRHI, verts, indices))
				{
					std::cerr << "DeserializeState: failed to initialize mesh for entity\n";
				}
				else
				{
					if (!geom->InitializePipeline(m_vulkanRHI, m_vulkanRHI->GetRenderPass(), m_vulkanRHI->GetSwapchainExtent(),
						"SHADERS/object.vert.spv", "SHADERS/object.frag.spv"))
					{
						std::cerr << "DeserializeState: failed to initialize pipeline for entity\n";
					}
					
					// Apply the dynamically chosen texture
					geom->AddTexture(m_vulkanRHI, objectTex);
				}
			}

			// If gravity setting exists, set ComponentPhysics affected flag for any physics components that already exist.
			if (entity.HasComponent(EComponentType::Component_Physics))
			{
				auto* phys = entity.GetComponent<ComponentPhysics>(EComponentType::Component_Physics);
				if (phys)
					phys->SetAffectedByGravity(m_gravityOn);
			}

			m_entities.push_back(std::move(entity));
			objectIndex++;
		}
	}

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

void FlatBufferScene::CreateDiagnosticCube()
{
	if (!m_vulkanRHI || m_vulkanRHI->GetDevice() == VK_NULL_HANDLE) {
		std::cerr << "Diagnostic: VulkanRHI not initialized\n";
		return;
	}

	Entity testCube;
	testCube.AddComponent(EComponentType::Component_Transform,
		glm::vec3(0.0f, 0.0f, 0.0f),
		glm::vec3(0.0f),
		glm::vec3(1.0f));
	testCube.AddComponent(EComponentType::Component_Geometry);

	ComponentGeometry* geom = testCube.GetComponent<ComponentGeometry>(EComponentType::Component_Geometry);
	if (!geom) { std::cerr << "Diagnostic: no geometry component\n"; return; }

	auto meshData = ResourceManager::CreateCubeMesh();
	auto [verts, indices] = meshData;
	bool meshOk = geom->InitializeMesh(m_vulkanRHI, verts, indices);
	std::cout << "Diagnostic: InitializeMesh = " << meshOk << "\n";
	bool pipeOk = geom->InitializePipeline(m_vulkanRHI, m_vulkanRHI->GetRenderPass(), m_vulkanRHI->GetSwapchainExtent(),
		"SHADERS/object.vert.spv", "SHADERS/object.frag.spv");
	std::cout << "Diagnostic: InitializePipeline = " << pipeOk << "\n";
	std::cout << "Diagnostic: IsValid = " << geom->IsValid() << "\n";

	m_entities.push_back(std::move(testCube));
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