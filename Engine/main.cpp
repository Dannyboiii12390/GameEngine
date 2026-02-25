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
#include "Core/Scenes/CollideBallWithAnotherBallScene.h"

#include "DebugUtils.h"
#include "../PhysicsEngine/Networking/Environment.h"
#include "../PhysicsEngine/Networking/Address.h"
#include "../PhysicsEngine/Networking/Packet.h"
#include "../PhysicsEngine/Networking/TCPSocket.h"
#include <thread>

//  flat buffer serialization - packet should probably use this
//  .fbs
//  wireshark - network sniffer
//  winsock shims
//  TMNetsim

//  need to implement a UDP Socket class
//  class UDP socket
//  unit tests for networking code

int clientRequest()
{
    Networking::Environment env;
    Networking::Address serverAddr("127.0.0.1", 54000);

    Networking::TCPSocket client(serverAddr);

    std::vector<std::string> messages = {
        "Hello, Server!",
        "This is a test message.",
        "Goodbye, Server!"
    };
    std::vector<uint8_t> recvAccumulator;
    recvAccumulator.reserve(8192);

    for (const auto& msg : messages) {
        // Create packet and set payload (updates header.payloadSize)
        Networking::Packet pkt;
        pkt.header.type = 1; // example type
        pkt.setPayload(msg.data(), static_cast<std::size_t>(msg.size()));

        // Serialize packet
        std::vector<uint8_t> out;
        pkt.serialize(out);
        // Send packet
        client.Send(out.data(), static_cast<int>(out.size()));

        while (true)
        {
            uint8_t temp[4096];
            int r = client.Receive(temp, static_cast<int>(sizeof(temp)));
            if (r <= 0) {
                std::cerr << "clientRequest: receive failed or connection closed\n";
                return -1;
            }
            recvAccumulator.insert(recvAccumulator.end(), temp, temp + r);

            Networking::Packet incoming;
            if (Networking::Packet::tryDeserializeFromBuffer(recvAccumulator, incoming)) {
                std::string payload;
                if (incoming.payloadSize() > 0) {
                    payload.assign(reinterpret_cast<const char*>(incoming.payloadData()), incoming.payloadSize());
                }
                std::cout << "Echoed back: type=" << incoming.header.type << " payload=\"" << payload << "\"\n";
                break; // proceed to next message
            }
            // otherwise continue receiving bytes
        }

        // small delay so output is readable
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    return 0;
}

int main()
{
    try
    {

        #ifdef _OPENMP
        LOG_DEBUG("OpenMP is enabled! Max threads: " << omp_get_max_threads());
        #endif

		clientRequest();    

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

		sceneManager.AddScene(std::make_unique<CollideBallWithAnotherBallScene>(window, &vulkanRHI, &gui));

		Physics::Sphere testSphere(glm::vec3(0.0f, 0.0f, 0.0f), 1.0f);
		Physics::Sphere testSphere2(glm::vec3(0.5f, 0.0f, 0.0f), 1.0f);

        std::vector<float> fpsHistory(100);

        while (window.getGLFWwindow() && !glfwWindowShouldClose(window.getGLFWwindow()))
        {
            float deltaTime = getDeltaTime();

			IScene* scene = sceneManager.GetCurrentScene();
            scene->Update(deltaTime);
            scene->HandleInput(deltaTime);
            scene->Draw();

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
        for(auto it = fpsHistory.begin(); it != fpsHistory.end(); it++)
        {
            if(*it) std::cout << "FPS: " << *it << std::endl;
		}
    }


  
    catch (const std::exception& e)
    {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}

