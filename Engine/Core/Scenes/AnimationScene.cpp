#include "AnimationScene.h"
#include "../Managers/ResourceManager.h"
#include "../../IMGUI/imgui.h"
#include "../Systems/SystemSwapBuffers.h"
#include "../Systems/SystemVelocity.h"
#include "../Systems/SystemPhysics.h"
#include "../Systems/SystemCollision.h"
#include "../Systems/SystemAnimation.h"
#include "../Managers/SceneManager.h"
#include "BallDropScene.h"
#include "PanningScene.h"
#include "TemplateScene.h"
#include "FlatBufferScene.h"

AnimationScene::AnimationScene(Window& p_window, VulkanRHI* rhi, GUI* p_gui)
    : m_window(&p_window), 
      m_inputHandler(p_window), 
      m_camera(60.0f, 16.0f / 9.0f, 0.1f, 1000.0f),
      m_vulkanRHI(rhi), 
      m_gui(p_gui), 
      m_systemManager(rhi, 2)
{
    // Setup camera - position it to see the animated objects
    m_camera.SetPosition(glm::vec3(0.0f, 3.0f, 8.0f));
    m_camera.LookAt(glm::vec3(0.0f, 2.0f, 0.0f));
    m_vulkanRHI->SetActiveCamera(&m_camera);

    // Load textures
    const Texture redBrick(m_vulkanRHI, "Assets/red_brick_diff_1k.jpg", TextureType::Albedo, true);
    const Texture mossyCobblestone(m_vulkanRHI, "Assets/mossy_cobblestone_diff_1k.jpg", TextureType::Albedo, true);

    // Create ground plane
    {
        Entity planeEntity;
        planeEntity.AddComponent(EComponentType::Component_Transform, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-90.0f, 0.0f, 0.0f), glm::vec3(50.0f));
        planeEntity.AddComponent(EComponentType::Component_Geometry);
        planeEntity.AddComponent(EComponentType::Component_Collision);

        ComponentGeometry* geom = planeEntity.GetComponent<ComponentGeometry>(EComponentType::Component_Geometry);
        if (geom)
        {
            MeshData meshData = ResourceManager::CreatePlaneMesh(10.0f, 1.0f, 1.0f);
            auto [verts, indices] = meshData;
            geom->InitializeMesh(m_vulkanRHI, verts, indices);
            geom->InitializePipeline(m_vulkanRHI, m_vulkanRHI->GetRenderPass(), m_vulkanRHI->GetSwapchainExtent(),
                "SHADERS/object.vert.spv", "SHADERS/object.frag.spv");
            geom->AddTexture(m_vulkanRHI, redBrick);
        }

        ComponentCollision* col = planeEntity.GetComponent<ComponentCollision>(EComponentType::Component_Collision);
        if (col)
        {
            // Plane collider: normal pointing UP, positioned at origin
            col->SetCollider(std::make_unique<Physics::Plane>(
                glm::vec3(0.0f, 1.0f, 0.0f),  // Normal pointing up
                glm::vec3(1.0f, 0.0f, 0.0f),  // Right vector
                glm::vec3(0.0f, 0.0f, 1.0f)   // Forward vector
            ));
        }

        m_entities.push_back(std::move(planeEntity));
    }

    // Create animated objects demonstrating different features
    CreateLinearPathObject();
    CreateLoopingPathObject();
    CreateReversingPathObject();
    CreateSmoothstepObject();
    CreateCollisionDemoObjects();

    // Register systems - CRITICAL: Animation must run FIRST to update animated transforms
    m_systemManager.RegisterSystem(std::make_unique<SystemAnimation>());
    m_systemManager.RegisterSystem(std::make_unique<SystemVelocity>());
    m_systemManager.RegisterSystem(std::make_unique<SystemPhysics>());
    m_systemManager.RegisterSystem(std::make_unique<SystemCollision>(m_entities.size(), m_vulkanRHI));
    m_systemManager.RegisterSystem(std::make_unique<SystemSwapBuffers>());

    m_paused = false;
}

AnimationScene::~AnimationScene()
{
    if (m_vulkanRHI)
    {
        m_vulkanRHI->WaitIdle();

        for (auto& entity : m_entities)
            entity.Destroy();

        m_systemManager.Shutdown();
    }
}

void AnimationScene::CreateLinearPathObject()
{
    Entity animatedCube;
    glm::vec3 startPos(-3.0f, 1.0f, 0.0f);
    animatedCube.AddComponent(EComponentType::Component_Transform, startPos, glm::vec3(0.0f), glm::vec3(1.0f));
    animatedCube.AddComponent(EComponentType::Component_Geometry);
    animatedCube.AddComponent(EComponentType::Component_Animation);
    animatedCube.AddComponent(EComponentType::Component_Collision);

    // Setup geometry
    ComponentGeometry* geom = animatedCube.GetComponent<ComponentGeometry>(EComponentType::Component_Geometry);
    if (geom)
    {
        MeshData meshData = ResourceManager::CreateCubeMesh();
        auto [verts, indices] = meshData;
        geom->InitializeMesh(m_vulkanRHI, verts, indices);
        geom->InitializePipeline(m_vulkanRHI, m_vulkanRHI->GetRenderPass(), m_vulkanRHI->GetSwapchainExtent(),
            "SHADERS/object.vert.spv", "SHADERS/object.frag.spv");

        // Create and add RED 1x1 solid color texture
        unsigned char redPixel[4] = { 255, 0, 0, 255 };
        auto redTexture = Texture::CreateFromMemory(m_vulkanRHI, redPixel, 1, 1, 4, TextureType::Albedo, false);
        if (redTexture)
        {
            geom->AddTexture(m_vulkanRHI, *redTexture);
        }
    }

    // Setup animation with linear interpolation
    ComponentAnimation* anim = animatedCube.GetComponent<ComponentAnimation>(EComponentType::Component_Animation);
    if (anim)
    {
        anim->AddWaypoint(glm::vec3(-3.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f), 0.0f);
        anim->AddWaypoint(glm::vec3(3.0f, 1.0f, 0.0f), glm::vec3(0.0f, 360.0f, 0.0f), 4.0f);
        anim->SetEasingType(EasingType::LINEAR);
        anim->SetPathMode(PathMode::STOP);
        anim->SetTotalDuration(4.0f);
        anim->Play();
    }

    // Setup collision
    ComponentCollision* col = animatedCube.GetComponent<ComponentCollision>(EComponentType::Component_Collision);
    if (col)
    {
        col->SetCollider(std::make_unique<Physics::Sphere>(startPos, 1.0f));
    }

    m_entities.push_back(std::move(animatedCube));
}

void AnimationScene::CreateLoopingPathObject()
{
    Entity animatedSphere;
    glm::vec3 startPos(-2.0f, 2.0f, -2.0f);
    animatedSphere.AddComponent(EComponentType::Component_Transform, startPos, glm::vec3(0.0f), glm::vec3(1.0f));
    animatedSphere.AddComponent(EComponentType::Component_Geometry);
    animatedSphere.AddComponent(EComponentType::Component_Animation);
    animatedSphere.AddComponent(EComponentType::Component_Collision);

    // Setup geometry
    ComponentGeometry* geom = animatedSphere.GetComponent<ComponentGeometry>(EComponentType::Component_Geometry);
    if (geom)
    {
        MeshData meshData = ResourceManager::CreateSphereMesh(1.0f, 16, 16);
        auto [verts, indices] = meshData;
        geom->InitializeMesh(m_vulkanRHI, verts, indices);
        geom->InitializePipeline(m_vulkanRHI, m_vulkanRHI->GetRenderPass(), m_vulkanRHI->GetSwapchainExtent(),
            "SHADERS/object.vert.spv", "SHADERS/object.frag.spv");

        // Create and add GREEN 1x1 solid color texture
        unsigned char greenPixel[4] = { 0, 255, 0, 255 };
        auto greenTexture = Texture::CreateFromMemory(m_vulkanRHI, greenPixel, 1, 1, 4, TextureType::Albedo, false);
        if (greenTexture)
        {
            geom->AddTexture(m_vulkanRHI, *greenTexture);
        }
    }

    // Setup animation with looping - Square path
    ComponentAnimation* anim = animatedSphere.GetComponent<ComponentAnimation>(EComponentType::Component_Animation);
    if (anim)
    {
        anim->AddWaypoint(glm::vec3(-2.0f, 2.0f, -2.0f), glm::vec3(0.0f, 0.0f, 0.0f), 0.0f);
        anim->AddWaypoint(glm::vec3(2.0f, 2.0f, -2.0f), glm::vec3(0.0f, 90.0f, 0.0f), 2.0f);
        anim->AddWaypoint(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 180.0f, 0.0f), 4.0f);
        anim->AddWaypoint(glm::vec3(-2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 270.0f, 0.0f), 6.0f);
        anim->SetEasingType(EasingType::LINEAR);
        anim->SetPathMode(PathMode::LOOP);
        anim->SetTotalDuration(6.0f);
        anim->Play();
    }

    // Setup collision
    ComponentCollision* col = animatedSphere.GetComponent<ComponentCollision>(EComponentType::Component_Collision);
    if (col)
    {
        col->SetCollider(std::make_unique<Physics::Sphere>(startPos, 1.0f));
    }

    m_entities.push_back(std::move(animatedSphere));
}

void AnimationScene::CreateReversingPathObject()
{
    Entity animatedCapsule;
    glm::vec3 startPos(0.0f, 1.5f, -3.0f);
    animatedCapsule.AddComponent(EComponentType::Component_Transform, startPos, glm::vec3(0.0f), glm::vec3(1.0f, 1.5f, 1.0f));
    animatedCapsule.AddComponent(EComponentType::Component_Geometry);
    animatedCapsule.AddComponent(EComponentType::Component_Animation);
    animatedCapsule.AddComponent(EComponentType::Component_Collision);

    // Setup geometry
    ComponentGeometry* geom = animatedCapsule.GetComponent<ComponentGeometry>(EComponentType::Component_Geometry);
    if (geom)
    {
        MeshData meshData = ResourceManager::CreateCapsuleMesh(1.0f, 0.5f, 1.5f, 16, 16);
        auto [verts, indices] = meshData;
        geom->InitializeMesh(m_vulkanRHI, verts, indices);
        geom->InitializePipeline(m_vulkanRHI, m_vulkanRHI->GetRenderPass(), m_vulkanRHI->GetSwapchainExtent(),
            "SHADERS/object.vert.spv", "SHADERS/object.frag.spv");

        // Create and add BLUE 1x1 solid color texture
        unsigned char bluePixel[4] = { 0, 0, 255, 255 };
        auto blueTexture = Texture::CreateFromMemory(m_vulkanRHI, bluePixel, 1, 1, 4, TextureType::Albedo, false);
        if (blueTexture)
        {
            geom->AddTexture(m_vulkanRHI, *blueTexture);
        }
    }

    // Setup animation with reversing
    ComponentAnimation* anim = animatedCapsule.GetComponent<ComponentAnimation>(EComponentType::Component_Animation);
    if (anim)
    {
        anim->AddWaypoint(glm::vec3(0.0f, 1.5f, -3.0f), glm::vec3(0.0f, 0.0f, 0.0f), 0.0f);
        anim->AddWaypoint(glm::vec3(3.0f, 3.5f, -3.0f), glm::vec3(0.0f, 0.0f, 180.0f), 2.0f);
        anim->SetEasingType(EasingType::LINEAR);
        anim->SetPathMode(PathMode::REVERSE);
        anim->SetTotalDuration(2.0f);
        anim->Play();
    }

    // Setup collision
    ComponentCollision* col = animatedCapsule.GetComponent<ComponentCollision>(EComponentType::Component_Collision);
    if (col)
    {
        col->SetCollider(std::make_unique<Physics::Capsule>(
            startPos + glm::vec3(0.0f, 0.75f, 0.0f),
            startPos - glm::vec3(0.0f, 0.75f, 0.0f),
            0.5f));
    }

    m_entities.push_back(std::move(animatedCapsule));
}

void AnimationScene::CreateSmoothstepObject()
{
    Entity animatedCylinder;
    glm::vec3 startPos(3.0f, 1.0f, 0.0f);
    animatedCylinder.AddComponent(EComponentType::Component_Transform, startPos, glm::vec3(0.0f), glm::vec3(0.8f, 1.5f, 0.8f));
    animatedCylinder.AddComponent(EComponentType::Component_Geometry);
    animatedCylinder.AddComponent(EComponentType::Component_Animation);
    animatedCylinder.AddComponent(EComponentType::Component_Collision);

    // Setup geometry
    ComponentGeometry* geom = animatedCylinder.GetComponent<ComponentGeometry>(EComponentType::Component_Geometry);
    if (geom)
    {
        MeshData meshData = ResourceManager::CreateCylinderMesh(1.0f, 0.8f, 1.5f, 16);
        auto [verts, indices] = meshData;
        geom->InitializeMesh(m_vulkanRHI, verts, indices);
        geom->InitializePipeline(m_vulkanRHI, m_vulkanRHI->GetRenderPass(), m_vulkanRHI->GetSwapchainExtent(),
            "SHADERS/object.vert.spv", "SHADERS/object.frag.spv");

        // Create and add YELLOW 1x1 solid color texture
        unsigned char yellowPixel[4] = { 255, 255, 0, 255 };
        auto yellowTexture = Texture::CreateFromMemory(m_vulkanRHI, yellowPixel, 1, 1, 4, TextureType::Albedo, false);
        if (yellowTexture)
        {
            geom->AddTexture(m_vulkanRHI, *yellowTexture);
        }
    }

    // Setup animation with smoothstep easing - bobbing motion
    ComponentAnimation* anim = animatedCylinder.GetComponent<ComponentAnimation>(EComponentType::Component_Animation);
    if (anim)
    {
        anim->AddWaypoint(glm::vec3(3.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f), 0.0f);
        anim->AddWaypoint(glm::vec3(3.0f, 3.5f, 0.0f), glm::vec3(720.0f, 0.0f, 0.0f), 3.0f);
        anim->SetEasingType(EasingType::SMOOTHSTEP);
        anim->SetPathMode(PathMode::LOOP);
        anim->SetTotalDuration(3.0f);
        anim->Play();
    }

    // Setup collision
    ComponentCollision* col = animatedCylinder.GetComponent<ComponentCollision>(EComponentType::Component_Collision);
    if (col)
    {
        col->SetCollider(std::make_unique<Physics::Cylinder>(
            startPos + glm::vec3(0.0f, 0.75f, 0.0f),
            startPos - glm::vec3(0.0f, 0.75f, 0.0f),
            0.8f));
    }

    m_entities.push_back(std::move(animatedCylinder));
}

void AnimationScene::CreateCollisionDemoObjects()
{
    // Create a falling cube that will collide with moving platform
    Entity fallingCube;
    glm::vec3 cubePos(0.0f, 5.0f, 2.0f);
    fallingCube.AddComponent(EComponentType::Component_Transform, cubePos, glm::vec3(0.0f), glm::vec3(0.8f));
    fallingCube.AddComponent(EComponentType::Component_Velocity, glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(0.0f));
    fallingCube.AddComponent(EComponentType::Component_Physics);
    fallingCube.AddComponent(EComponentType::Component_Geometry);
    fallingCube.AddComponent(EComponentType::Component_Collision);

    ComponentGeometry* geom = fallingCube.GetComponent<ComponentGeometry>(EComponentType::Component_Geometry);
    if (geom)
    {
        MeshData meshData = ResourceManager::CreateCubeMesh();
        auto [verts, indices] = meshData;
        geom->InitializeMesh(m_vulkanRHI, verts, indices);
        geom->InitializePipeline(m_vulkanRHI, m_vulkanRHI->GetRenderPass(), m_vulkanRHI->GetSwapchainExtent(),
            "SHADERS/object.vert.spv", "SHADERS/object.frag.spv");

        // Create and add WHITE 1x1 solid color texture
        unsigned char whitePixel[4] = { 255, 255, 255, 255 };
        auto whiteTexture = Texture::CreateFromMemory(m_vulkanRHI, whitePixel, 1, 1, 4, TextureType::Albedo, false);
        if (whiteTexture)
        {
            geom->AddTexture(m_vulkanRHI, *whiteTexture);
        }
    }

    ComponentPhysics* phys = fallingCube.GetComponent<ComponentPhysics>(EComponentType::Component_Physics);
    if (phys)
    {
        phys->SetMass(1.0f);
        phys->SetAffectedByGravity(true);
    }

    ComponentCollision* col = fallingCube.GetComponent<ComponentCollision>(EComponentType::Component_Collision);
    if (col)
    {
        col->SetCollider(std::make_unique<Physics::Sphere>(cubePos, 0.8f));
    }

    m_entities.push_back(std::move(fallingCube));
}

void AnimationScene::Start(float deltaTime)
{
    // No threading - everything runs on main thread
}

void AnimationScene::Stop()
{
    // No threads to stop
}

void AnimationScene::Update(float deltaTime)
{
    m_window->PollEvents();
    m_deltaTime = deltaTime;

    if (m_paused.load())
        deltaTime = 0.0f;

    // Debug output (remove after verification)
    static int frameCount = 0;
    if (frameCount++ % 60 == 0)  // Print every 60 frames
    {
        std::cout << "Update called! DeltaTime: " << deltaTime
            << " Paused: " << m_paused.load()
            << " EntityCount: " << m_entities.size() << std::endl;
    }

    // Update all systems on main thread
    m_systemManager.Update(m_entities, deltaTime);
}

void AnimationScene::HandleInput(float deltaTime)
{
    if (m_inputHandler.isKeyPressed(GLFW_KEY_ESCAPE))
        glfwSetWindowShouldClose(m_window->getGLFWwindow(), true);

    float cameraMoveSpeed = 5.0f;
    if (m_inputHandler.isKeyHeld(GLFW_KEY_LEFT_SHIFT))
        cameraMoveSpeed *= 2.0f;

    if (m_inputHandler.isKeyHeld(GLFW_KEY_W))
        m_camera.Translate(m_camera.Forward() * cameraMoveSpeed * deltaTime);
    if (m_inputHandler.isKeyHeld(GLFW_KEY_S))
        m_camera.Translate(-m_camera.Forward() * cameraMoveSpeed * deltaTime);
    if (m_inputHandler.isKeyHeld(GLFW_KEY_A))
        m_camera.Translate(-m_camera.Right() * cameraMoveSpeed * deltaTime);
    if (m_inputHandler.isKeyHeld(GLFW_KEY_D))
        m_camera.Translate(m_camera.Right() * cameraMoveSpeed * deltaTime);
    if (m_inputHandler.isKeyHeld(GLFW_KEY_LEFT_CONTROL))
        m_camera.Translate(-m_camera.Up() * cameraMoveSpeed * deltaTime);
    if (m_inputHandler.isKeyHeld(GLFW_KEY_SPACE))
        m_camera.Translate(m_camera.Up() * cameraMoveSpeed * deltaTime);

    if (m_inputHandler.isKeyHeld(GLFW_KEY_J))
        m_camera.Rotate(glm::vec3(0.0f, -90.0f * deltaTime, 0.0f));
    if (m_inputHandler.isKeyHeld(GLFW_KEY_L))
        m_camera.Rotate(glm::vec3(0.0f, 90.0f * deltaTime, 0.0f));
    if (m_inputHandler.isKeyHeld(GLFW_KEY_K))
        m_camera.Rotate(glm::vec3(-90.0f * deltaTime, 0.0f, 0.0f));
    if (m_inputHandler.isKeyHeld(GLFW_KEY_I))
        m_camera.Rotate(glm::vec3(90.0f * deltaTime, 0.0f, 0.0f));

    m_vulkanRHI->SetActiveCamera(&m_camera);
}

void AnimationScene::FixedUpdate()
{
}

void AnimationScene::Draw()
{
    m_vulkanRHI->BeginFrame();

    VkCommandBuffer cmd = m_vulkanRHI->GetCurrentCommandBuffer();
    if (cmd != VK_NULL_HANDLE)
    {
        m_systemManager.Render(cmd, m_entities);

        if (m_gui)
        {
            m_gui->NewFrame(*m_window);

            if (ImGui::BeginMainMenuBar())
            {
                if (ImGui::BeginMenu("Scenes"))
                {
                    if (ImGui::MenuItem("Animation Demo"))
                        SceneManager::Instance().RequestReplaceScene(std::make_unique<AnimationScene>(*m_window, m_vulkanRHI, m_gui));
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

            ImGui::Begin("Animation Demo - Controls");
            ImGui::Text("Animated Objects Demonstration");
            ImGui::Separator();

            ImGui::Text("1. Red Cube (LINEAR + STOP)");
            ImGui::Text("   - Sweeps left to right over 4 seconds");
            ImGui::Text("   - Linear interpolation, stops at end");

            ImGui::Separator();

            ImGui::Text("2. Green Sphere (LINEAR + LOOP)");
            ImGui::Text("   - Traces a square path continuously");
            ImGui::Text("   - Loops every 6 seconds");

            ImGui::Separator();

            ImGui::Text("3. Blue Capsule (LINEAR + REVERSE)");
            ImGui::Text("   - Bounces forward then backward");
            ImGui::Text("   - 2 seconds each direction");

            ImGui::Separator();

            ImGui::Text("4. Yellow Cylinder (SMOOTHSTEP + LOOP)");
            ImGui::Text("   - Bobs smoothly up and down");
            ImGui::Text("   - Smooth acceleration/deceleration");

            ImGui::Separator();

            ImGui::Text("5. White Cube (PHYSICS)");
            ImGui::Text("   - Falls from above due to gravity");
            ImGui::Text("   - Collides with ground and animated objects");

            ImGui::Separator();

            if (ImGui::Button("Start/Stop Simulation"))
                m_paused = !m_paused;

            ImGui::Text("FPS: %.1f", m_deltaTime > 0.0f ? 1.0f / m_deltaTime : 0.0f);

            ImGui::End();

            m_gui->Render(cmd);
        }

        m_vulkanRHI->EndFrame();
        m_vulkanRHI->Present();
    }
}

void AnimationScene::SerializeState()
{
}

void AnimationScene::DeserializeState()
{
}

void AnimationScene::Destroy()
{
    if (m_vulkanRHI)
    {
        m_vulkanRHI->WaitIdle();

        for (auto& entity : m_entities)
            entity.Destroy();

        m_systemManager.Shutdown();
    }
}

void AnimationScene::AddEntity(Entity&& entity)
{
    m_entities.push_back(std::move(entity));
}

void AnimationScene::RemoveEntity(int index)
{
    if (index >= 0 && index < static_cast<int>(m_entities.size()))
        m_entities.erase(m_entities.begin() + index);
}