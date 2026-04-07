#define GLFW_INCLUDE_VULKAN


// Reduce Windows header pollution and disable min/max macros
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// Some old Windows headers define 'near'/'far' macros which break generated code.
// Undefine them if present so methods named near()/far() in generated FlatBuffers
// headers compile correctly.
#if defined(near)
#undef near
#endif

#if defined(far)
#undef far
#endif

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
#include "../PhysicsEngine/Networking/Address.h"
#include "../PhysicsEngine/Networking/Packet.h"
#include "../PhysicsEngine/Networking/TCPSocket.h"
#include <thread>

#include "flatbuffers/flatbuffers.h"
#include "Core/Scenes/BallDropScene.h"
#include <numeric>
#include "Renderer/ComputeShader.h"
#include "Core/Scenes/PanningScene.h"
#include "IMGUI/imgui.h"
#include "Core/Scenes/FlatBufferScene.h"


/*
Todo List
- get multiple cameras to work - have multiple instances of Camera class and have pointer to the active one - friday
- networking - next week
- flocking - using compute shader - week after next
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
            scene->Update(deltaTime);

            scene->HandleInput(deltaTime);
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

