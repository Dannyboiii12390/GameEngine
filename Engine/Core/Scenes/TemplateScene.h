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

class Entity;


class TemplateScene : public IScene
{
public:
	
	
	TemplateScene(Window& p_window, VulkanRHI* rhi);
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



	SystemRenderer m_renderer; // temporary for testing, should be owned by system manager, not scene

	std::unique_ptr<GUI> m_gui;




};