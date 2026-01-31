#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>
#include <stdexcept>

#include <array>
#include <filesystem>

// GLM is used for vec3 / mat4 in this file and components
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Renderer/VulkanRHI.h"
#include "Renderer/Window.h"

#include "Core/Entity.h"
#include "Core/Systems/SystemRenderer.h"

int main()
{
    try
    {
        std::cout << "[MAIN] start\n" << std::flush;

        if (!glfwInit())
        {
            throw std::runtime_error("Failed to initialize GLFW");
        }
        std::cout << "[MAIN] glfwInit OK\n" << std::flush;

        // Do not create an OpenGL context; we'll use Vulkan.
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        const int width = 1280;
        const int height = 720;
        GLFWwindow* raw = glfwCreateWindow(width, height, "Vulkan Window", nullptr, nullptr);
        std::cout << "[MAIN] glfwCreateWindow returned " << static_cast<void*>(raw) << "\n" << std::flush;
        Window window(raw);

        VulkanRHI vulkanRHI;
        Entity entity;
        entity.AddTranslation(glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(1.0f));
        entity.AddVelocity(glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(0.0f));
        entity.AddGeometry();

        SystemRenderer renderer;

        if (!window.GetGLFWwindow())
        {
            std::cerr << "[MAIN] window.GetGLFWwindow() is null\n" << std::flush;
            glfwTerminate();
            throw std::runtime_error("Failed to create GLFW window");
        }
        std::cout << "[MAIN] window valid\n" << std::flush;

        std::cout << "[MAIN] calling vulkanRHI.Initialise\n" << std::flush;
        vulkanRHI.Initialise(&window);
        std::cout << "[MAIN] vulkanRHI.Initialise returned\n" << std::flush;

        std::cout << "[MAIN] initializing renderer\n" << std::flush;
        renderer.Initialize(&vulkanRHI);
        std::cout << "[MAIN] renderer.Initialize returned\n" << std::flush;

        // Create the rotating triangle mesh + pipeline after RHI initialised
        ComponentGeometry* geom = entity.GetComponent<ComponentGeometry>(EComponentType::Component_Geometry);
        if (geom)
        {
            std::cout << "[MAIN] got ComponentGeometry\n" << std::flush;
            std::vector<Mesh::Vertex> verts(3);
            verts[0].position[0] = 0.0f; verts[0].position[1] =  0.5f; verts[0].position[2] = 0.0f;
            verts[1].position[0] = 0.5f; verts[1].position[1] = -0.5f; verts[1].position[2] = 0.0f;
            verts[2].position[0] = -0.5f; verts[2].position[1] = -0.5f; verts[2].position[2] = 0.0f;
            for (auto &v : verts) { v.normal[0] = v.normal[1] = v.normal[2] = 0.0f; v.uv[0] = v.uv[1] = 0.0f; }

            std::vector<uint32_t> indices = { 0, 1, 2 };

            if (!geom->InitializeMesh(&vulkanRHI, verts, indices))
                throw std::runtime_error("Failed to initialize triangle mesh");
            std::cout << "[MAIN] InitializeMesh OK\n" << std::flush;

            namespace fs = std::filesystem;
            fs::path shaderDir = fs::path(__FILE__).parent_path() / "SHADERS";
            std::string vertSpv = (shaderDir / "triangle.vert.spv").string();
            std::string fragSpv = (shaderDir / "triangle.frag.spv").string();

            std::cout << "[MAIN] looking for shaders:\n  " << vertSpv << "\n  " << fragSpv << "\n" << std::flush;
            if (!fs::exists(vertSpv) || !fs::exists(fragSpv))
            {
                std::cerr << "[MAIN] Shader files not found.\nExpected:\n  " << vertSpv << "\n  " << fragSpv << std::endl;
                std::cerr << "[MAIN] Check your custom build step or move the .spv files to the SHADERS folder." << std::endl;
                throw std::runtime_error("Missing SPIR-V shader files");
            }

            if (!geom->InitializePipeline(&vulkanRHI, vulkanRHI.GetRenderPass(), vulkanRHI.GetSwapchainExtent(), vertSpv, fragSpv))
                throw std::runtime_error("Failed to create triangle pipeline");
            std::cout << "[MAIN] InitializePipeline OK\n" << std::flush;
        }
        else
        {
            std::cout << "[MAIN] no geometry component\n" << std::flush;
        }

        std::cout << "[MAIN] entering main loop\n" << std::flush;
        double lastTime = glfwGetTime();
        int loopCount = 0;
        while (!glfwWindowShouldClose(window.GetGLFWwindow()))
        {
            glfwPollEvents();
            vulkanRHI.BeginFrame();

            double now = glfwGetTime();
            float dt = static_cast<float>(now - lastTime);
            lastTime = now;
            if (entity.HasComponent(EComponentType::Component_Translation))
            {
                auto* xf = entity.GetComponent<ComponentTranslation>(EComponentType::Component_Translation);
                if (xf)
                {
                    glm::vec3 rot = xf->Rotation();
                    rot.z += 90.0f * dt; // rotate 90 deg/sec around Z
                    xf->SetRotation(rot);
                }
            }

            std::vector<Entity*> entities = { &entity };

            // Record draw commands into the currently-acquired command buffer
            VkCommandBuffer cmd = vulkanRHI.GetCurrentCommandBuffer();
            if (cmd != VK_NULL_HANDLE)
            {
                renderer.Render(cmd, entities);
            }

            vulkanRHI.EndFrame();
            vulkanRHI.Present();

            ++loopCount;
            if ((loopCount % 60) == 0)
            {
                std::cout << "[MAIN] loop iterations: " << loopCount << "\n" << std::flush;
            }

            // safety: if loop runs too long during debugging, let it exit after many iterations
            if (loopCount > 10000)
            {
                std::cout << "[MAIN] safety exit after " << loopCount << " iterations\n" << std::flush;
                break;
            }
        }

        std::cout << "[MAIN] leaving main loop\n" << std::flush;
        glfwDestroyWindow(window.GetGLFWwindow());
        glfwTerminate();
        std::cout << "[MAIN] shutdown complete\n" << std::flush;
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}

