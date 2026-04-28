#pragma once
#include "IScene.h"
#include <vector>
#include <string>
#include "../../Renderer/VulkanRHI.h"
#include "../Managers/SystemManager.h"
#include "../../Renderer/GUI.h"
#include "../InputHandler.h"
#include "../../Renderer/Camera.h"
#include "../NetworkTypes.h"
#include "../Spawner.h"

#include "../../../PhysicsEngine/Networking/ListeningSocket.h"

#include <thread>
#include <atomic>
#include <mutex>

namespace Networking { class TCPSocket; }

class Entity;
class Window;

enum class NetworkRole
{
	Host,
	Client
};

struct SpawnerData {
	std::string name;
	float startTime = 0.0f;
	uint8_t spawnType = 0;
	uint8_t locationType = 0;
	uint8_t spawnerType = 0;
	std::string material;
	int owner = 0;
	ObjectType objectType = ObjectType::Simulated;
	bool spawnAsSolid = true;
	glm::vec3 fixedPosition = glm::vec3(0.0f);
	glm::vec3 boxMin = glm::vec3(-1.0f);
	glm::vec3 boxMax = glm::vec3(1.0f);
	glm::vec3 sphereCenter = glm::vec3(0.0f);
	float sphereRadius = 1.0f;
	glm::vec3 linearVelMin = glm::vec3(-1.0f);
	glm::vec3 linearVelMax = glm::vec3(1.0f);
	glm::vec3 angularVelMin = glm::vec3(-1.0f);
	glm::vec3 angularVelMax = glm::vec3(1.0f);
	float radiusMin = 0.5f;
	float radiusMax = 0.5f;
	float heightMin = 1.0f;
	float heightMax = 1.0f;
	glm::vec3 sizeMin = glm::vec3(1.0f);
	glm::vec3 sizeMax = glm::vec3(1.0f);
	uint32_t singleBurstCount = 1;
	float repeatingInterval = 1.0f;
	uint32_t repeatingMaxCount = 10;
};

struct MaterialInteractionData {
	std::string materialA;
	std::string materialB;
	float restitution = 0.0f;
	float staticFriction = 0.0f;
	float dynamicFriction = 0.0f;
};

struct MaterialData {
	std::string name;
	float density = 0.0f;
};

class FlatBufferScene : public IScene
{
public:
	void Start(float deltaTime) override;
	void Stop() override;
	void Update(float deltaTime) override;
	void FixedUpdate() override;
	void Draw() override;
	void SerializeState() override;
	void DeserializeState() override;
	void HandleInput(float deltaTime) override;
	void Destroy() override;
	void AddEntity(Entity&& entity) override { 
		std::lock_guard<std::mutex> lock(m_sceneMutex);
		m_entities.push_back(std::move(entity)); 
	}
	void RemoveEntity(int index) override { 
		std::lock_guard<std::mutex> lock(m_sceneMutex);
		if (index >= 0 && index < m_entities.size()) {
			m_entities.erase(m_entities.begin() + index); 
		}
	}

	FlatBufferScene(Window& p_window, VulkanRHI* rhi, GUI* p_gui);
	~FlatBufferScene() override;

private:
	Networking::Address GetClientAddress();

	void RebuildSpawnerRuntime();
	void UpdateSpawnerRuntime(float deltaTime);
	void SpawnEntityFromSpawner(
		const SpawnerData& spawner,
		const glm::vec3& position,
		const glm::vec3& linearVelocity,
		const glm::vec3& angularVelocity,
		const glm::vec3& randomSize,
		float radius,
		float height,
		PeerID ownerId);
	uint32_t BuildSpawnNetworkId(PeerID ownerId);
	void HandleSpawnMessage(const SpawnPacket& packet);

	std::vector<Entity> m_entities;
	VulkanRHI* m_vulkanRHI;

	SystemManager m_systemManager;
	GUI* m_gui;
	Window* m_window;

	float m_deltaTime = 0.0f;
	std::atomic<float> m_renderDeltaTime{ 0.0f };
	std::atomic<bool> m_paused{ false };
	std::atomic<int> m_physicsHz{ 120 };
	std::atomic<int> m_graphicsHz{ 30 };

	InputHandler m_inputHandler;

	std::vector<Camera> m_cameras;
	Camera* m_activeCamera = nullptr;

	std::string m_sceneName = "";
	std::string m_sceneDescription = "";
	bool m_gravityOn = true;

	std::unique_ptr<Networking::ListeningSocket> m_tcpListener;
	std::unique_ptr<Networking::TCPSocket> m_tcpClient;
	std::thread m_networkThread;
	std::atomic<bool> m_networkRunning = false;
	std::atomic<bool> m_peerConnected = false;
	std::string m_instanceId;

	std::atomic<PeerID> m_localPeerId{ 0 };
	std::atomic<PeerID> m_runtimePeerCount{ 3 }; // NEW
	NetworkRole m_networkRole = NetworkRole::Host; // NEW

	int m_selectedEntityIndex = -1;

	std::mutex m_sceneMutex;
	std::shared_ptr<SharedNetworkData> m_networkData;
	std::vector<MaterialData> m_materials;
	std::vector<MaterialInteractionData> m_materialInteractions;
	std::vector<SpawnerData> m_spawners;

	std::vector<Spawner> m_runtimeSpawners;
	float m_sceneTime = 0.0f;
	uint32_t m_nextSpawnNetworkId = 0;

	int num_boids = 2048;
};