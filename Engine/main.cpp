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

#include "flatbuffers/flatbuffers.h"
#include "Core/Scenes/RotationScene.h"
#include "Core/Scenes/BallDropScene.h"
#include <numeric>
#include "Renderer/ComputeShader.h"
#include "Core/Scenes/PanningScene.h"
// add a toString class to every collider

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
        // Build a FlatBuffers buffer whose root is a string.
        // This avoids needing a generated schema while still using FlatBuffers.
        flatbuffers::FlatBufferBuilder builder;
        auto strOff = builder.CreateString(msg);
        builder.Finish(strOff); // root is the string itself

        // Create packet and set payload (updates header.payloadSize)
        Networking::Packet pkt;
        pkt.header.type = 1; // example type
        pkt.setPayload(builder.GetBufferPointer(), builder.GetSize());

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
                    // Try to interpret payload as a FlatBuffers root string.
                    // If that fails, fall back to raw bytes as string.
                    const uint8_t* p = incoming.payloadData();
                    // flatbuffers::GetRoot<T> is valid for built buffers whose root is T
                    const flatbuffers::String* fbStr = flatbuffers::GetRoot<flatbuffers::String>(p);
                    if (fbStr && fbStr->c_str()) {
                        payload.assign(fbStr->c_str());
                    }
                    else {
                        payload.assign(reinterpret_cast<const char*>(p), incoming.payloadSize());
                    }
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

void runComputeShader(VulkanRHI& vulkanRHI)
{
	glm::vec3 zeroVec(0.0f);
        constexpr uint32_t numEntities = 1024;
        const float dt = 1.0f; // simulate a single frame step

        std::vector<Entity> entities;
        entities.reserve(numEntities);

        // Host-side contiguous GPU-friendly arrays
        std::vector<glm::vec4> positions;
        std::vector<glm::vec4> velocities;
        positions.reserve(numEntities);
        velocities.reserve(numEntities);

        // Create entities and populate host arrays
        for (uint32_t i = 0; i < numEntities; ++i)
        {
            glm::vec3 pos(static_cast<float>(i), 0.0f, 0.0f);
            glm::vec3 vel(0.0f, static_cast<float>(i) * 0.1f, 0.0f);

            Entity e;
            e.AddComponent(EComponentType::Component_Transform, pos, glm::vec3(0.0f), glm::vec3(1.0f));
            e.AddComponent(EComponentType::Component_Velocity, vel, glm::vec3(0.0f), glm::vec3(1.0f));
            entities.push_back(std::move(e));

            positions.emplace_back(pos.x, pos.y, pos.z, 0.0f);
            velocities.emplace_back(vel.x, vel.y, vel.z, 0.0f);
        }

        // Output buffer for GPU results
        std::vector<glm::vec4> positionsOut;
        positionsOut.resize(numEntities);

        // Create and configure compute shader
        ComputeShader cs(&vulkanRHI);
        cs.LoadShader("SHADERS/translate_by_vel.comp.spv"); // shader must implement out[i] = inPos[i] + inVel[i] * dt

        VkDeviceSize bufSize = sizeof(glm::vec4) * numEntities;
        // Bindings: 0 = inPos, 1 = inVel, 2 = outPos
        cs.CreateBuffers({ bufSize, bufSize, bufSize });

        // Upload initial data
        cs.Upload(0, positions.data(), bufSize);
        cs.Upload(1, velocities.data(), bufSize);

        // If shader expects dt as push-constant you would set it via the ComputeShader API.
        // For this example we assume the shader uses a specialization constant or a hardcoded dt.
        // If the ComputeShader supports push-constants, you would call something like:
        // cs.PushConstants(&dt, sizeof(dt));

        // Dispatch: choose local size consistent with shader. Use 256 as a common local size.
        constexpr uint32_t localSizeX = 256;
        cs.Dispatch(numEntities, localSizeX);

        // Read back results
        cs.Readback(2, positionsOut.data(), bufSize);

        // Apply results back to components. For portability, remove and re-add transform component with new position.
        for (uint32_t i = 0; i < numEntities; ++i)
        {
            const glm::vec4& p = positionsOut[i];
            glm::vec3 newPos(p.x, p.y, p.z);

            // Replace transform component with updated position (preserve rotation/scale defaults)
            auto* transform = entities[i].GetComponent<ComponentTransform>(EComponentType::Component_Transform);
            transform->SetPosition(newPos);
        }

        for(auto& entity : entities)
        {
            auto* transform = entity.GetComponent<ComponentTransform>(EComponentType::Component_Transform);
            auto* velocity = entity.GetComponent<ComponentVelocity>(EComponentType::Component_Velocity);

            transform->SwapBuffers();
            velocity->SwapBuffers();
        }
        
        // Example debug print of first few results
        std::cout << "Translated positions (first 8):\n";
        for (uint32_t i = 0; i < std::min<uint32_t>(8, numEntities); ++i)
        {
            const glm::vec4& p = positionsOut[i];
            std::cout << i << ": (" << p.x << ", " << p.y << ", " << p.z << ")\n";
        }

        //cs.Destroy();
    
    




    // Example usage of ComputeShader class to add two int arrays
    constexpr uint32_t numElements = 1024;
    std::vector<int> elements(numElements);
    std::vector<int> elements2(numElements);
    for (uint32_t i = 0; i < numElements; ++i)
    {
        elements[i] = static_cast<int>(i);
        elements2[i] = static_cast<int>(i * 2);
    }
    std::vector<int> elementsOut(numElements, 0);

    //ComputeShader cs(&vulkanRHI);
    cs.LoadShader("SHADERS/add.comp.spv");

    bufSize = sizeof(int) * numElements;
    cs.CreateBuffers({ bufSize, bufSize, bufSize }); // inA, inB, out

    cs.Upload(0, elements.data(), bufSize);
    cs.Upload(1, elements2.data(), bufSize);

    // Dispatch and read back
    //constexpr uint32_t localSizeX = 256; // must match shader local_size_x
    cs.Dispatch(numElements, localSizeX);

    cs.Readback(2, elementsOut.data(), bufSize);

    // Print some results
    std::cout << "Compute shader results (first 8):\n";
    for (uint32_t i = 0; i < std::min<uint32_t>(8, numElements); ++i)
    {
        std::cout << i << ": " << elements[i] << " + " << elements2[i] << " = " << elementsOut[i] << "\n";
    }

    cs.Destroy();
}


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

        const int width = 1920;
        const int height = 1080;
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

        runComputeShader(vulkanRHI);

        GUI gui;

        sceneManager.AddScene(std::make_unique<PanningScene>(window, &vulkanRHI, &gui));

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

