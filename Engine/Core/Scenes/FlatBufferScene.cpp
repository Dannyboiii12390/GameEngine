#include "FlatBufferScene.h"
#include "../Managers/ResourceManager.h"
#include "../../IMGUI/imgui.h"
#include "../Systems/SystemSwapBuffers.h"

#include <fstream>
#include <iostream>
#include <vector>
#include <filesystem>

#include "../Systems/SystemVelocity.h"
#include "../Systems/SystemPhysics.h"
#include "../Systems/SystemCollision.h"
#include "BallDropScene.h"
#include "PanningScene.h"
#include "TemplateScene.h"
#include "../Managers/SceneManager.h"

#include "../../Assets/Scene_generated.h"
#include <flatbuffers/flatbuffers.h>
#include "../../DebugUtils.h"

FlatBufferScene::FlatBufferScene(Window& p_window, VulkanRHI* rhi, GUI* p_gui) :
	m_window(&p_window), m_inputHandler(p_window), m_camera(90, 16.0f / 9.0f, 0.1f, 100.0f), m_vulkanRHI(rhi),
	m_gui(p_gui), m_systemManager(rhi, 2)
{
	// Camera, vulkan, and imgui Initialisation
	m_vulkanRHI->SetActiveCamera(&m_camera);
	m_camera.SetPosition(glm::vec3(-5.0f, 5.0f, 0.0f));
	m_camera.LookAt(glm::vec3(0.0f, 0.0f, 0.0f));

	m_paused = false;

	// 1. Ensure the file exists before attempting to load
	if (!std::filesystem::exists("scenes/Level1.bin"))
	{
		std::cout << "Bin file not found, creating a default one...\n";
		SerializeState();
	}

	// 2. Load properties directly into the scene
	DeserializeState();

	//// 3. TEMPORARY: Manually Add a Cube Entity so we have something to render
	auto createEntity = [&](glm::vec3 pos)
		{
			Entity cube;
			cube.AddComponent(EComponentType::Component_Transform, pos, glm::vec3(0.0f), glm::vec3(1.0f));
			cube.AddComponent(EComponentType::Component_Geometry);

			ComponentGeometry* geom = cube.GetComponent<ComponentGeometry>(EComponentType::Component_Geometry);
			//if (geom)
			{
				// You can swap this with a pure cube mesh if your ResourceManager supports it
				MeshData meshData = ResourceManager::CreateCubeMesh(); // Fallback visible mesh
				auto [verts, indices] = meshData;
				geom->InitializeMesh(m_vulkanRHI, verts, indices);
				geom->InitializePipeline(m_vulkanRHI, m_vulkanRHI->GetRenderPass(), m_vulkanRHI->GetSwapchainExtent(), "SHADERS/object.vert.spv", "SHADERS/object.frag.spv");

				// Note: Requires a valid texture
				const Texture defaultTex(m_vulkanRHI, "Assets/red_brick_diff_1k.jpg", TextureType::Albedo, true);
				geom->AddTexture(m_vulkanRHI, defaultTex);
			}
			m_entities.push_back(std::move(cube));
		};
	//createEntity(glm::vec3(-5.0f, 0.0f, 0.0f));
	//createEntity(glm::vec3(5.0f, 0.0f, 0.0f));
	

	// 4. Register the required systems so objects are rendered
	m_systemManager.RegisterSystem(std::make_unique<SystemVelocity>());
	m_systemManager.RegisterSystem(std::make_unique<SystemPhysics>());
	m_systemManager.RegisterSystem(std::make_unique<SystemCollision>(m_entities.size(), m_vulkanRHI));
	m_systemManager.RegisterSystem(std::make_unique<SystemSwapBuffers>());
}

FlatBufferScene::~FlatBufferScene()
{
	if (m_vulkanRHI)
	{
		m_vulkanRHI->WaitIdle();

		for (auto& entity : m_entities)
			entity.Destroy();

		m_systemManager.Shutdown();
	}
}

void FlatBufferScene::Destroy()
{
	if (m_vulkanRHI)
	{
		m_vulkanRHI->WaitIdle();

		for (auto& entity : m_entities)
			entity.Destroy();

		m_systemManager.Shutdown();
	}
}

void FlatBufferScene::Start(float deltaTime) 
{
	// add diagnostics only once
	static bool diagCreated = false;
	if (!diagCreated) {
		CreateDiagnosticCube();
		diagCreated = true;
	}
}
void FlatBufferScene::Stop() {}

void FlatBufferScene::Update(float deltaTime)
{
	m_window->PollEvents();
	m_deltaTime = deltaTime;
	deltaTime = m_paused ? 0.0f : deltaTime;

	m_systemManager.Update(m_entities, deltaTime);
}

void FlatBufferScene::FixedUpdate() {}

void FlatBufferScene::Draw()
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

			// Global UI to switch between scenes
			if (ImGui::BeginMainMenuBar())
			{
				if (ImGui::BeginMenu("Scenes"))
				{
					if (ImGui::MenuItem("Ball Drop"))
						SceneManager::Instance().RequestReplaceScene(std::make_unique<BallDropScene>(*m_window, m_vulkanRHI, m_gui));
					
					if (ImGui::MenuItem("Panning"))
						SceneManager::Instance().RequestReplaceScene(std::make_unique<PanningScene>(*m_window, m_vulkanRHI, m_gui));
					
					if (ImGui::MenuItem("Template"))
						SceneManager::Instance().RequestReplaceScene(std::make_unique<TemplateScene>(*m_window, m_vulkanRHI, m_gui));

					if (ImGui::MenuItem("Reload FlatBuffer Scene"))
						SceneManager::Instance().RequestReplaceScene(std::make_unique<FlatBufferScene>(*m_window, m_vulkanRHI, m_gui));

					ImGui::EndMenu();
				}

				ImGui::EndMainMenuBar();
			}

			if (show_demo_window)
				ImGui::ShowDemoWindow(&show_demo_window);

			if (show_about)
			{
				ImGui::Begin("Debug Diagnostics");
				ImGui::Text("File: FlatBuffer Scene Loaded");
				ImGui::Text("FPS: %.1f", m_deltaTime > 0 ? (1.0f / m_deltaTime) : 0);
				ImGui::Text("Object Count: %d", m_entities.size());
				if (ImGui::Button("Start/Stop Simulation"))
					m_paused = !m_paused;
				if(ImGui::Button("Save State")) SerializeState();
				ImGui::End();
			}

			m_gui->Render(cmd);
		}
		m_vulkanRHI->EndFrame();
		m_vulkanRHI->Present();
	}
}

void FlatBufferScene::HandleInput(float deltaTime)
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

void FlatBufferScene::SerializeState()
{
	LOG_DEBUG("Serializing scene state to flatbuffer...");

	flatbuffers::FlatBufferBuilder builder(4096);

	// Scene metadata
	auto sceneName = builder.CreateString("Level1");
	auto sceneDesc = builder.CreateString("Saved scene containing cube entities and camera state.");

	// Cameras - save current active camera (m_camera)
	{
		glm::vec3 camPos = m_camera.GetPosition();
		glm::vec3 camRot = m_camera.GetRotation(); // x = pitch, y = yaw, z = roll

		Simulation::Vec3 camPosStruct(camPos.x, camPos.y, camPos.z);
		Simulation::RotationEuler camOrientStruct(camRot.y /*yaw*/, camRot.x /*pitch*/, camRot.z /*roll*/);
		Simulation::Vec3 camScaleStruct(1.0f, 1.0f, 1.0f); // cameras don't use scale but Transform requires it
		Simulation::Transform camTransform(camPosStruct, camOrientStruct, camScaleStruct);

		auto camName = builder.CreateString("MainCamera");
		auto camOffset = Simulation::CreateCamera(builder, camName, &camTransform, Simulation::CameraType_NONE, 0);

		std::vector<flatbuffers::Offset<Simulation::Camera>> cams;
		cams.push_back(camOffset);
		auto camerasVec = builder.CreateVector(cams);

		// Objects vector will be created below; temporarily collect objects first.
		// Build objects
		std::vector<flatbuffers::Offset<Simulation::Object>> objectsVecOffsets;
		for (auto &e : m_entities)
		{
			// Save only entities that have a transform + geometry (likely visible objects)
			if (!e.HasComponent(EComponentType::Component_Geometry) || !e.HasComponent(EComponentType::Component_Transform))
				continue;

			auto* xf = e.GetComponent<ComponentTransform>(EComponentType::Component_Transform);
			if (!xf) continue;

			// Read transform from component (read buffer values)
			glm::vec3 pos = xf->Position();
			glm::vec3 rot = xf->Rotation(); // pitch(x), yaw(y), roll(z)
			glm::vec3 scale = xf->Scale();

			Simulation::Vec3 posStruct(pos.x, pos.y, pos.z);
			Simulation::RotationEuler orientStruct(rot.y /*yaw*/, rot.x /*pitch*/, rot.z /*roll*/);
			Simulation::Vec3 scaleStruct(scale.x, scale.y, scale.z);
			Simulation::Transform objTransform(posStruct, orientStruct, scaleStruct);

			// Represent geometry as a cuboid with 'size' equal to entity scale.
			auto cuboidOffset = Simulation::CreateCuboid(builder, &scaleStruct);

			// Name (optional)
			auto name = builder.CreateString("cube");

			// Create the Object table. Use Shape_Cuboid and default behaviour/collision.
			auto objOffset = Simulation::CreateObject(
				builder,
				name,
				&objTransform,
				0, // material
				Simulation::Shape_Cuboid,
				cuboidOffset.Union(), // union as void offset (Offset<T> convertible)
				Simulation::Behaviour_NONE,
				0,
				Simulation::CollisionType_SOLID
			);

			objectsVecOffsets.push_back(objOffset);
		}

		auto objectsVec = builder.CreateVector(objectsVecOffsets);

		// Finish the scene with cameras and objects filled.
		auto sceneOffset = Simulation::CreateScene(
			builder,
			sceneName,
			sceneDesc,
			true,           // gravity_on
			camerasVec,
			objectsVec,
			0,              // spawners_type
			0,              // spawners
			0,              // materials
			0               // interactions
		);

		builder.Finish(sceneOffset);

		// Ensure directory exists and write buffer to disk
		std::filesystem::create_directories("scenes");
		std::ofstream outfile("scenes/Level1.bin", std::ios::binary);
		if (!outfile.is_open())
		{
			std::cerr << "SerializeState: failed to open scenes/Level1.bin for writing\n";
			return;
		}
		outfile.write(reinterpret_cast<const char*>(builder.GetBufferPointer()), static_cast<std::streamsize>(builder.GetSize()));
		outfile.close();
	}
}

void FlatBufferScene::DeserializeState()
{
	std::ifstream infile("scenes/Level1.bin", std::ios::binary);
	if (!infile.is_open())
	{
		std::cerr << "DeserializeState: Failed to open scene binary data.\n";
		return;
	}

	std::vector<char> buffer((std::istreambuf_iterator<char>(infile)), std::istreambuf_iterator<char>());

	if (buffer.empty()) return;

	// Verify and parse scene
	auto scene = Simulation::GetScene(buffer.data());
	if (!scene)
	{
		std::cerr << "DeserializeState: invalid scene data\n";
		return;
	}

	// Load camera (first camera)
	if (scene->cameras() && scene->cameras()->size() > 0)
	{
		auto camFlat = scene->cameras()->Get(0);
		if (camFlat && camFlat->transform())
		{
			const Simulation::Transform* t = camFlat->transform();
			glm::vec3 camPos(t->position().x(), t->position().y(), t->position().z());
			// Transform orientation stored yaw,pitch,roll -> Component / Camera expects pitch(x), yaw(y), roll(z)
			glm::vec3 camRot(t->orientation().pitch(), t->orientation().yaw(), t->orientation().roll());
			m_camera.SetPosition(camPos);
			m_camera.SetRotation(camRot);
		}
	}

	// Load objects
	if (scene->objects())
	{
		for (auto objFlat : *scene->objects())
		{
			if (!objFlat) continue;
			const Simulation::Transform* t = objFlat->transform();
			if (!t) continue;

			glm::vec3 pos(t->position().x(), t->position().y(), t->position().z());
			glm::vec3 rot(t->orientation().pitch(), t->orientation().yaw(), t->orientation().roll());
			glm::vec3 scale(t->scale().x(), t->scale().y(), t->scale().z());

			// Create entity and components
			Entity entity;
			entity.AddComponent(EComponentType::Component_Transform, pos, rot, scale);
			entity.AddComponent(EComponentType::Component_Geometry);

			const Texture defaultTex(m_vulkanRHI, "Assets/red_brick_diff_1k.jpg", TextureType::Albedo, true);
			ComponentGeometry* geom = entity.GetComponent<ComponentGeometry>(EComponentType::Component_Geometry);
			if (geom)
			{
				// For cuboid shapes we use the cube mesh and let the transform scale it.
				MeshData meshData = ResourceManager::CreateCubeMesh();
				auto [verts, indices] = meshData;
				if (!geom->InitializeMesh(m_vulkanRHI, verts, indices))
				{
					std::cerr << "DeserializeState: failed to initialize mesh for entity\n";
				}
				else
				{
					if (!geom->InitializePipeline(m_vulkanRHI, m_vulkanRHI->GetRenderPass(), m_vulkanRHI->GetSwapchainExtent(),
						"SHADERS/object.vert.spv", "SHADERS/object.frag.spv"))
					{
						std::cerr << "DeserializeState: failed to initialize pipeline for entity\n";
					}
					// Do not require a per-entity texture here; RHI will supply a default 1x1 texture.
					// Optionally: geom->AddTexture(m_vulkanRHI, someTexture);
					geom->AddTexture(m_vulkanRHI, defaultTex);
				}
			}

			m_entities.push_back(std::move(entity));
		}
	}
}

void FlatBufferScene::CreateDiagnosticCube()
{
	if (!m_vulkanRHI || m_vulkanRHI->GetDevice() == VK_NULL_HANDLE) {
		std::cerr << "Diagnostic: VulkanRHI not initialized\n";
		return;
	}

	Entity testCube;
	testCube.AddComponent(EComponentType::Component_Transform,
		glm::vec3(0.0f, 0.0f, 0.0f),
		glm::vec3(0.0f),
		glm::vec3(1.0f));
	testCube.AddComponent(EComponentType::Component_Geometry);

	ComponentGeometry* geom = testCube.GetComponent<ComponentGeometry>(EComponentType::Component_Geometry);
	if (!geom) { std::cerr << "Diagnostic: no geometry component\n"; return; }

	auto meshData = ResourceManager::CreateCubeMesh();
	auto [verts, indices] = meshData;
	bool meshOk = geom->InitializeMesh(m_vulkanRHI, verts, indices);
	std::cout << "Diagnostic: InitializeMesh = " << meshOk << "\n";
	bool pipeOk = geom->InitializePipeline(m_vulkanRHI, m_vulkanRHI->GetRenderPass(), m_vulkanRHI->GetSwapchainExtent(),
		"SHADERS/object.vert.spv", "SHADERS/object.frag.spv");
	std::cout << "Diagnostic: InitializePipeline = " << pipeOk << "\n";
	std::cout << "Diagnostic: IsValid = " << geom->IsValid() << "\n";

	m_entities.push_back(std::move(testCube));
}

void FlatBufferScene::AddEntity(Entity&& entity)
{
	m_entities.push_back(std::move(entity));
}

void FlatBufferScene::RemoveEntity(int index)
{
	if (index >= 0 && index < m_entities.size())
		m_entities.erase(m_entities.begin() + index);
}