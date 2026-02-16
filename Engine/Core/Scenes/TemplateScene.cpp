#include "TemplateScene.h"

#include "../Entity.h"
#include "../../Renderer/VulkanRHI.h"
#include "../Managers/ResourceManager.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include "../../IMGUI/imgui.h"


TemplateScene::TemplateScene(Window& p_window, VulkanRHI* rhi, GUI* p_gui) : 
	m_window(&p_window), m_inputHandler(p_window), m_camera(90, 16.0f / 9.0f, 0.1f, 100.0f), m_vulkanRHI(rhi), m_gui(p_gui), m_velocitySystem(m_entities), m_physicsSystem(m_entities)
{
	// Initialize VulkanRHI and Renderer here if needed
	m_renderer.Initialize(m_vulkanRHI);
	m_vulkanRHI->SetActiveCamera(&m_camera);

	//temporary entity creation for testing, should be done in a scene setup function or via a scene editor in the future
	auto createEntity = [this](Entity& entity, glm::vec3 pos)
	{
		static int entityCount = 0;

		entity.AddComponent(EComponentType::Component_Translation, pos, glm::vec3(0.0f), glm::vec3(1.0f));
		entity.AddComponent(EComponentType::Component_Velocity, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f), glm::vec3(0.0f));
		entity.AddComponent(EComponentType::Component_Geometry);

		ComponentGeometry* geom = entity.GetComponent<ComponentGeometry>(EComponentType::Component_Geometry);
		if (geom)
		{
			auto [verts, indices] = entityCount ? ResourceManager::CreateCubeMesh() : ResourceManager::CreatePlaneMesh();

			if(!geom->InitializeMesh(m_vulkanRHI, verts, indices))
				throw std::runtime_error("Failed to initialize triangle mesh");

			const std::string vertSpv = "SHADERS/triangle.vert.spv";
			const std::string fragSpv = "SHADERS/triangle.frag.spv";

			if (!geom->InitializePipeline(m_vulkanRHI, m_vulkanRHI->GetRenderPass(), m_vulkanRHI->GetSwapchainExtent(), vertSpv, fragSpv))
				throw std::runtime_error("Failed to create triangle pipeline");

			const std::string texturePath = "red_brick_diff_1k.jpg";

			if (!geom->CreateTexture(m_vulkanRHI, texturePath, TextureType::Albedo, true));
		}
		entityCount++;
	};

	Entity entity1;
	Entity entity2;

	createEntity(entity1, glm::vec3(-1.0f));
	createEntity(entity2, glm::vec3(1.0f, 0.0f, 0.0f));

	entity2.AddComponent(EComponentType::Component_Physics);

	AddEntity(std::move(entity1));
	AddEntity(std::move(entity2));

	m_camera.SetPosition(glm::vec3(2.0f));
	m_camera.LookAt(glm::vec3(0.0f, 0.0f, 0.0f));

}
TemplateScene::~TemplateScene()
{
	if (m_vulkanRHI)
	{
		m_vulkanRHI->WaitIdle(); // ensure device idle before destroying GUI resources

		for(auto& entity : m_entities)
		{
			auto geom = entity.GetComponent<ComponentGeometry>(EComponentType::Component_Geometry);
			if (geom) geom->Destroy(); // ensure mesh & pipeline free their VkBuffers/VkPipeline while device is still valid and idle
		}

		m_renderer.Shutdown();
	}

}
void TemplateScene::Start(float deltaTime)
{
	// Example: start a timer, play music, trigger an animation.
}
void TemplateScene::Stop()
{
	// Example: pause a timer, stop music, pause an animation.
}
void TemplateScene::Update(float deltaTime)
{
	m_window->PollEvents();

	if (m_entities[0].HasComponent(EComponentType::Component_Translation))
	{
		auto* xf = m_entities[0].GetComponent<ComponentTranslation>(EComponentType::Component_Translation);
		if (xf)
		{
			glm::vec3 rot = xf->Rotation();
			rot.z += 90.0f * deltaTime; // rotate 90 deg/sec around Z
			xf->SetRotation(rot);
		}
	}
	m_physicsSystem.OnUpdate(deltaTime);
	m_velocitySystem.OnUpdate(deltaTime);
}
void TemplateScene::FixedUpdate()
{
	// Example: physics updates at fixed timestep.
}
void TemplateScene::Draw()
{
	m_vulkanRHI->BeginFrame();

	// Record draw commands into the currently-acquired command buffer
	VkCommandBuffer cmd = m_vulkanRHI->GetCurrentCommandBuffer();
	if (cmd != VK_NULL_HANDLE)
	{
		m_renderer.Render(cmd, m_entities);


		if (m_gui)
		{
			m_gui->NewFrame();
			ImGui::Begin("Hello, ImGui!");
			ImGui::Text("This is a simple GUI overlay.");
			ImGui::End();
			m_gui->Render(cmd);
		}
	}
	m_vulkanRHI->EndFrame();
	m_vulkanRHI->Present();
}
void TemplateScene::HandleInput(float deltaTime) {

	if (m_inputHandler.isKeyPressed(GLFW_KEY_ESCAPE)) glfwSetWindowShouldClose(m_window->getGLFWwindow(), true);

	const float cameraMoveSpeed = 1.0f;
	if (m_inputHandler.isKeyHeld(GLFW_KEY_W)) m_camera.Translate(m_camera.Forward() * cameraMoveSpeed * deltaTime);
	if (m_inputHandler.isKeyHeld(GLFW_KEY_S)) m_camera.Translate(-m_camera.Forward() * cameraMoveSpeed * deltaTime);
	if (m_inputHandler.isKeyHeld(GLFW_KEY_A)) m_camera.Translate(-m_camera.Right() * cameraMoveSpeed * deltaTime);
	if (m_inputHandler.isKeyHeld(GLFW_KEY_D)) m_camera.Translate(m_camera.Right() * cameraMoveSpeed * deltaTime);
	m_vulkanRHI->SetActiveCamera(&m_camera);
}
void TemplateScene::SerializeState()
{
	// Example: serialize entity states to a file.
}
void TemplateScene::DeserializeState()
{
	// Example: deserialize entity states from a file.
}

void TemplateScene::AddEntity(Entity&& entity) {
	m_entities.push_back(std::move(entity));
	m_velocitySystem = SystemVelocity(m_entities);
	m_physicsSystem = SystemPhysics(m_entities);
}
void TemplateScene::RemoveEntity(int index) {
	if (index >= 0 && index < m_entities.size())
		m_entities.erase(m_entities.begin() + index);
}