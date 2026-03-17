#include "BallDropScene.h"
#include "../Managers/ResourceManager.h"
#include "../../IMGUI/imgui.h"
#include "../Systems/SystemSwapBuffers.h"

#include <fstream>

#ifdef _DEBUG
constexpr int NUM_BALLS = 30;
#else
constexpr int NUM_BALLS = 1000;
#endif

BallDropScene::BallDropScene(Window& p_window, VulkanRHI* rhi, GUI* p_gui) :
	m_window(&p_window), m_inputHandler(p_window), m_camera(90, 16.0f / 9.0f, 0.1f, 100.0f), m_vulkanRHI(rhi),
	m_gui(p_gui), m_systemManager(rhi, 2), woodTex(m_vulkanRHI, "Assets/wood_shutter_diff_1k.jpg", TextureType::Albedo, true)
{

	m_vulkanRHI->SetActiveCamera(&m_camera);

	m_gui->Create(*rhi, *m_window);

	const Texture entTex(m_vulkanRHI, "Assets/red_brick_diff_1k.jpg", TextureType::Albedo, true);
	const Texture concTex(m_vulkanRHI, "Assets/conc_tex.jpg", TextureType::Albedo, true);
	const Texture cobbleTex(m_vulkanRHI, "Assets/cobblestone_diff.jpg", TextureType::Albedo, true);

	//load camera position and rotation from file if it exists, otherwise use defaults
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

	auto createPlaneEntity = [this, &concTex](Entity& entity, glm::vec3 pos, int left)
		{
			glm::vec3 rotDegrees = glm::vec3(left * 45.0f, 0.0f, 0.0f);
			entity.AddComponent(EComponentType::Component_Transform, pos, rotDegrees, glm::vec3(100.0f));
			entity.AddComponent(EComponentType::Component_Geometry);
			entity.AddComponent(EComponentType::Component_Collision);
			ComponentGeometry* geom = entity.GetComponent<ComponentGeometry>(EComponentType::Component_Geometry);
			if (geom)
			{
				MeshData meshData = ResourceManager::CreatePlaneMesh();
				auto [verts, indices] = meshData;
				geom->InitializeMesh(m_vulkanRHI, verts, indices);
				geom->InitializePipeline(m_vulkanRHI, m_vulkanRHI->GetRenderPass(), m_vulkanRHI->GetSwapchainExtent(), "SHADERS/object.vert.spv", "SHADERS/object.frag.spv");
				geom->AddTexture(m_vulkanRHI, concTex);
			}
			ComponentCollision* col = entity.GetComponent<ComponentCollision>(EComponentType::Component_Collision);
			if (col)
			{
				// Provide the fully default (unrotated) XY plane vectors. 
				// SystemCollision will sync the actual collider rotation with ComponentTransform!
				col->SetCollider(std::make_unique<Physics::Plane>(pos, glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f)));
			}
		};	
	auto createOtherPlaneEntity = [this, &cobbleTex](Entity& entity, glm::vec3 pos, int left)
	{
		glm::vec3 rotDegrees = glm::vec3(0.0f, 270.0f, left * 45.0f);
		entity.AddComponent(EComponentType::Component_Transform, pos, rotDegrees, glm::vec3(100.0f));
		entity.AddComponent(EComponentType::Component_Geometry);
		entity.AddComponent(EComponentType::Component_Collision);
		ComponentGeometry* geom = entity.GetComponent<ComponentGeometry>(EComponentType::Component_Geometry);
		if (geom)
		{
			MeshData meshData = ResourceManager::CreatePlaneMesh();
			auto [verts, indices] = meshData;
			geom->InitializeMesh(m_vulkanRHI, verts, indices);
			geom->InitializePipeline(m_vulkanRHI, m_vulkanRHI->GetRenderPass(), m_vulkanRHI->GetSwapchainExtent(), "SHADERS/object.vert.spv", "SHADERS/object.frag.spv");
			geom->AddTexture(m_vulkanRHI, cobbleTex);
		}
		ComponentCollision* col = entity.GetComponent<ComponentCollision>(EComponentType::Component_Collision);
		if (col)
		{
			// Provide the fully default (unrotated) XY plane vectors.
			col->SetCollider(std::make_unique<Physics::Plane>(pos, glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f)));
		}
	};
	auto createFloorEntity = [this, &entTex](Entity& entity)
	{
		entity.AddComponent(EComponentType::Component_Transform, glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(-90.0f, 0.0f, 0.0f), glm::vec3(10.0f, 10.0f, 10.0f));
		entity.AddComponent(EComponentType::Component_Geometry);
		entity.AddComponent(EComponentType::Component_Collision);
		ComponentGeometry* geom = entity.GetComponent<ComponentGeometry>(EComponentType::Component_Geometry);
		if (geom)
		{
			MeshData meshData = ResourceManager::CreatePlaneMesh();
			auto [verts, indices] = meshData;
			// Initialize mesh so the renderer has a valid Mesh to bind/draw
			geom->InitializeMesh(m_vulkanRHI, verts, indices);
			geom->InitializePipeline(m_vulkanRHI, m_vulkanRHI->GetRenderPass(), m_vulkanRHI->GetSwapchainExtent(), "SHADERS/object.vert.spv", "SHADERS/object.frag.spv");
			geom->AddTexture(m_vulkanRHI, entTex);
		}
		ComponentCollision* col = entity.GetComponent<ComponentCollision>(EComponentType::Component_Collision);
		if (col)
		{
			col->SetCollider(std::make_unique<Physics::Plane>(glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f)));
		}
	};

	Entity leftWall;
	createPlaneEntity(leftWall, glm::vec3(0.0f, 0.0f, -5.0f), -1);
	Entity rightWall;
	createPlaneEntity(rightWall, glm::vec3(0.0f, 0.0f, 5.0f), 1);
	Entity backWall;
	createOtherPlaneEntity(backWall, glm::vec3(-5.0f, 0.0f, 0.0f), 1);
	Entity frontWall;
	createOtherPlaneEntity(frontWall, glm::vec3(5.0f, 0.0f, 0.0f), -1);

	Entity floor;
	createFloorEntity(floor);

    // Plan (pseudocode):
    // - Make the capsule visually obvious by ensuring the mesh and transform align with the capsule dimensions.
    // - Use a taller mesh height and a larger radius so the shape reads clearly as a capsule.
    // - Set transform scale to uniform (1,1,1) because the mesh will already represent the desired height & radius.
    // - Ensure the physics collider uses the same radius and height so collisions match the visual mesh.
    //
    // Implementation details:
    // - CreateCapsuleMesh(radius = 1.0f, height = 10.0f, sectorCount = 16)
    // - Transform scale = vec3(1.0f) (no additional non-uniform scaling)
    // - Physics capsule: radius = 1.0f, height = 10.0f, endpoints computed from center +/- height/2 along Y

	Entity capsuleEntity;
	glm::vec3 capsulePos = glm::vec3(0.0f, 0.0f, 0.0f);
	glm::vec3 capsuleRot = glm::vec3(0.0f, 0.0f, 45.0f); // 45 degree tilt
	capsuleEntity.AddComponent(EComponentType::Component_Transform, capsulePos, capsuleRot, glm::vec3(1.0f));
	capsuleEntity.AddComponent(EComponentType::Component_Geometry);
	capsuleEntity.AddComponent(EComponentType::Component_Collision);
	ComponentGeometry* geom2 = capsuleEntity.GetComponent<ComponentGeometry>(EComponentType::Component_Geometry);
	if (geom2)
	{
		MeshData meshData = ResourceManager::CreateCapsuleMesh(1.0f, 10.0f, 16);
		auto [verts, indices] = meshData;
		geom2->InitializeMesh(m_vulkanRHI, verts, indices);
		geom2->InitializePipeline(m_vulkanRHI, m_vulkanRHI->GetRenderPass(), m_vulkanRHI->GetSwapchainExtent(), "SHADERS/object.vert.spv", "SHADERS/object.frag.spv");
		geom2->AddTexture(m_vulkanRHI, entTex);
	}
	ComponentCollision* col2 = capsuleEntity.GetComponent<ComponentCollision>(EComponentType::Component_Collision);
	if (col2)
	{
		const float radius = 1.0f;
		const float height = 10.0f;
		
		// Provide perfectly unrotated Y-up capsule endpoints.
		// SystemCollision will take these A & B points and tilt them 45 degrees 
		// automatically based on the Transform we attached to this entity!
		glm::vec3 unrotatedA = capsulePos + glm::vec3(0.0f, -height * 0.5f, 0.0f);
		glm::vec3 unrotatedB = capsulePos + glm::vec3(0.0f,  height * 0.5f, 0.0f);

		col2->SetCollider(std::make_unique<Physics::Capsule>(unrotatedA, unrotatedB, radius));
	}

	m_entities.push_back(std::move(leftWall));
	m_entities.push_back(std::move(rightWall));
	m_entities.push_back(std::move(backWall));
	m_entities.push_back(std::move(frontWall));
	m_entities.push_back(std::move(floor));
	m_entities.push_back(std::move(capsuleEntity));

	for(int i = 0; i < NUM_BALLS; i++)
		CreateSphere();

	m_systemManager.RegisterSystem(std::make_unique<SystemVelocity>());
	m_systemManager.RegisterSystem(std::make_unique<SystemPhysics>());
	m_systemManager.RegisterSystem(std::make_unique<SystemCollision>(m_entities.size(), m_vulkanRHI));
	m_systemManager.RegisterSystem(std::make_unique<SystemSwapBuffers>());

	m_paused = false;
}
BallDropScene::~BallDropScene()
{

	//save camera position and rotation to file for next time
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

		woodTex.Destroy(m_vulkanRHI);

		for (auto& entity : m_entities)
			entity.Destroy();

		m_systemManager.Shutdown();
	}
}
void BallDropScene::Destroy()
{
	if (m_vulkanRHI)
	{
		m_vulkanRHI->WaitIdle();

		for (auto& entity : m_entities)
			entity.Destroy();

		m_systemManager.Shutdown();
	}
}
void BallDropScene::Start(float deltaTime)
{
}
void BallDropScene::Stop()
{
}
void BallDropScene::Update(float deltaTime)
{
	m_window->PollEvents();

	m_deltaTime = deltaTime;

	deltaTime = m_paused ? 0.0f : deltaTime;

	m_systemManager.Update(m_entities, deltaTime);

}
void BallDropScene::FixedUpdate()
{
}
void BallDropScene::Draw()
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
				ImGui::Text("Sphere Count: %d", m_sphereCount);
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
void BallDropScene::HandleInput(float deltaTime)
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

	if (m_inputHandler.isKeyPressed(GLFW_KEY_ENTER)) CreateSphere();




	m_vulkanRHI->SetActiveCamera(&m_camera);
}
void BallDropScene::SerializeState()
{
}
void BallDropScene::DeserializeState()
{
}
void BallDropScene::AddEntity(Entity&& entity)
{
	m_entities.push_back(std::move(entity));
}
void BallDropScene::RemoveEntity(int index)
{
	if (index >= 0 && index < m_entities.size())
		m_entities.erase(m_entities.begin() + index);
}
void BallDropScene::CreateSphere()
{
	auto createSphereEntity = [this](Entity& entity, glm::vec3 pos, glm::vec3 vel)
		{
			entity.AddComponent(EComponentType::Component_Transform, pos, glm::vec3(0.0f), glm::vec3(1.0f));
			entity.AddComponent(EComponentType::Component_Velocity, vel, glm::vec3(0.0f), glm::vec3(0.0f));
			entity.AddComponent(EComponentType::Component_Geometry);
			entity.AddComponent(EComponentType::Component_Collision);
			entity.AddComponent(EComponentType::Component_Physics);
			ComponentGeometry* geom = entity.GetComponent<ComponentGeometry>(EComponentType::Component_Geometry);
			if (geom)
			{
				MeshData meshData = ResourceManager::CreateSphereMesh(16, 16);
				auto [verts, indices] = meshData;
				// Initialize mesh so mesh buffers exist for draw calls
				geom->InitializeMesh(m_vulkanRHI, verts, indices);
				geom->InitializePipeline(m_vulkanRHI, m_vulkanRHI->GetRenderPass(), m_vulkanRHI->GetSwapchainExtent(), "SHADERS/object.vert.spv", "SHADERS/object.frag.spv");
				geom->AddTexture(m_vulkanRHI, woodTex);
			}
			ComponentCollision* col = entity.GetComponent<ComponentCollision>(EComponentType::Component_Collision);
			if (col)
			{
				col->SetCollider(std::make_unique<Physics::Sphere>(pos, 0.5f));
			}
		};
	Entity sphereEntity;
	// random position above the floor within 4 units of the center, and random horizontal velocity( 1, 10) in a random direction
	createSphereEntity(sphereEntity, glm::vec3((rand() % 800 - 400) / 100.0f, 5.0f + (rand() % 500) / 100.0f, (rand() % 800 - 400) / 100.0f), glm::vec3((rand() % 900 + 100) / 100.0f * (rand() % 2 == 0 ? -1 : 1), 0.0f, (rand() % 900 + 100) / 100.0f * (rand() % 2 == 0 ? -1 : 1)));
	m_entities.push_back(std::move(sphereEntity));

	m_sphereCount++;
}