#pragma once
#include "IScene.h"
#include <vector>
#include "../../Renderer/VulkanRHI.h"
#include "../Managers/SystemManager.h"
#include "../../Renderer/GUI.h"
#include "../InputHandler.h"
#include "../../Renderer/Camera.h"

class Entity;
class Window;


class FlatBufferScene : public IScene
{
public:
	// Inherited via IScene
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
};