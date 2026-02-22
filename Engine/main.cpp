#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>

#include <chrono>
#include <omp.h>

#include "Renderer/Window.h"
#include "Core/Managers/SceneManager.h"
#include "Core/Scenes/TemplateScene.h"

#ifdef _DEBUG
#define LOG_DEBUG(msg) std::cout << msg << std::endl;
#else
#define LOG_DEBUG(msg)
#endif

#include "../PhysicsEngine/Shapes/Sphere.h"

#include <fstream>

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

    addrinfo hints;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;      // IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;  // TCP

    addrinfo* addrResult = nullptr;
    result = getaddrinfo(host, port, &hints, &addrResult);
    if (result != 0) {
        std::cerr << "getaddrinfo failed: " << result << "\n";
        WSACleanup();
        return 1;
    }
    SOCKET sock = INVALID_SOCKET;
    for (addrinfo* ptr = addrResult; ptr != nullptr; ptr = ptr->ai_next) {
        sock = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (sock == INVALID_SOCKET) continue;
        if (connect(sock, ptr->ai_addr, static_cast<int>(ptr->ai_addrlen)) == 0) {
            break; // connected
        }
        closesocket(sock);
        sock = INVALID_SOCKET;
    }
    freeaddrinfo(addrResult);

    if (sock == INVALID_SOCKET) {
        std::cerr << "Unable to connect to server.\n";
        WSACleanup();
        return 1;
    }

    std::cout << "Connected to " << host << ":" << port << "\n";

    std::string line;
    const size_t bufSize = 1024;
    char recvBuf[bufSize];

    while (true) {
        std::cout << "Enter message (or 'quit' to exit): ";
        if (!std::getline(std::cin, line)) break;
        if (line == "quit") break;

        // Send the data (allow empty strings by sending at least a newline if desired).
        const char* data = line.c_str();
        int toSend = static_cast<int>(line.size());
        int sentTotal = 0;
        while (sentTotal < toSend) {
            int n = send(sock, data + sentTotal, toSend - sentTotal, 0);
            if (n == SOCKET_ERROR) {
                std::cerr << "send failed: " << WSAGetLastError() << "\n";
                closesocket(sock);
                WSACleanup();
                return 1;
            }
            sentTotal += n;
        }
        // Receive the echoed bytes. We expect the same number of bytes back.
        std::vector<char> received;
        received.reserve(toSend ? toSend : 1);
        int recvTotal = 0;
        while (recvTotal < toSend) {
            int n = recv(sock, recvBuf, static_cast<int>(bufSize), 0);
            if (n > 0) {
                received.insert(received.end(), recvBuf, recvBuf + n);
                recvTotal += n;
            }
            else if (n == 0) {
                std::cout << "Connection closed by server.\n";
                closesocket(sock);
                WSACleanup();
                return 0;
            }
            else {
                std::cerr << "recv failed: " << WSAGetLastError() << "\n";
                closesocket(sock);
                WSACleanup();
                return 1;
            }
        }
        std::string echoed(received.begin(), received.end());
        std::cout << "Echoed: " << echoed << "\n";
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}

int main()
{
    try
    {

        #ifdef _OPENMP
        std::cout << "OpenMP is enabled! Max threads: " << omp_get_max_threads() << std::endl;
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
                std::cout << "FPS: " << fps << std::endl; 
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
    }


  
    catch (const std::exception& e)
    {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}

