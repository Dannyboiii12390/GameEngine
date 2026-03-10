#pragma once
#include "IScene.h"
#include <vector>
#include "../InputHandler.h"
#include "../Systems/SystemRenderer.h"
#include "../../Renderer/Window.h"
#include "../../Renderer/VulkanRHI.h"
#include "../../Renderer/Camera.h"
#include "../../Renderer/GUI.h"
#include "../Systems/SystemVelocity.h"
#include "../Systems/SystemPhysics.h"
#include "../Systems/SystemCollision.h"
#include "../Managers/SystemManager.h"

class Entity;


class BallDropScene : public IScene
{
public:


	BallDropScene(Window& p_window, VulkanRHI* rhi, GUI* p_gui);
	~BallDropScene() override;

	void Start(float deltaTime) override;
	void Stop() override;

	void Update(float deltaTime) override;
	void FixedUpdate() override;

	void Draw() override;

	void SerializeState() override;
	void DeserializeState() override;

	void HandleInput(float deltaTime) override;

	void Destroy() override;

private:
	void AddEntity(Entity&& entity) override;
	void RemoveEntity(int index) override;

	void CreateSphere();

	std::vector<Entity> m_entities;
	Camera m_camera;
	InputHandler m_inputHandler;
	Window* m_window;
	VulkanRHI* m_vulkanRHI;

	// temporary for testing, should be owned by system manager, not scene
	SystemManager m_systemManager;

	GUI* m_gui;

	bool m_paused;

	Texture woodTex;
	float m_deltaTime;
	int m_sphereCount = 0;

};