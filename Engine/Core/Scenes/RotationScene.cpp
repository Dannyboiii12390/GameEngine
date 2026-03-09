#include "RotationScene.h"
#include "../Entity.h"
#include "../../Renderer/VulkanRHI.h"
#include "../Managers/ResourceManager.h"

#include <chrono>
#include <iostream>
#include "../../IMGUI/imgui.h"
#include "../../../PhysicsEngine/Maths/CollisionResolution/ConservationOfMomentum.h"
#include "../../../PhysicsEngine/Maths/CollisionResolution/CollisionResolution.h"
#include "../../DebugUtils.h"

# define M_PI           3.14159265358979323846  /* pi */

RotationScene::RotationScene(Window& p_window, VulkanRHI* rhi, GUI* p_gui) :
	m_window(&p_window), m_inputHandler(p_window), m_camera(90, 16.0f / 9.0f, 0.1f, 100.0f), m_vulkanRHI(rhi),
	m_gui(p_gui), m_systemManager(rhi, 2)
{
	m_systemManager.RegisterSystem(std::make_unique<SystemVelocity>());
	m_systemManager.RegisterSystem(std::make_unique<SystemPhysics>());
	m_systemManager.RegisterSystem(std::make_unique<SystemCollision>());

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
					col->SetCollider(std::make_unique<Physics::Sphere>(pos, 0.5f));
					break;
				case Physics::EColliderType::PLANE:
					xf = entity.GetComponent<ComponentTransform>(EComponentType::Component_Transform);
					xf->SetRotation(glm::vec3(-90.0f, 0.0f, 0.0f));
					meshData = ResourceManager::CreatePlaneMesh();
					// u=(1,0,0), v=(0,1,0) -> normal = cross(u,v) = (0,0,1) rotated by -90 X -> (0,1,0) pointing up
					col->SetCollider(std::make_unique<Physics::Plane>(pos, glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f)));
					transform->SetScale(glm::vec3(50.0f));
					break;
				default:
					LOG_DEBUG("unsupported mesh type for entity");
					break;
				}
				auto [verts, indices] = meshData;

				if (!geom->InitializeMesh(m_vulkanRHI, verts, indices))
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
	Entity floorEntity;

	const Texture entTex(m_vulkanRHI, "Assets/red_brick_diff_1k.jpg", TextureType::Albedo, true);
	const Texture woodTex(m_vulkanRHI, "Assets/wood_shutter_diff_1k.jpg", TextureType::Albedo, true);

	createEntity(entity1, glm::vec3(0.5f, -5.0f, 0.0f), entTex);
	createEntity(entity2, glm::vec3(0.0f, 0.0f, 0.0f), woodTex);
	createEntity(floorEntity, glm::vec3(0.0f, -7.5f, 0.0f), woodTex, Physics::EColliderType::PLANE);

	entity2.AddComponent(EComponentType::Component_Physics);
	entity1.AddComponent(EComponentType::Component_Physics);
	ComponentPhysics* phys1 = entity1.GetComponent<ComponentPhysics>(EComponentType::Component_Physics);
	if (phys1)
	{
		phys1->SetAffectedByGravity(false);
		phys1->SetMass(2.0f);

		ComponentCollision* colComp = entity1.GetComponent<ComponentCollision>(EComponentType::Component_Collision);
		if (colComp)
		{
			Physics::Collider* collider = colComp->GetCollider();
			if (collider && collider->getType() == Physics::EColliderType::SPHERE)
			{
				auto* sphere = dynamic_cast<Physics::Sphere*>(collider);
				if (sphere)
				{
					float r = sphere->getRadius();
					float I = 0.4f * phys1->GetMass() * r * r;
					phys1->SetMomentOfInertia(glm::vec3(I, I, I));
				}
			}
		}
	}

	ComponentTransform* xf1 = entity1.GetComponent<ComponentTransform>(EComponentType::Component_Transform);
	xf1->SetScale(glm::vec3(2.0f));
	ComponentCollision* col1 = entity1.GetComponent<ComponentCollision>(EComponentType::Component_Collision);
	Physics::Sphere* sphereColEntity1 = dynamic_cast<Physics::Sphere*>(col1->GetCollider());
	sphereColEntity1->setRadius(1.0f);
	//col1->SetOnCollision(CollisionResponse);

	ComponentCollision* col2 = entity2.GetComponent<ComponentCollision>(EComponentType::Component_Collision);
	//col2->SetOnCollision(CollisionResponse);

	auto* velFloor = floorEntity.GetComponent<ComponentVelocity>(EComponentType::Component_Velocity);
	velFloor->SetRotationalVelocity(glm::vec3(15.0f, 0.0f, 0.0f));

	ComponentTransform* xf_floor = floorEntity.GetComponent<ComponentTransform>(EComponentType::Component_Transform);

	AddEntity(std::move(entity1));
	AddEntity(std::move(entity2));
	AddEntity(std::move(floorEntity));

	m_camera.SetPosition(glm::vec3(5.0f, -1.0f, 5.0f));
	m_camera.LookAt(glm::vec3(0.0f, -5.0f, 0.0f));
}
RotationScene::~RotationScene()
{
	if (m_vulkanRHI)
	{
		m_vulkanRHI->WaitIdle();

		for (auto& entity : m_entities)
			entity.Destroy();

		m_systemManager.Shutdown();
	}
}
void RotationScene::Destroy()
{
	if (m_vulkanRHI)
	{
		m_vulkanRHI->WaitIdle();

		for (auto& entity : m_entities)
			entity.Destroy();

		m_systemManager.Shutdown();
	}
}
void RotationScene::Start(float deltaTime)
{
}
void RotationScene::Stop()
{
}
void RotationScene::Update(float deltaTime)
{
	m_window->PollEvents();

	deltaTime = m_paused ? 0.0f : deltaTime;

	if (!m_paused && m_entities.size() > 0)
	{
		ComponentPhysics* phys = m_entities[0].GetComponent<ComponentPhysics>(EComponentType::Component_Physics);
		if (phys)
		{
			const glm::vec3 continuousTorque(0.0f, 0.0f, 50.0f);
			phys->ApplyTorque(continuousTorque);
		}
	}

	m_systemManager.Update(m_entities, deltaTime);
}
void RotationScene::FixedUpdate()
{
}
void RotationScene::Draw()
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
				ImGui::Begin("About", &show_about);
				auto* col = m_entities[2].GetComponent<ComponentCollision>(EComponentType::Component_Collision);
				auto collider = col->GetCollider();
				auto planeCol = dynamic_cast<Physics::Plane*>(collider);
				ImGui::Text(planeCol->toString().c_str());
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
void RotationScene::HandleInput(float deltaTime)
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
void RotationScene::SerializeState()
{
}
void RotationScene::DeserializeState()
{
}
void RotationScene::AddEntity(Entity&& entity)
{
	m_entities.push_back(std::move(entity));
}
void RotationScene::RemoveEntity(int index)
{
	if (index >= 0 && index < m_entities.size())
		m_entities.erase(m_entities.begin() + index);
}