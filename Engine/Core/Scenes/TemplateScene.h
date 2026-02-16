#pragma once
#include "IScene.h"
#include <vector>
#include <memory>
#include "../InputHandler.h"
#include "../Systems/SystemRenderer.h"
#include "../../Renderer/Window.h"
#include "../../Renderer/VulkanRHI.h"
#include "../../Renderer/Camera.h"
#include "../../Renderer/GUI.h"
#include "../Systems/SystemVelocity.h"
#include "../Systems/SystemPhysics.h"

class Entity;


class TemplateScene : public IScene
{
public:
	
	
	TemplateScene(Window& p_window, VulkanRHI* rhi, GUI* p_gui);
	~TemplateScene() override;

	void Start(float deltaTime) override;
	void Stop() override;

	void Update(float deltaTime) override;
	void FixedUpdate() override;

	void Draw() override;

	void SerializeState() override;
	void DeserializeState() override;

	void HandleInput(float deltaTime) override;

private:
	void AddEntity(Entity&& entity) override;
	void RemoveEntity(int index) override;

	
	std::vector<Entity> m_entities;
	Camera m_camera;
	InputHandler m_inputHandler;
	Window* m_window;
	VulkanRHI* m_vulkanRHI;

	// temporary for testing, should be owned by system manager, not scene
	SystemRenderer m_renderer;
	SystemVelocity m_velocitySystem;
	SystemPhysics m_physicsSystem;

	GUI* m_gui;
};