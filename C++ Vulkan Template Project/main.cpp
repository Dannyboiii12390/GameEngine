#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>
#include <stdexcept>

#include <array>

#include "Renderer/VulkanRHI.h"
#include "Renderer/Window.h"

#include "Core/Entity.h"

int main()
{
    try
    {
        if (!glfwInit())
        {
            throw std::runtime_error("Failed to initialize GLFW");
        }

        // Do not create an OpenGL context; we'll use Vulkan.
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        // Optional: make the window not resizable if desired.
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        const int width = 1280;
        const int height = 720;
        Window window(glfwCreateWindow(width, height, "Vulkan Window", nullptr, nullptr));
		VulkanRHI vulkanRHI;
		Entity entity;
		entity.AddComponent(EComponentType::Component_Translation, glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(1.0f));
		entity.AddComponent(EComponentType::Component_Velocity, glm::vec3(0.0f), glm::vec3(1.0f), glm::vec3(0.0f));
        

        if (!window.GetGLFWwindow())
        {
            glfwTerminate();
            throw std::runtime_error("Failed to create GLFW window");
		}

		vulkanRHI.Initialise(&window);
        // Main loop: keep running until the user closes the window.
        while (!glfwWindowShouldClose(window.GetGLFWwindow()))
        {
            glfwPollEvents();
            // Placeholder: render or integrate with VulkanRHI here.
			vulkanRHI.BeginFrame();
			vulkanRHI.EndFrame();
			vulkanRHI.Present();

        }

        glfwDestroyWindow(window.GetGLFWwindow());
        glfwTerminate();
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}

