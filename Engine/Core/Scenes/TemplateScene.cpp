#include "TemplateScene.h"

#include "../Entity.h"
#include "../../Renderer/VulkanRHI.h"
#include "../Managers/ResourceManager.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include "../../IMGUI/imgui.h"
#include "../../../PhysicsEngine/Maths/CollisionResolution/ConservationOfMomentum.h"
#include "../../../PhysicsEngine/Maths/CollisionResolution/CollisionResolution.h"


TemplateScene::TemplateScene(Window& p_window, VulkanRHI* rhi, GUI* p_gui) : 
	m_window(&p_window), m_inputHandler(p_window), m_camera(90, 16.0f / 9.0f, 0.1f, 100.0f), m_vulkanRHI(rhi),
	m_gui(p_gui), m_collisionSystem(50, rhi)
{
	// Initialize VulkanRHI and Renderer here if needed
	m_renderer.Initialize(m_vulkanRHI);
	m_vulkanRHI->SetActiveCamera(&m_camera);

	m_gui->Create(*rhi, *m_window);

	//temporary entity creation for testing, should be done in a scene setup function or via a scene editor in the future
	auto createEntity = [this](Entity& entity, glm::vec3 pos, const Texture& entTex, Physics::EColliderType type = Physics::EColliderType::SPHERE)
	{
		static int entityCount = 0;

		entity.AddComponent(EComponentType::Component_Transform, pos, glm::vec3(0.0f), glm::vec3(1.0f));
		entity.AddComponent(EComponentType::Component_Velocity, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f), glm::vec3(0.0f));
		entity.AddComponent(EComponentType::Component_Geometry);
		entity.AddComponent(EComponentType::Component_Collision);

		ComponentGeometry* geom = entity.GetComponent<ComponentGeometry>(EComponentType::Component_Geometry);
		if (geom)
		{
			MeshData meshData;
			ComponentCollision* col = entity.GetComponent<ComponentCollision>(EComponentType::Component_Collision);
			ComponentTransform* xf;
			auto* transform = entity.GetComponent<ComponentTransform>(EComponentType::Component_Transform);
			switch (type)
			{
				case Physics::EColliderType::SPHERE:
					meshData = ResourceManager::CreateSphereMesh(16, 16);
					col->SetCollider(std::make_unique<Physics::Sphere>(pos, 0.5f)); // radius 0.5 to match unit sphere mesh scaled by 0.5 in translation component
					break;
				case Physics::EColliderType::PLANE:
					xf = entity.GetComponent<ComponentTransform>(EComponentType::Component_Transform);
					xf->SetRotation(glm::vec3(-90.0f, 0.0f, 0.0f)); // rotate plane to be horizontal
					meshData = ResourceManager::CreatePlaneMesh();
					col->SetCollider(std::make_unique<Physics::Plane>(pos, glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)));
					transform->SetScale(glm::vec3(10.0f));

					break;
				default:
					std::cout << "unsupported mesh type for entity" << std::endl;
					break;
			}
			auto [verts, indices] = meshData;

			if(!geom->InitializeMesh(m_vulkanRHI, verts, indices))
				throw std::runtime_error("Failed to initialize triangle mesh");

			const std::string vertSpv = "SHADERS/object.vert.spv";
			const std::string fragSpv = "SHADERS/object.frag.spv";

			if (!geom->InitializePipeline(m_vulkanRHI, m_vulkanRHI->GetRenderPass(), m_vulkanRHI->GetSwapchainExtent(), vertSpv, fragSpv))
				throw std::runtime_error("Failed to create triangle pipeline");

			if (!geom->AddTexture(m_vulkanRHI, entTex))
				throw std::runtime_error("Failed to add texture");
		}
		entityCount++;
	};

	Entity entity1;
	Entity entity2;

	const Texture entTex(m_vulkanRHI, "Assets/red_brick_diff_1k.jpg", TextureType::Albedo, true);
	const Texture woodTex(m_vulkanRHI, "Assets/wood_shutter_diff_1k.jpg", TextureType::Albedo, true);

	createEntity(entity1, glm::vec3(0.5f, -5.0f, 0.0f), entTex, Physics::EColliderType::PLANE);
	createEntity(entity2, glm::vec3(0.0f, 0.0f, 0.0f), woodTex);

	entity2.AddComponent(EComponentType::Component_Physics);

	ComponentCollision* col2 = entity2.GetComponent<ComponentCollision>(EComponentType::Component_Collision);

	col2->SetOnCollision(CollisionResponse);

	AddEntity(std::move(entity1));
	AddEntity(std::move(entity2));

	m_camera.SetPosition(glm::vec3(5.0f, -1.0f, 5.0f));
	m_camera.LookAt(glm::vec3(0.0f, -5.0f, 0.0f));
}
TemplateScene::~TemplateScene()
{
	if (m_vulkanRHI)
	{
		m_vulkanRHI->WaitIdle(); // ensure device idle before destroying GUI resources

		for(auto& entity : m_entities)
		{
			entity.Destroy();
		}

		m_renderer.Shutdown();
	}
}
void TemplateScene::Destroy()
{
	if (m_vulkanRHI)
	{
		m_vulkanRHI->WaitIdle(); // ensure device idle before destroying GUI resources

		for (auto& entity : m_entities)
		{
			entity.Destroy();
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

	//if (m_entities[0].HasComponent(EComponentType::Component_Transform))
	//{
	//	auto* xf = m_entities[0].GetComponent<ComponentTransform>(EComponentType::Component_Transform);
	//	if (xf)
	//	{
	//		glm::vec3 rot = xf->Rotation();
	//		rot.z += 90.0f * deltaTime; // rotate 90 deg/sec around Z
	//		xf->SetRotation(rot);
	//	}
	//}

	deltaTime = m_paused ? 0.0f : deltaTime;


	m_physicsSystem.OnUpdate(m_entities, deltaTime);
	m_velocitySystem.OnUpdate(m_entities, deltaTime);
	m_collisionSystem.OnUpdate(m_entities, deltaTime);

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

			// Simple main menu bar with File and View menus
			static bool show_demo_window = false;
			static bool show_about = true;

			if (ImGui::BeginMainMenuBar())
			{
				if (ImGui::BeginMenu("File"))
				{
					if (ImGui::MenuItem("Exit", "Esc"))
					{
						glfwSetWindowShouldClose(m_window->getGLFWwindow(), true);
					}
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
						// TODO: open docs or trigger action
					}
					ImGui::EndMenu();
				}

				ImGui::EndMainMenuBar();
			}

			// Optional windows driven by menu toggles
			if (show_demo_window)
				ImGui::ShowDemoWindow(&show_demo_window);

			if (show_about)
			{
				ImGui::Begin("About", &show_about);
				ImGui::Text("GameEngine - ImGui Menu Bar Example");
				ImGui::Text("Press Esc or use File -> Exit to quit.");
				if (ImGui::Button("Start/Stop Simulation"))
				{
					m_paused = !m_paused;
				}
				ImGui::End();
			}

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
}
void TemplateScene::RemoveEntity(int index) {
	if (index >= 0 && index < m_entities.size())
		m_entities.erase(m_entities.begin() + index);
}