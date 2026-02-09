
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>

#include <chrono>
#include <omp.h>

// GLM is used for vec3 / mat4 in this file and components
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Renderer/Window.h"

#include "Core/Systems/SystemRenderer.h"
#include "Core/Managers/ResourceManager.h"
#include "Core/Managers/SceneManager.h"
#include "Core/Scenes/TemplateScene.h"

#ifdef _DEBUG
#define LOG_DEBUG(msg) std::cout << msg << std::endl;
#else
#define LOG_DEBUG(msg)
#endif


/*
- Render Passes
- Render Graph
*/

int main()
{
    try
    {

        #ifdef _OPENMP
        std::cout << "OpenMP is enabled! Max threads: " << omp_get_max_threads() << std::endl;
        #endif

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

        SceneManager sceneManager;

        VulkanRHI vulkanRHI;
		vulkanRHI.Initialise(&window);
        vulkanRHI.ToggleVSync(true);    

		sceneManager.AddScene(std::make_unique<TemplateScene>(window, &vulkanRHI));


        while (window.getGLFWwindow() && !glfwWindowShouldClose(window.getGLFWwindow()))
        {
            float deltaTime = getDeltaTime();

			IScene* scene = sceneManager.GetCurrentScene();
            scene->Update(deltaTime);
            scene->HandleInput(deltaTime);
            scene->Draw();

			static float timeAccumulator = 0.0f;
			//after 5 seconds, add a new scene on top of the current one to test scene management
			timeAccumulator += deltaTime;
            if (timeAccumulator > 5.0f)
            {
                sceneManager.AddScene(std::make_unique<TemplateScene>(window, &vulkanRHI));
                timeAccumulator = 0.0f;
            }
        }

        sceneManager.Shutdown();
        vulkanRHI.Shutdown();
        window.Shutdown();
    }


  
    catch (const std::exception& e)
    {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}

