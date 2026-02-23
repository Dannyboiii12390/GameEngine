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

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <vector>
#include "Core/Scenes/CollideBallWithAnotherBallScene.h"

#include "DebugUtils.h"

#include "../ServerWSock2/Sockets/TCPSocket.h"
#include "../ServerWSock2/Packet.h"
#pragma comment(lib, "Ws2_32.lib")

int clientRequest()
{
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        std::cerr << "WSAStartup failed: " << result << "\n";
        return 1;
    }

    const char* host = "127.0.0.1";
    const char* port = "54000";

    Networking::TCPSocket tcp;
    tcp.connect(host, port);
    if (!tcp.isConnected())
    {
        std::cerr << "Unable to connect to server: " << host << ":" << port << "\n";
        WSACleanup();
        return 1;
    }

    std::cout << "Connected to " << host << ":" << port << "\n";

    std::string line;
    constexpr size_t bufSize = 1024;
    std::vector<char> recvBuf(bufSize);

    // Buffer for assembling incoming TCP stream into packets
    std::vector<uint8_t> tcpRecvBuffer;

    while (true) {
        std::cout << "Enter message (or 'quit' to exit): ";
        if (!std::getline(std::cin, line)) break;
        if (line == "quit") break;

        const char* data = line.data();
        int toSend = static_cast<int>(line.size());

        // Build a Packet and serialize it
        Networking::Packet pkt(1); // arbitrary type 1 for application message
        if (toSend > 0) {
            pkt.setPayload(data, static_cast<std::size_t>(toSend));
        }
        else {
            pkt.setPayload(nullptr, 0);
        }

        std::vector<uint8_t> out;
        pkt.serialize(out);

        if (out.empty()) {
            std::cerr << "Failed to serialize packet\n";
            continue;
        }

        // Send the serialized packet (handle partial sends)
        const char* sendBuf = reinterpret_cast<const char*>(out.data());
        int total = static_cast<int>(out.size());
        int sent = 0;
        while (sent < total) {
            int n = tcp.send(sendBuf + sent, total - sent) ? (total - sent) : SOCKET_ERROR;
            // Note: tcp.send already handles full-send in original API. If it returns true we assume all bytes sent.
            if (n == SOCKET_ERROR) {
                std::cerr << "send failed (TCPSocket)\n";
                tcp.disconnect();
                WSACleanup();
                return 1;
            }
            sent = total;
        }

        // Wait for echoed Packet. Accumulate stream bytes until Packet::tryDeserializeFromBuffer succeeds.
        Networking::Packet echoedPkt;
        while (true) {
            // First attempt to parse any already-received bytes
            if (Networking::Packet::tryDeserializeFromBuffer(tcpRecvBuffer, echoedPkt)) {
                break;
            }

            int n = tcp.receive(recvBuf.data(), static_cast<int>(bufSize));
            if (n > 0) {
                tcpRecvBuffer.insert(tcpRecvBuffer.end(), recvBuf.data(), recvBuf.data() + n);
            }
            else if (n == 0) {
                std::cout << "Connection closed by server.\n";
                tcp.disconnect();
                WSACleanup();
                return 0;
            }
            else {
                std::cerr << "recv failed (TCPSocket)\n";
                tcp.disconnect();
                WSACleanup();
                return 1;
            }
        }

        std::string echoed(reinterpret_cast<const char*>(echoedPkt.payload.data()), echoedPkt.payload.size());
        std::cout << "Echoed: " << echoed << "\n";
    }

    tcp.disconnect();
    WSACleanup();
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

