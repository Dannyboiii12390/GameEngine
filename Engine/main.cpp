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

void runComputeShader(VulkanRHI& rhi)
{
    constexpr uint32_t numElements = 1024;
    std::vector<int> elements(numElements);
    std::vector<int> elements2(numElements);
    for (uint32_t i = 0; i < numElements; ++i)
    {
        elements[i] = static_cast<int>(i);
        elements2[i] = static_cast<int>(i * 2);
    }
    std::vector<int> elementsOut(numElements, 0);

    VkDevice device = rhi.GetDevice();
    VkPhysicalDevice physical = rhi.GetPhysicalDevice();
    VkQueue queue = rhi.GetGraphicsQueue();
    VkCommandPool cmdPool = rhi.GetCommandPool();

    if (device == VK_NULL_HANDLE || physical == VK_NULL_HANDLE)
        throw std::runtime_error("runComputeShader: VulkanRHI not initialised");

    auto findMemoryType = [&](uint32_t typeFilter, VkMemoryPropertyFlags properties) -> uint32_t
        {
            VkPhysicalDeviceMemoryProperties memProperties;
            vkGetPhysicalDeviceMemoryProperties(physical, &memProperties);
            for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i)
            {
                if ((typeFilter & (1u << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
                    return i;
            }
            throw std::runtime_error("runComputeShader: Failed to find suitable memory type");
        };

    auto createHostBuffer = [&](VkDeviceSize size, VkBuffer& buffer, VkDeviceMemory& memory)
        {
            VkBufferCreateInfo bufInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
            bufInfo.size = size;
            bufInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            if (vkCreateBuffer(device, &bufInfo, nullptr, &buffer) != VK_SUCCESS)
                throw std::runtime_error("runComputeShader: failed to create buffer");

            VkMemoryRequirements memReq;
            vkGetBufferMemoryRequirements(device, buffer, &memReq);

            VkMemoryAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
            allocInfo.allocationSize = memReq.size;
            allocInfo.memoryTypeIndex = findMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

            if (vkAllocateMemory(device, &allocInfo, nullptr, &memory) != VK_SUCCESS)
                throw std::runtime_error("runComputeShader: failed to allocate buffer memory");

            vkBindBufferMemory(device, buffer, memory, 0);
        };

    const VkDeviceSize bufferSize = sizeof(int) * numElements;

    VkBuffer bufA = VK_NULL_HANDLE, bufB = VK_NULL_HANDLE, bufOut = VK_NULL_HANDLE;
    VkDeviceMemory memA = VK_NULL_HANDLE, memB = VK_NULL_HANDLE, memOut = VK_NULL_HANDLE;

    createHostBuffer(bufferSize, bufA, memA);
    createHostBuffer(bufferSize, bufB, memB);
    createHostBuffer(bufferSize, bufOut, memOut);

    // Upload input data
    void* mapped = nullptr;
    vkMapMemory(device, memA, 0, bufferSize, 0, &mapped);
    memcpy(mapped, elements.data(), static_cast<size_t>(bufferSize));
    vkUnmapMemory(device, memA);

    vkMapMemory(device, memB, 0, bufferSize, 0, &mapped);
    memcpy(mapped, elements2.data(), static_cast<size_t>(bufferSize));
    vkUnmapMemory(device, memB);

    // output already zeroed by vector; nothing to upload

    // Load SPIR-V
    std::ifstream file("SHADERS/add.comp.spv", std::ios::binary | std::ios::ate);
    if (!file.is_open())
        throw std::runtime_error("runComputeShader: failed to open shader SPV (Engine/SHADERS/add.comp.spv)");
    size_t codeSize = (size_t)file.tellg();
    file.seekg(0);
    std::vector<char> code(codeSize);
    file.read(code.data(), codeSize);
    file.close();

    VkShaderModule shaderModule = VK_NULL_HANDLE;
    {
        VkShaderModuleCreateInfo smci{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
        smci.codeSize = code.size();
        smci.pCode = reinterpret_cast<const uint32_t*>(code.data());
        if (vkCreateShaderModule(device, &smci, nullptr, &shaderModule) != VK_SUCCESS)
            throw std::runtime_error("runComputeShader: failed to create shader module");
    }

    // Descriptor set layout: 3 storage buffers
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    {
        std::array<VkDescriptorSetLayoutBinding, 3> bindings{};
        for (uint32_t i = 0; i < bindings.size(); ++i)
        {
            bindings[i].binding = i;
            bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            bindings[i].descriptorCount = 1;
            bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
            bindings[i].pImmutableSamplers = nullptr;
        }
        VkDescriptorSetLayoutCreateInfo dslci{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        dslci.bindingCount = static_cast<uint32_t>(bindings.size());
        dslci.pBindings = bindings.data();
        if (vkCreateDescriptorSetLayout(device, &dslci, nullptr, &descriptorSetLayout) != VK_SUCCESS)
            throw std::runtime_error("runComputeShader: failed to create descriptor set layout");
    }

    // Pipeline layout with push constant for uint count
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    {
        VkPushConstantRange pcr{};
        pcr.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pcr.offset = 0;
        pcr.size = sizeof(uint32_t);

        VkPipelineLayoutCreateInfo plci{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        plci.setLayoutCount = 1;
        plci.pSetLayouts = &descriptorSetLayout;
        plci.pushConstantRangeCount = 1;
        plci.pPushConstantRanges = &pcr;
        if (vkCreatePipelineLayout(device, &plci, nullptr, &pipelineLayout) != VK_SUCCESS)
            throw std::runtime_error("runComputeShader: failed to create pipeline layout");
    }

    // Compute pipeline
    VkPipeline pipeline = VK_NULL_HANDLE;
    {
        VkPipelineShaderStageCreateInfo stage{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
        stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stage.module = shaderModule;
        stage.pName = "main";

        VkComputePipelineCreateInfo cpci{ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
        cpci.stage = stage;
        cpci.layout = pipelineLayout;

        if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &cpci, nullptr, &pipeline) != VK_SUCCESS)
            throw std::runtime_error("runComputeShader: failed to create compute pipeline");
    }

    // Descriptor pool & set
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    {
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSize.descriptorCount = 3;

        VkDescriptorPoolCreateInfo dpci{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
        dpci.maxSets = 1;
        dpci.poolSizeCount = 1;
        dpci.pPoolSizes = &poolSize;
        if (vkCreateDescriptorPool(device, &dpci, nullptr, &descriptorPool) != VK_SUCCESS)
            throw std::runtime_error("runComputeShader: failed to create descriptor pool");

        VkDescriptorSetAllocateInfo dsai{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
        dsai.descriptorPool = descriptorPool;
        dsai.descriptorSetCount = 1;
        dsai.pSetLayouts = &descriptorSetLayout;
        if (vkAllocateDescriptorSets(device, &dsai, &descriptorSet) != VK_SUCCESS)
            throw std::runtime_error("runComputeShader: failed to allocate descriptor set");

        VkDescriptorBufferInfo bufInfos[3];
        bufInfos[0].buffer = bufA; bufInfos[0].offset = 0; bufInfos[0].range = bufferSize;
        bufInfos[1].buffer = bufB; bufInfos[1].offset = 0; bufInfos[1].range = bufferSize;
        bufInfos[2].buffer = bufOut; bufInfos[2].offset = 0; bufInfos[2].range = bufferSize;

        std::array<VkWriteDescriptorSet, 3> writes{};
        for (uint32_t i = 0; i < writes.size(); ++i)
        {
            writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[i].dstSet = descriptorSet;
            writes[i].dstBinding = i;
            writes[i].dstArrayElement = 0;
            writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            writes[i].descriptorCount = 1;
            writes[i].pBufferInfo = &bufInfos[i];
        }
        vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }

    // Command buffer
    VkCommandBufferAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = cmdPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(device, &allocInfo, &cmd) != VK_SUCCESS)
        throw std::runtime_error("runComputeShader: failed to allocate command buffer");

    VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

    // push constant = numElements
    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &numElements);

    // Dispatch
    constexpr uint32_t localSizeX = 256; // must match your compute shader's local_size_x
    uint32_t groups = (numElements + localSizeX - 1) / localSizeX;
    vkCmdDispatch(cmd, groups, 1, 1);

    // Barrier so host can read buffer after shader writes
    VkBufferMemoryBarrier bufferBarrier{ VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
    bufferBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    bufferBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferBarrier.buffer = bufOut;
    bufferBarrier.offset = 0;
    bufferBarrier.size = bufferSize;

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
        0, 0, nullptr, 1, &bufferBarrier, 0, nullptr);

    vkEndCommandBuffer(cmd);

    // Submit and wait
    VkFence fence = VK_NULL_HANDLE;
    VkFenceCreateInfo fci{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    vkCreateFence(device, &fci, nullptr, &fence);

    VkSubmitInfo submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    if (vkQueueSubmit(queue, 1, &submitInfo, fence) != VK_SUCCESS)
        throw std::runtime_error("runComputeShader: failed to submit compute work");

    vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
    vkDestroyFence(device, fence, nullptr);

    // Read back results
    vkMapMemory(device, memOut, 0, bufferSize, 0, &mapped);
    memcpy(elementsOut.data(), mapped, static_cast<size_t>(bufferSize));
    vkUnmapMemory(device, memOut);

    // Print a few results
    std::cout << "Compute shader results (first 8):\n";
    for (uint32_t i = 0; i < std::min<uint32_t>(8, numElements); ++i)
    {
        std::cout << i << ": " << elements[i] << " + " << elements2[i] << " = " << elementsOut[i] << "\n";
    }

    // Cleanup
    vkFreeCommandBuffers(device, cmdPool, 1, &cmd);
    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    vkDestroyPipeline(device, pipeline, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
    vkDestroyShaderModule(device, shaderModule, nullptr);

    vkDestroyBuffer(device, bufA, nullptr);
    vkFreeMemory(device, memA, nullptr);
    vkDestroyBuffer(device, bufB, nullptr);
    vkFreeMemory(device, memB, nullptr);
    vkDestroyBuffer(device, bufOut, nullptr);
    vkFreeMemory(device, memOut, nullptr);
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

        runComputeShader(vulkanRHI);

        GUI gui;

		sceneManager.AddScene(std::make_unique<BallDropScene>(window, &vulkanRHI, &gui));

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

