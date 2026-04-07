#pragma once
#include "IScene.h"
#include <vector>
#include "../../Renderer/VulkanRHI.h"
#include "../Managers/SystemManager.h"
#include "../../Renderer/GUI.h"
#include "../InputHandler.h"
#include "../../Renderer/Camera.h"

#include "../../../PhysicsEngine/Networking/ListeningSocket.h"

#include <thread>
#include <atomic>
#include <mutex>

namespace Networking { class TCPSocket; }

class Entity;
class Window;

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
	void AddEntity(Entity&& entity) override;
	void RemoveEntity(int index) override;

	FlatBufferScene(Window& p_window, VulkanRHI* rhi, GUI* p_gui);
	~FlatBufferScene() override;

private:
	Networking::Address GetClientAddress();

	void CreateDiagnosticCube();

	std::vector<Entity> m_entities;
	VulkanRHI* m_vulkanRHI;

	SystemManager m_systemManager;
	GUI* m_gui;
	Window* m_window;

	float m_deltaTime = 0.0f;
	bool m_paused = false;

	InputHandler m_inputHandler;

	std::vector<Camera> m_cameras;
	Camera* m_activeCamera = nullptr;

	// Network Server Members
	std::unique_ptr<Networking::ListeningSocket> m_tcpListener;
	std::unique_ptr<Networking::TCPSocket> m_tcpClient;
	std::thread m_networkThread;
	std::atomic<bool> m_networkRunning = false;
	std::atomic<bool> m_peerConnected = false;
	std::string m_instanceId;

	// Physics Members
	std::thread m_physicsThread;
	std::atomic<bool> m_isPhysicsRunning{false};
	std::mutex m_sceneMutex; // Protects m_entities during sync
};