#define GLFW_INCLUDE_VULKAN

#include <GLFW/glfw3.h>
#include <iostream>
#include <chrono>
#include <omp.h>
#include <fstream>

#include "Renderer/Window.h"
#include "Core/Managers/SceneManager.h"
#include "Core/Scenes/TemplateScene.h"
#include "../PhysicsEngine/Shapes/Sphere.h"

/*
- Simple Lighting
- Render Passes
- Render Graph
*/
#include <vector>

#include "DebugUtils.h"
#include "../PhysicsEngine/Networking/Environment.h"
#include "IMGUI/imgui.h"
#include "Core/Scenes/FlatBufferScene.h"

#include "Core/Scenes/AnimationScene.h"
#include "Core/Scenes/BallDropScene.h"

#define GLM_FORCE_INTRINSICS
#define GLM_FORCE_SSE2
#define GLM_FORCE_AVX
/*
- [ ] {Bug: after swapping scenes, camera can no longer be controlled by input}
    - likely due to dangling pointer in VulkanRHI after old scene's camera is destroyed 
    - fix by having VulkanRHI not take ownership of camera and instead just have a raw pointer that scenes set to point to their active camera
- [ ] changes needed to extend to 3 peers
    1.	Set runtime peer count to 3.
    2.	Implement host-assigned IDs (0,1,2) in handshake.
    3.	Keep host as relay (no full mesh yet).
    4.	Include sourcePeerId in packets.
*/
int main()
{
    try
    {
#ifdef _OPENMP
        LOG_DEBUG("OpenMP is enabled! Max threads: " << omp_get_max_threads());
#endif

        //clientRequest();    

        if (!glfwInit())
        {
            throw std::runtime_error("Failed to initialize GLFW");
        }

        auto getDeltaTime = []()
        {
            static auto timeLastFrame = std::chrono::high_resolution_clock::now();
            auto timeNow = std::chrono::high_resolution_clock::now();
            float deltaTime = std::chrono::duration<float>(timeNow - timeLastFrame).count();
            timeLastFrame = timeNow;
            return deltaTime;
        };

        const int width = 1280;
        const int height = 720;
        Window window(width, height, "Vulkan Engine");

        VulkanRHI vulkanRHI;

        #ifndef _DEBUG
            // Disable validation layers in release builds to avoid their per-call CPU overhead
            vulkanRHI.EnableValidationLayers(false);
            constexpr bool VsyncOn = false; 
        #else
            constexpr bool VsyncOn = true; // Enable VSync in debug builds to cap FPS and make debugging easier 
        #endif

        vulkanRHI.Initialise(&window);
        vulkanRHI.ToggleVSync(VsyncOn);    

        GUI gui;
        gui.Create(vulkanRHI, window);

        Networking::Environment env;

        //sceneManager.AddScene(std::make_unique<BallDropScene>(window, &vulkanRHI, &gui));
		SceneManager& sceneManager = SceneManager::Instance();
		sceneManager.AddScene(std::make_unique<FlatBufferScene>(window, &vulkanRHI, &gui));

        Physics::Sphere testSphere(glm::vec3(0.0f, 0.0f, 0.0f), 1.0f);
        Physics::Sphere testSphere2(glm::vec3(0.5f, 0.0f, 0.0f), 1.0f);

        std::vector<float> fpsHistory(100);
		IScene* currentScene = sceneManager.GetCurrentScene();
		currentScene->Start(getDeltaTime()); // Call Start on the initial scene before entering the main loop

        while (window.getGLFWwindow() && !glfwWindowShouldClose(window.getGLFWwindow()))
        {
            float deltaTime = getDeltaTime();

            IScene* scene = sceneManager.GetCurrentScene();
            scene->HandleInput(deltaTime);
            scene->Update(deltaTime);
            scene->Draw();

			sceneManager.ApplyPending();

            {
                // temporary diagnostics — remove after debugging
                GLFWwindow* win = window.getGLFWwindow();
                double cx, cy;
                glfwGetCursorPos(win, &cx, &cy);
                int focused = glfwGetWindowAttrib(win, GLFW_FOCUSED);
                int cursorMode = glfwGetInputMode(win, GLFW_CURSOR);
                ImGuiIO& io = ImGui::GetIO();
                //LOG_DEBUG("GLFW: focused=" << focused << " cursorMode=" << cursorMode << " glfwCursor=(" << cx << "," << cy << ")");
                //LOG_DEBUG("ImGui: WantCaptureMouse=" << (int)io.WantCaptureMouse << " MousePos=(" << io.MousePos.x << "," << io.MousePos.y << ")");
            }

            static float timeAccumulator = 0.0f;
            static int frameCount = 0;
            static float framTimeAccumulator = 0.0f;

            // Log FPS every second
            timeAccumulator += deltaTime;
            framTimeAccumulator += deltaTime;
            frameCount++; 
            if (framTimeAccumulator >= 1.0f)
            { 
                float fps = frameCount / framTimeAccumulator;
                fpsHistory.push_back(fps);
                framTimeAccumulator = 0.0f;
                frameCount = 0; 
            }
        }

        vulkanRHI.WaitIdle();
        
        // IMPORTANT: Destroy all scenes BEFORE shutting down GUI and VulkanRHI
        // This ensures all texture resources are cleaned up while the device is still valid
        sceneManager.Shutdown();
        
        gui.Shutdown();
        vulkanRHI.Shutdown();
        window.Shutdown();


        unsigned int count = 0;
        float sum = 0.0f;
        for(float fps : fpsHistory)
        {
            if(fps > 0.0f) // filter out any zero or uninitialized values
            {
                count++;
                sum += fps;
            }
        }


        std::cout << "Average FPS: " << sum / static_cast<float>(count) << "\n";
    }



    catch (const std::exception& e)
    {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}

