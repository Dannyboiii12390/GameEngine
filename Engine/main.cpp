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
#include <mutex>
#include "../PhysicsEngine/Threading/ThreadPool.h"
#pragma comment(lib, "Ws2_32.lib")

int clientRequest()
{
    constexpr int numClients = 10;

    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        std::cerr << "WSAStartup failed: " << result << "\n";
        return 1;
    }

    const char* host = "127.0.0.1";
    const char* port = "54000";

    std::mutex coutMutex;
    std::atomic<int> failures{ 0 };

    Threading::ThreadPool threadpool(2);
    std::vector<std::future<void>> futures;
    futures.reserve(numClients);

    for (int id = 0; id < numClients; ++id)
    {
        futures.push_back(threadpool.Enqueue([id, host, port, &coutMutex, &failures]()
            {
                Networking::TCPSocket tcp;
                tcp.connect(host, port);
                if (!tcp.isConnected())
                {
                    std::lock_guard<std::mutex> lk(coutMutex);
                    std::cerr << "Client " << id << " - Unable to connect to server: " << host << ":" << port << "\n";
                    ++failures;
                    return;
                }

                {
                    std::lock_guard<std::mutex> lk(coutMutex);
                    std::cout << "Client " << id << " - Connected to " << host << ":" << port << "\n";
                }

                // Prepare a message unique to this client
                std::string message = "Hello from client " + std::to_string(id);

                Networking::Packet pkt(1); // application message type
                if (!message.empty()) {
                    pkt.setPayload(message.data(), static_cast<std::size_t>(message.size()));
                }
                else {
                    pkt.setPayload(nullptr, 0);
                }

                std::vector<uint8_t> out;
                pkt.serialize(out);

                if (out.empty()) {
                    std::lock_guard<std::mutex> lk(coutMutex);
                    std::cerr << "Client " << id << " - Failed to serialize packet\n";
                    ++failures;
                    tcp.disconnect();
                    return;
                }

                // Send serialized packet (TCPSocket::send is used similarly to original code)
                const char* sendBuf = reinterpret_cast<const char*>(out.data());
                int total = static_cast<int>(out.size());
                int sent = 0;
                while (sent < total) {
                    int n = tcp.send(sendBuf + sent, total - sent) ? (total - sent) : SOCKET_ERROR;
                    // Note: the wrapper used in this project returns true when send succeeded for the whole buffer.
                    if (n == SOCKET_ERROR) {
                        std::lock_guard<std::mutex> lk(coutMutex);
                        std::cerr << "Client " << id << " - send failed (TCPSocket)\n";
                        ++failures;
                        tcp.disconnect();
                        return;
                    }
                    sent = total;
                }

                // Wait for echoed Packet. Accumulate stream bytes until Packet::tryDeserializeFromBuffer succeeds.
                constexpr size_t bufSize = 1024;
                std::vector<char> recvBuf(bufSize);
                std::vector<uint8_t> tcpRecvBuffer;
                Networking::Packet echoedPkt;

                while (true) {
                    if (Networking::Packet::tryDeserializeFromBuffer(tcpRecvBuffer, echoedPkt)) {
                        break;
                    }

                    int n = tcp.receive(recvBuf.data(), static_cast<int>(bufSize));
                    if (n > 0) {
                        tcpRecvBuffer.insert(tcpRecvBuffer.end(), recvBuf.data(), recvBuf.data() + n);
                    }
                    else if (n == 0) {
                        std::lock_guard<std::mutex> lk(coutMutex);
                        std::cout << "Client " << id << " - Connection closed by server.\n";
                        tcp.disconnect();
                        ++failures;
                        return;
                    }
                    else {
                        std::lock_guard<std::mutex> lk(coutMutex);
                        std::cerr << "Client " << id << " - recv failed (TCPSocket)\n";
                        ++failures;
                        tcp.disconnect();
                        return;
                    }
                }

                std::string echoed(reinterpret_cast<const char*>(echoedPkt.payload.data()), echoedPkt.payload.size());
                {
                    std::lock_guard<std::mutex> lk(coutMutex);
                    std::cout << "Client " << id << " - Echoed: " << echoed << "\n";
                }

                tcp.disconnect();
            }));
    }

    // Join all threads
    for (auto& t : futures) {
        t.get();
    }

    WSACleanup();
    return (failures.load() == 0) ? 0 : 1;
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

