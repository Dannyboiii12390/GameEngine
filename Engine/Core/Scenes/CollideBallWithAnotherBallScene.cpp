#include "CollideBallWithAnotherBallScene.h"
#include "../Entity.h"
#include "../../Renderer/VulkanRHI.h"
#include "../Managers/ResourceManager.h"

#include <chrono>
#include <iostream>
#include "../../IMGUI/imgui.h"
#include "../../../PhysicsEngine/Maths/CollisionResolution/ConservationOfMomentum.h"
#include "../../../PhysicsEngine/Maths/CollisionResolution/CollisionResolution.h"
#include "../../DebugUtils.h"

static void CollisionResponse(Entity& self, Entity& other)
{
	LOG_DEBUG("Collision detected between entities!");

	auto* selfColComp = self.GetComponent<ComponentCollision>(EComponentType::Component_Collision);
	auto* col = selfColComp->GetCollider();
	if (col->getType() != Physics::EColliderType::SPHERE)
		return;

	// Gather collider pointer for other entity
	Physics::Collider* otherCollider = nullptr;
	if (other.HasComponent(EComponentType::Component_Collision))
	{
		ComponentCollision* otherColComp = other.GetComponent<ComponentCollision>(EComponentType::Component_Collision);
		if (otherColComp)
			otherCollider = otherColComp->GetCollider();
	}
	if (!otherCollider)
		return;

	// Required components on self to update velocity/position
	ComponentTranslation* transSelf = self.GetComponent<ComponentTranslation>(EComponentType::Component_Translation);
	ComponentVelocity* velSelf = self.GetComponent<ComponentVelocity>(EComponentType::Component_Velocity);
	ComponentPhysics* physSelf = self.GetComponent<ComponentPhysics>(EComponentType::Component_Physics);

	if (!transSelf || !velSelf)
		return;

	// Current data for self
	glm::vec3 posSelf = transSelf->Position();
	glm::vec3 vSelf = velSelf->GetPositionVelocity();
	float massSelf = (physSelf) ? physSelf->GetMass() : 1.0f;
	float invMassSelf = (physSelf) ? physSelf->GetInverseMass() : 1.0f; // if no physics component, treat as mass 1

	// compute contact normal from other -> points from other surface toward sphere center
	glm::vec3 contactNormal(0.0f);
	const float EPS = 1e-6f;

	if (otherCollider->getType() == Physics::EColliderType::PLANE)
	{
		auto* plane = dynamic_cast<Physics::Plane*>(otherCollider);
		if (!plane) return;

		glm::vec3 proj = plane->projectPoint(posSelf);
		glm::vec3 dir = posSelf - proj;
		float dist = glm::length(dir);
		if (dist > EPS)
			contactNormal = dir / dist;
		else
		{
			// Degenerate: choose plane normal direction using signed distance sign
			float signedD = plane->signedDistance(posSelf);
			contactNormal = (std::abs(signedD) > EPS) ? glm::normalize(posSelf - proj) : glm::vec3(0.0f, 1.0f, 0.0f);
		}
	}
	else if (otherCollider->getType() == Physics::EColliderType::SPHERE)
	{
		auto* otherSphere = dynamic_cast<Physics::Sphere*>(otherCollider);
		if (!otherSphere) return;
		glm::vec3 dir = posSelf - otherSphere->getPos();
		float len = glm::length(dir);
		contactNormal = (len > EPS) ? (dir / len) : glm::vec3(0.0f, 1.0f, 0.0f);
	}
	else if (otherCollider->getType() == Physics::EColliderType::CYLINDER)
	{
		auto* cyl = dynamic_cast<Physics::Cylinder*>(otherCollider);
		if (!cyl) return;

		glm::vec3 A = cyl->getA();
		glm::vec3 B = cyl->getB();
		glm::vec3 AB = B - A;
		float L = glm::length(AB);
		if (L <= EPS)
		{
			// degenerate -> radial from A
			glm::vec3 diff = posSelf - A;
			float dlen = glm::length(diff);
			contactNormal = (dlen > EPS) ? diff / dlen : glm::vec3(0.0f, 1.0f, 0.0f);
		}
		else
		{
			glm::vec3 u = AB / L;
			float t_raw = glm::dot(posSelf - A, u);
			float t_clamped = std::clamp(t_raw, 0.0f, L);
			glm::vec3 closest = A + u * t_clamped;
			glm::vec3 diff = posSelf - closest;
			float dlen = glm::length(diff);
			contactNormal = (dlen > EPS) ? diff / dlen : glm::vec3(0.0f, 1.0f, 0.0f);
		}
	}
	else
	{
		// Not handled
		return;
	}

	// If the other entity participates in physics with non-zero inverse mass -> two-body resolution
	ComponentPhysics* physOther = other.GetComponent<ComponentPhysics>(EComponentType::Component_Physics);
	ComponentVelocity* velOther = other.GetComponent<ComponentVelocity>(EComponentType::Component_Velocity);

	bool otherHasFiniteMass = (physOther && physOther->GetInverseMass() > 0.0f);
	if (otherHasFiniteMass)
	{
		float massOther = physOther->GetMass();
		glm::vec3 vOther = (velOther) ? velOther->GetPositionVelocity() : glm::vec3(0.0f);

		// Compute elastic collision along the collision normal only:
		// Project velocities onto the normal and tangential components remain unchanged.
		glm::vec3 n = contactNormal;
		glm::vec3 u1n = glm::dot(vSelf, n) * n;
		glm::vec3 u1t = vSelf - u1n;
		glm::vec3 u2n = glm::dot(vOther, n) * n;
		glm::vec3 u2t = vOther - u2n;

		// Solve 1D elastic collision for normal components
		auto [v1_after, v2_after] = Physics::ElasticCollision(u1n, u2n, massSelf, massOther);

		glm::vec3 newV1 = v1_after + u1t;
		glm::vec3 newV2 = v2_after + u2t;

		// Apply velocities back
		velSelf->SetPositionalVelocity(newV1);
		if (velOther)
			velOther->SetPositionalVelocity(newV2);
	}
	else
	{
		// Treat other as fixed/infinite mass. Use resolver with restitution.
		// Determine restitution: prefer a restitution stored in physics component if present.
		float restitution = 1.0f; // default perfectly elastic
		// If self has a physics component we might expose restitution later; for now use 1.0
		glm::vec3 newV = Physics::ResolveVelocityAgainstFixedObject(vSelf, restitution, contactNormal);
		velSelf->SetPositionalVelocity(newV);

		// Optional: positional fixup (push sphere out of penetration) could be added here.
	}

	// Clear accumulated physics forces for self to avoid immediate re-penetration due to stored impulses
	if (physSelf)
	{
		physSelf->ClearForces();
	}
}


CollideBallWithAnotherBallScene::CollideBallWithAnotherBallScene(Window& p_window, VulkanRHI* rhi, GUI* p_gui) :
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

			entity.AddComponent(EComponentType::Component_Translation, pos, glm::vec3(0.0f), glm::vec3(1.0f));
			entity.AddComponent(EComponentType::Component_Velocity, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f), glm::vec3(0.0f));
			entity.AddComponent(EComponentType::Component_Geometry);
			entity.AddComponent(EComponentType::Component_Collision);

			ComponentGeometry* geom = entity.GetComponent<ComponentGeometry>(EComponentType::Component_Geometry);
			if (geom)
			{
				MeshData meshData;
				ComponentCollision* col = entity.GetComponent<ComponentCollision>(EComponentType::Component_Collision);
				ComponentTranslation* xf;
				auto* transform = entity.GetComponent<ComponentTranslation>(EComponentType::Component_Translation);
				switch (type)
				{
				case Physics::EColliderType::SPHERE:
					meshData = ResourceManager::CreateSphereMesh(16, 16);
					col->SetCollider(std::make_unique<Physics::Sphere>(pos, 0.5f)); // radius 0.5 to match unit sphere mesh scaled by 0.5 in translation component
					break;
				case Physics::EColliderType::PLANE:
					xf = entity.GetComponent<ComponentTranslation>(EComponentType::Component_Translation);
					xf->SetRotation(glm::vec3(-90.0f, 0.0f, 0.0f)); // rotate plane to be horizontal
					meshData = ResourceManager::CreatePlaneMesh();
					col->SetCollider(std::make_unique<Physics::Plane>(pos, glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)));
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
	}
	
	ComponentTranslation* xf1 = entity1.GetComponent<ComponentTranslation>(EComponentType::Component_Translation);
	xf1->SetScale(glm::vec3(2.0f));
	ComponentCollision* col1 = entity1.GetComponent<ComponentCollision>(EComponentType::Component_Collision);
	Physics::Sphere* sphereColEntity1 = dynamic_cast<Physics::Sphere*>(col1->GetCollider());
	sphereColEntity1->setRadius(1.0f);
	col1->SetOnCollision(CollisionResponse);

	ComponentCollision* col2 = entity2.GetComponent<ComponentCollision>(EComponentType::Component_Collision);
	col2->SetOnCollision(CollisionResponse);

	ComponentTranslation* xf_floor = floorEntity.GetComponent<ComponentTranslation>(EComponentType::Component_Translation);

	AddEntity(std::move(entity1));
	AddEntity(std::move(entity2));
	AddEntity(std::move(floorEntity));

	m_camera.SetPosition(glm::vec3(5.0f, -1.0f, 5.0f));
	m_camera.LookAt(glm::vec3(0.0f, -5.0f, 0.0f));
}
CollideBallWithAnotherBallScene::~CollideBallWithAnotherBallScene()
{
	if (m_vulkanRHI)
	{
		m_vulkanRHI->WaitIdle(); // ensure device idle before destroying GUI resources

		for (auto& entity : m_entities)
		{
			entity.Destroy();
		}

		m_systemManager.Shutdown();
	}
}
void CollideBallWithAnotherBallScene::Destroy()
{
	if (m_vulkanRHI)
	{
		m_vulkanRHI->WaitIdle(); // ensure device idle before destroying GUI resources

		for (auto& entity : m_entities)
		{
			entity.Destroy();
		}

		m_systemManager.Shutdown();
	}
}
void CollideBallWithAnotherBallScene::Start(float deltaTime)
{
	// Example: start a timer, play music, trigger an animation.
}
void CollideBallWithAnotherBallScene::Stop()
{
	// Example: pause a timer, stop music, pause an animation.
}
void CollideBallWithAnotherBallScene::Update(float deltaTime)
{
	m_window->PollEvents();

	//if (m_entities[0].HasComponent(EComponentType::Component_Translation))
	//{
	//	auto* xf = m_entities[0].GetComponent<ComponentTranslation>(EComponentType::Component_Translation);
	//	if (xf)
	//	{
	//		glm::vec3 rot = xf->Rotation();
	//		rot.z += 90.0f * deltaTime; // rotate 90 deg/sec around Z
	//		xf->SetRotation(rot);
	//	}
	//}

	deltaTime = m_paused ? 0.0f : deltaTime;


	m_systemManager.Update(m_entities, deltaTime);

}
void CollideBallWithAnotherBallScene::FixedUpdate()
{
	// Example: physics updates at fixed timestep.
}
void CollideBallWithAnotherBallScene::Draw()
{
	m_vulkanRHI->BeginFrame();

	// Record draw commands into the currently-acquired command buffer
	VkCommandBuffer cmd = m_vulkanRHI->GetCurrentCommandBuffer();
	if (cmd != VK_NULL_HANDLE)
	{
		m_systemManager.Render(cmd, m_entities);


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
void CollideBallWithAnotherBallScene::HandleInput(float deltaTime) {

	if (m_inputHandler.isKeyPressed(GLFW_KEY_ESCAPE)) glfwSetWindowShouldClose(m_window->getGLFWwindow(), true);

	const float cameraMoveSpeed = 1.0f;
	if (m_inputHandler.isKeyHeld(GLFW_KEY_W)) m_camera.Translate(m_camera.Forward() * cameraMoveSpeed * deltaTime);
	if (m_inputHandler.isKeyHeld(GLFW_KEY_S)) m_camera.Translate(-m_camera.Forward() * cameraMoveSpeed * deltaTime);
	if (m_inputHandler.isKeyHeld(GLFW_KEY_A)) m_camera.Translate(-m_camera.Right() * cameraMoveSpeed * deltaTime);
	if (m_inputHandler.isKeyHeld(GLFW_KEY_D)) m_camera.Translate(m_camera.Right() * cameraMoveSpeed * deltaTime);
	m_vulkanRHI->SetActiveCamera(&m_camera);
}
void CollideBallWithAnotherBallScene::SerializeState()
{
	// Example: serialize entity states to a file.
}
void CollideBallWithAnotherBallScene::DeserializeState()
{
	// Example: deserialize entity states from a file.
}

void CollideBallWithAnotherBallScene::AddEntity(Entity&& entity) {
	m_entities.push_back(std::move(entity));
}
void CollideBallWithAnotherBallScene::RemoveEntity(int index) {
	if (index >= 0 && index < m_entities.size())
		m_entities.erase(m_entities.begin() + index);
}