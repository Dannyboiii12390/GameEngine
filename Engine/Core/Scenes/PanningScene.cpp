#include "PanningScene.h"
#include "../Managers/ResourceManager.h"
#include "../../IMGUI/imgui.h"
#include "../Systems/SystemSwapBuffers.h"

#include <fstream>
#include "../Systems/SystemVelocity.h"
#include "../Systems/SystemPhysics.h"
#include "../Systems/SystemCollision.h"

#include "../../Renderer/Window.h"


PanningScene::PanningScene(Window& p_window, VulkanRHI* rhi, GUI* p_gui) :
	m_window(&p_window), m_inputHandler(p_window), m_camera(90, 16.0f / 9.0f, 0.1f, 100.0f), m_vulkanRHI(rhi),
	m_gui(p_gui), m_systemManager(rhi, 2)
{

	// Camera, vulkan, and imgui Initialisation
	m_vulkanRHI->SetActiveCamera(&m_camera);
	m_gui->Create(*rhi, *m_window);
	m_camera.SetPosition(glm::vec3(-5.0f, 5.0f, 0.0f));
	m_camera.LookAt(glm::vec3(0.0f, 0.0f, 0.0f));
	m_camera.SetNearFar(0.1f, 10000.0f);

	// Load textures
	const Texture red_brick(m_vulkanRHI, "Assets/red_brick_diff_1k.jpg", TextureType::Albedo, true);
	//const Texture mossy_cobblestone(m_vulkanRHI, "Assets/mossy_cobblestone_diff_1k.jpg", TextureType::Albedo, true);

	// Add Entities
	Entity planeEntity;
	planeEntity.AddComponent(EComponentType::Component_Transform, glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(-90.0f, 0.0f, 0.0f), glm::vec3(1000.0f));
	planeEntity.AddComponent(EComponentType::Component_Geometry);
	planeEntity.AddComponent(EComponentType::Component_Collision);
	ComponentGeometry* geom = planeEntity.GetComponent<ComponentGeometry>(EComponentType::Component_Geometry);
	if (geom)
	{
		const float meshWidth = 1.0f;
		const glm::vec3 entityScale = glm::vec3(1000.0f); // you used glm::vec3(10.0f,...)
		const float tileSizeWorld = 1.0f; // make 1.0 world unit per tile

		float uvRepeats = (meshWidth * entityScale.x) / tileSizeWorld;

		MeshData meshData = ResourceManager::CreatePlaneMesh(uvRepeats, meshWidth, /*height=*/1.0f);
		auto [verts, indices] = meshData;
		geom->InitializeMesh(m_vulkanRHI, verts, indices);
		geom->InitializePipeline(m_vulkanRHI, m_vulkanRHI->GetRenderPass(), m_vulkanRHI->GetSwapchainExtent(), "SHADERS/object.vert.spv", "SHADERS/object.frag.spv");
		geom->AddTexture(m_vulkanRHI, red_brick);
	}
	ComponentCollision* col = planeEntity.GetComponent<ComponentCollision>(EComponentType::Component_Collision);
	if (col)
	{
		col->SetCollider(std::make_unique<Physics::Plane>(glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f)));
	}


	m_entities.push_back(std::move(planeEntity));
	// Add Systems
	m_systemManager.RegisterSystem(std::make_unique<SystemVelocity>());
	m_systemManager.RegisterSystem(std::make_unique<SystemPhysics>());
	m_systemManager.RegisterSystem(std::make_unique<SystemCollision>(m_entities.size(), m_vulkanRHI));
	m_systemManager.RegisterSystem(std::make_unique<SystemSwapBuffers>());



	m_paused = false;
}
PanningScene::~PanningScene()
{
	if (m_vulkanRHI)
	{
		m_vulkanRHI->WaitIdle();

		for (auto& entity : m_entities)
			entity.Destroy();

		m_systemManager.Shutdown();
	}
}
void PanningScene::Destroy()
{
	if (m_vulkanRHI)
	{
		m_vulkanRHI->WaitIdle();

		for (auto& entity : m_entities)
			entity.Destroy();

		m_systemManager.Shutdown();
	}
}
void PanningScene::Start(float deltaTime)
{
}
void PanningScene::Stop()
{
}
void PanningScene::Update(float deltaTime)
{
	m_window->PollEvents();

	m_deltaTime = deltaTime;

	deltaTime = m_paused ? 0.0f : deltaTime;

	m_systemManager.Update(m_entities, deltaTime);

}
void PanningScene::FixedUpdate()
{
}
void PanningScene::Draw()
{
	m_vulkanRHI->BeginFrame();

	VkCommandBuffer cmd = m_vulkanRHI->GetCurrentCommandBuffer();
	if (cmd != VK_NULL_HANDLE)
	{
		m_systemManager.Render(cmd, m_entities);

		if (m_gui)
		{
			m_gui->NewFrame();

			static bool show_demo_window = false;
			static bool show_about = true;

			if (ImGui::BeginMainMenuBar())
			{
				if (ImGui::BeginMenu("File"))
				{
					if (ImGui::MenuItem("Exit", "Esc"))
						glfwSetWindowShouldClose(m_window->getGLFWwindow(), true);
					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("View"))
				{
					ImGui::MenuItem("Show ImGui Demo", nullptr, &show_demo_window);
					ImGui::MenuItem("About", nullptr, &show_about);
					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("Help"))
				{
					if (ImGui::MenuItem("Open Documentation"))
					{
					}
					ImGui::EndMenu();
				}

				ImGui::EndMainMenuBar();
			}

			if (show_demo_window)
				ImGui::ShowDemoWindow(&show_demo_window);

			if (show_about)
			{
				ImGui::Begin("Debug Diagnostics");
				ImGui::Text("FPS: %.1f", 1.0f / m_deltaTime);
				ImGui::Text("Sphere Count: %d", m_entities.size());
				if (ImGui::Button("Start/Stop Simulation"))
					m_paused = !m_paused;
				ImGui::End();
			}

			m_gui->Render(cmd);
		}
	}
	m_vulkanRHI->EndFrame();
	m_vulkanRHI->Present();
}
void PanningScene::HandleInput(float deltaTime)
{
	if (m_inputHandler.isKeyPressed(GLFW_KEY_ESCAPE)) glfwSetWindowShouldClose(m_window->getGLFWwindow(), true);

	float cameraMoveSpeed = 5.0f;
	if (m_inputHandler.isKeyHeld(GLFW_KEY_LEFT_SHIFT)) cameraMoveSpeed *= 2.0f;

	if (m_inputHandler.isKeyHeld(GLFW_KEY_W)) m_camera.Translate(m_camera.Forward() * cameraMoveSpeed * deltaTime);
	if (m_inputHandler.isKeyHeld(GLFW_KEY_S)) m_camera.Translate(-m_camera.Forward() * cameraMoveSpeed * deltaTime);
	if (m_inputHandler.isKeyHeld(GLFW_KEY_A)) m_camera.Translate(-m_camera.Right() * cameraMoveSpeed * deltaTime);
	if (m_inputHandler.isKeyHeld(GLFW_KEY_D)) m_camera.Translate(m_camera.Right() * cameraMoveSpeed * deltaTime);
	if (m_inputHandler.isKeyHeld(GLFW_KEY_LEFT_CONTROL)) m_camera.Translate(-m_camera.Up() * cameraMoveSpeed * deltaTime);
	if (m_inputHandler.isKeyHeld(GLFW_KEY_SPACE)) m_camera.Translate(m_camera.Up() * cameraMoveSpeed * deltaTime);

	if (m_inputHandler.isKeyHeld(GLFW_KEY_J)) m_camera.Rotate(glm::vec3(0.0f, -90.0f * deltaTime, 0.0f));
	if (m_inputHandler.isKeyHeld(GLFW_KEY_L)) m_camera.Rotate(glm::vec3(0.0f, 90.0f * deltaTime, 0.0f));
	if (m_inputHandler.isKeyHeld(GLFW_KEY_K)) m_camera.Rotate(glm::vec3(-90.0f * deltaTime, 0.0f, 0.0f));
	if (m_inputHandler.isKeyHeld(GLFW_KEY_I)) m_camera.Rotate(glm::vec3(90.0f * deltaTime, 0.0f, 0.0f));

	m_vulkanRHI->SetActiveCamera(&m_camera);
}
void PanningScene::SerializeState()
{
}
void PanningScene::DeserializeState()
{
}
void PanningScene::AddEntity(Entity&& entity)
{
	m_entities.push_back(std::move(entity));
}
void PanningScene::RemoveEntity(int index)
{
	if (index >= 0 && index < m_entities.size())
		m_entities.erase(m_entities.begin() + index);
}