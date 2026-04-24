#include "PanningScene.h"
#include "../Managers/ResourceManager.h"
#include "../../IMGUI/imgui.h"
#include "../Systems/SystemSwapBuffers.h"

#include <fstream>
#include "../Systems/SystemVelocity.h"
#include "../Systems/SystemPhysics.h"
#include "../Systems/SystemCollision.h"

#include "../../Renderer/Window.h"
#include <fstream>
#include "../Managers/SceneManager.h"
#include "BallDropScene.h"
#include "TemplateScene.h"


PanningScene::PanningScene(Window& p_window, VulkanRHI* rhi, GUI* p_gui) :
	m_window(&p_window), m_inputHandler(p_window), m_camera(90, 16.0f / 9.0f, 0.1f, 100.0f), m_vulkanRHI(rhi),
	m_gui(p_gui), m_systemManager(rhi, 2)
{

	// Camera, vulkan, and imgui Initialisation
	m_vulkanRHI->SetActiveCamera(&m_camera);
	
	//read camera position and rotation from file if it exists, otherwise use defaults
	std::ifstream file("camera_state.txt");
	if(file.is_open())
	{
		glm::vec3 pos, rot;
		file >> pos.x >> pos.y >> pos.z;
		file >> rot.x >> rot.y >> rot.z;
		m_camera.SetPosition(pos);
		m_camera.SetRotation(rot);
		file.close();
	}
	else
	{
		m_camera.SetPosition(glm::vec3(-5.0f, 5.0f, 0.0f));
		m_camera.LookAt(glm::vec3(0.0f, 0.0f, 0.0f));
	}
	
	m_camera.SetNearFar(0.1f, 1000.0f);

	// Load textures
	const Texture red_brick(m_vulkanRHI, "Assets/red_brick_diff_1k.jpg", TextureType::Albedo, true);
	const Texture mossy_cobblestone(m_vulkanRHI, "Assets/mossy_cobblestone_diff_1k.jpg", TextureType::Albedo, true);

	// Add Entities
	{
		Entity planeEntity;
		planeEntity.AddComponent(EComponentType::Component_Transform, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-90.0f, 0.0f, 0.0f), glm::vec3(100.0f));
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
	}
	auto createSpheres = [this, &mossy_cobblestone](glm::vec3 pos)
		{
			Entity sphereEntity;
			sphereEntity.AddComponent(EComponentType::Component_Transform, pos, glm::vec3(0.0f), glm::vec3(1.0f));
			sphereEntity.AddComponent(EComponentType::Component_Velocity, glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(0.0f));
			sphereEntity.AddComponent(EComponentType::Component_Geometry);
			sphereEntity.AddComponent(EComponentType::Component_Physics);
			sphereEntity.AddComponent(EComponentType::Component_Collision);
			ComponentGeometry* geom = sphereEntity.GetComponent<ComponentGeometry>(EComponentType::Component_Geometry);
			if (geom)
			{
				MeshData meshData = ResourceManager::CreateSphereMesh(1.0f, 16, 16);
				auto [verts, indices] = meshData;
				geom->InitializeMesh(m_vulkanRHI, verts, indices);
				geom->InitializePipeline(m_vulkanRHI, m_vulkanRHI->GetRenderPass(), m_vulkanRHI->GetSwapchainExtent(), "SHADERS/object.vert.spv", "SHADERS/object.frag.spv");
				geom->AddTexture(m_vulkanRHI, mossy_cobblestone);
			}
			ComponentCollision* col = sphereEntity.GetComponent<ComponentCollision>(EComponentType::Component_Collision);
			if (col)
			{
				col->SetCollider(std::make_unique<Physics::Sphere>(pos, 0.5f));
			}
			m_entities.push_back(std::move(sphereEntity));
		};
	for(int i = 0; i < 1000; ++i)
	{
		//random position in a 100x100 area above the plane
		float x = static_cast<float>(rand()) / RAND_MAX * 100.0f - 50.0f;
		float y = static_cast<float>(rand()) / RAND_MAX * 50.0f + 5.0f;
		float z = static_cast<float>(rand()) / RAND_MAX * 100.0f - 50.0f;

		createSpheres(glm::vec3(x, y, z));
	}


	// Add Systems
	m_systemManager.RegisterSystem(std::make_unique<SystemVelocity>());
	m_systemManager.RegisterSystem(std::make_unique<SystemPhysics>());
	m_systemManager.RegisterSystem(std::make_unique<SystemCollision>(m_entities.size(), m_vulkanRHI));
	m_systemManager.RegisterSystem(std::make_unique<SystemSwapBuffers>());



	m_paused = false;
}
PanningScene::~PanningScene()
{
	if (m_vulkanRHI && m_vulkanRHI->GetActiveCamera() == &m_camera)
	{
		m_vulkanRHI->SetActiveCamera(nullptr);
	}

	// save camera position and rotation to file for next time
	auto file = std::ofstream("camera_state.txt");
	if(file.is_open())
	{
		glm::vec3 pos = m_camera.GetPosition();
		glm::vec3 rot = m_camera.GetRotation();
		file << pos.x << " " << pos.y << " " << pos.z << std::endl;
		file << rot.x << " " << rot.y << " " << rot.z << std::endl;
		file.close();
	}


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
			m_gui->NewFrame(*m_window);

			static bool show_demo_window = false;
			static bool show_about = true;

			// Build main menu bar only (no other windows inside it)
			if (ImGui::BeginMainMenuBar())
			{
				if (ImGui::BeginMenu("Scenes"))
				{
					if (ImGui::MenuItem("Panning"))
					{
						SceneManager::Instance().RequestReplaceScene(std::make_unique<PanningScene>(*m_window, m_vulkanRHI, m_gui));
					}
					if (ImGui::MenuItem("Ball Drop"))
					{
						SceneManager::Instance().RequestReplaceScene(std::make_unique<BallDropScene>(*m_window, m_vulkanRHI, m_gui));
					}
					if (ImGui::MenuItem("Template"))
					{
						SceneManager::Instance().RequestReplaceScene(std::make_unique<TemplateScene>(*m_window, m_vulkanRHI, m_gui));
					}
					ImGui::EndMenu();
				}

				ImGui::EndMainMenuBar();
			}

			// Other windows must be created after closing the menu bar
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

			// Render ImGui once per frame after all ImGui calls
			m_gui->Render(cmd);
		}
		m_vulkanRHI->EndFrame();
		m_vulkanRHI->Present();
	}
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