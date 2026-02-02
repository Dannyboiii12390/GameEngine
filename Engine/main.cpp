#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>
#include <stdexcept>

#include <array>
#include <filesystem>
#include <fstream>

// GLM is used for vec3 / mat4 in this file and components
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Renderer/VulkanRHI.h"
#include "Renderer/Window.h"

#include "Core/Entity.h"
#include "Core/Systems/SystemRenderer.h"
#include "Renderer/Camera.h"

#ifdef _DEBUG
#define LOG_DEBUG(msg) std::cout << msg << std::endl;
#else
#define LOG_DEBUG(msg)
#endif

int main()
{
    try
    {
        LOG_DEBUG("[MAIN] start");

        if (!glfwInit())
        {
            throw std::runtime_error("Failed to initialize GLFW");
        }
        LOG_DEBUG("[MAIN] glfwInit OK");

        // Do not create an OpenGL context; we'll use Vulkan.
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        const int width = 1280;
        const int height = 720;
        GLFWwindow* raw = glfwCreateWindow(width, height, "Vulkan Window", nullptr, nullptr);
        LOG_DEBUG("[MAIN] glfwCreateWindow returned " << static_cast<void*>(raw));
        Window window(raw);

        VulkanRHI vulkanRHI;
        Entity entity;
        entity.AddComponent(EComponentType::Component_Translation, glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(1.0f));
		entity.AddComponent(EComponentType::Component_Velocity, glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(0.0f));
		entity.AddComponent(EComponentType::Component_Geometry);
        
		Camera camera(90, 16.0f / 9.0f, 0.1f, 100.0f);
		vulkanRHI.SetActiveCamera(&camera);
		camera.SetPosition(glm::vec3(0.0f, 0.0f, 2.0f));
		camera.LookAt(glm::vec3(0.0f, 0.0f, 0.0f));

        SystemRenderer renderer;

        if (!window.GetGLFWwindow())
        {
            std::cerr << "[MAIN] window.GetGLFWwindow() is null\n" << std::flush;
            glfwTerminate();
            throw std::runtime_error("Failed to create GLFW window");
        }
        LOG_DEBUG("[MAIN] window valid");

        LOG_DEBUG("[MAIN] calling vulkanRHI.Initialise");
        vulkanRHI.Initialise(&window);
        LOG_DEBUG("[MAIN] vulkanRHI.Initialise returned");

        LOG_DEBUG("[MAIN] initializing renderer");
        renderer.Initialize(&vulkanRHI);
        LOG_DEBUG("[MAIN] renderer.Initialize returned");

        // Create the rotating triangle mesh + pipeline after RHI initialised
        ComponentGeometry* geom = entity.GetComponent<ComponentGeometry>(EComponentType::Component_Geometry);
        if (geom)
        {
            LOG_DEBUG("[MAIN] got ComponentGeometry");
            std::vector<Mesh::Vertex> verts(3);
            verts[0].position[0] = 0.0f; verts[0].position[1] = 0.5f; verts[0].position[2] = 0.0f;
            verts[1].position[0] = 0.5f; verts[1].position[1] = -0.5f; verts[1].position[2] = 0.0f;
            verts[2].position[0] = -0.5f; verts[2].position[1] = -0.5f; verts[2].position[2] = 0.0f;

            // Improve lighting by giving a normal pointing out of the screen
            // and give each vertex sensible UVs. Increase `uvTile` to tile the
            // brick texture across the triangle (makes bricks appear smaller / clearer).
            const float uvTile = 1.0f; // tweak this (e.g. 2..8) to change brick density/clarity

            // top vertex
            verts[0].normal[0] = 0.0f; verts[0].normal[1] = 0.0f; verts[0].normal[2] = 1.0f;
            verts[0].uv[0] = 0.5f * uvTile; verts[0].uv[1] = 1.0f * uvTile;

            // bottom-right
            verts[1].normal[0] = 0.0f; verts[1].normal[1] = 0.0f; verts[1].normal[2] = 1.0f;
            verts[1].uv[0] = 1.0f * uvTile; verts[1].uv[1] = 0.0f * uvTile;

            // bottom-left
            verts[2].normal[0] = 0.0f; verts[2].normal[1] = 0.0f; verts[2].normal[2] = 1.0f;
            verts[2].uv[0] = 0.0f * uvTile; verts[2].uv[1] = 0.0f * uvTile;

            std::vector<uint32_t> indices = { 0, 1, 2 };

            if (!geom->InitializeMesh(&vulkanRHI, verts, indices))
                throw std::runtime_error("Failed to initialize triangle mesh");
            LOG_DEBUG("[MAIN] InitializeMesh OK");

            namespace fs = std::filesystem;
            fs::path shaderDir = fs::path(__FILE__).parent_path() / "SHADERS";
            std::string vertSpv = (shaderDir / "triangle.vert.spv").string();
            std::string fragSpv = (shaderDir / "triangle.frag.spv").string();

            LOG_DEBUG("[MAIN] looking for shaders:\n  " << vertSpv << "\n  " << fragSpv);
            if (!fs::exists(vertSpv) || !fs::exists(fragSpv))
            {
                LOG_DEBUG("[MAIN] Shader files not found.\nExpected:\n  " << vertSpv << "\n  " << fragSpv);
				LOG_DEBUG("[MAIN] Check your custom build step or move the .spv files to the SHADERS folder.");
                throw std::runtime_error("Missing SPIR-V shader files");
            }

            if (!geom->InitializePipeline(&vulkanRHI, vulkanRHI.GetRenderPass(), vulkanRHI.GetSwapchainExtent(), vertSpv, fragSpv))
                throw std::runtime_error("Failed to create triangle pipeline");

            std::string texturePath = "red_brick_diff_1k.jpg";
            if (!geom->CreateTexture(&vulkanRHI, texturePath, TextureType::Albedo, true))
            {
                LOG_DEBUG("[MAIN] Warning: failed to create texture from " << texturePath);
			}

            LOG_DEBUG("[MAIN] InitializePipeline OK");
        }
        else
        {
            LOG_DEBUG("[MAIN] no geometry component");
        }

        LOG_DEBUG("[MAIN] entering main loop");
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
        }

        LOG_DEBUG("[MAIN] leaving main loop");

        // --- Ensure GPU work is finished and free per-object GPU resources before tearing down the RHI ---
        if (geom)
        {
			LOG_DEBUG("[MAIN] Destroying ComponentGeometry GPU resources");
            geom->Destroy(); // ensure mesh & pipeline free their VkBuffers/VkPipeline while device is still valid and idle
        }

        LOG_DEBUG("[MAIN] calling vulkanRHI.Shutdown");
        vulkanRHI.Shutdown(); // Shutdown waits for device idle internally
        LOG_DEBUG("[MAIN] vulkanRHI.Shutdown returned");

        glfwDestroyWindow(window.GetGLFWwindow());
        glfwTerminate();
        LOG_DEBUG("[MAIN] shutdown complete");
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}

