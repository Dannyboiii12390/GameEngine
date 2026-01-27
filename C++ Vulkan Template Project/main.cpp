#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>
#include <stdexcept>

#include "Renderer/RHI/VulkanRHI.h"
#include "Graphics/Window.h"


int main()
{
    // ---- GLFW init ----
    if (!glfwInit())
    {
        std::cerr << "Failed to init GLFW\n";
        return -1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* w = glfwCreateWindow(1280, 720, "Vulkan RHI Test", nullptr, nullptr);

    if (!w)
    {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }

    Window window(w);

    // ---- Vulkan RHI ----
	std::unique_ptr<IRHI> rhi = std::make_unique<VulkanRHI>();

    try
    {
        // RHI::Initialise expects a Window* per project signatures
        rhi->Initialise(&window);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Failed to initialize VulkanRHI: " << e.what() << "\n";
        glfwDestroyWindow(window.GetGLFWwindow());
        glfwTerminate();
        return -1;
    }

    // ---- Main loop ----
    while (!glfwWindowShouldClose(window.GetGLFWwindow()))
    {
        glfwPollEvents();

        rhi->BeginFrame();

        // NOTE:
        // Here you would normally record rendering commands into your command buffer.
        // For a minimal test, we just begin/end and present.

        rhi->EndFrame();
        rhi->Present();
    }

    // ---- Cleanup ----
    rhi->WaitIdle();
    rhi->Shutdown();

    glfwDestroyWindow(window.GetGLFWwindow());
    glfwTerminate();

    return 0;
}

