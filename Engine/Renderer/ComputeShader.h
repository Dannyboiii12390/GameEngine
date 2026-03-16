#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <cstdint>

class VulkanRHI; // forward

// Generic compute shader helper that manages:
// - loading a compute SPIR-V module
// - creating N host-visible storage buffers
// - binding buffers to descriptor set
// - dispatching a compute shader with a push-constant uint count
//
// Usage pattern:
//   ComputeShader cs(rhi);
//   cs.LoadShader("Engine/SHADERS/add.comp.spv");
//   cs.PushConstants(&myData, mySize);            // optional: set push-constant data/size before CreateBuffers()
//   cs.CreateBuffers({ sizeA, sizeB, sizeOut });  // pipeline layout uses push-constant size set above
//   cs.Upload(0, dataA, sizeA);
//   cs.Upload(1, dataB, sizeB);
//   cs.Dispatch(numElements, localSizeX);         // if no push-constant data provided, numElements is pushed as uint32_t
//   cs.Readback(2, outData, sizeOut);
class ComputeShader
{
public:
    explicit ComputeShader(VulkanRHI* rhi);
    ~ComputeShader();

    // Load SPIR-V compute shader and create pipeline layout (no buffers yet)
    void LoadShader(const std::string& spvPath);

    // Create host-visible storage buffers. sizes vector length = number of buffers.
    void CreateBuffers(const std::vector<VkDeviceSize>& sizes);

    // Upload host data into a created buffer index
    void Upload(size_t bufferIndex, const void* data, VkDeviceSize size);

    // Dispatch compute shader. numElements is passed as push-constant uint if no custom push-constant set.
    // localSizeX must match the shader's layout(local_size_x = ...)
    void Dispatch(uint32_t numElements, uint32_t localSizeX = 256);

    // Read back buffer contents into dst (must match created size)
    void Readback(size_t bufferIndex, void* dst, VkDeviceSize size);

    // Set arbitrary push-constant data (call before CreateBuffers() to have pipeline layout match size).
    // If called after CreateBuffers(), size must equal the layout's push-constant size or else an exception is thrown.
    void PushConstants(const void* data, uint32_t size);
    void ClearPushConstants();

    void Destroy();

private:
    VulkanRHI* m_RHI;
    VkDevice m_Device = VK_NULL_HANDLE;
    VkPhysicalDevice m_Physical = VK_NULL_HANDLE;
    VkQueue m_Queue = VK_NULL_HANDLE;
    VkCommandPool m_CmdPool = VK_NULL_HANDLE;

    VkShaderModule m_ShaderModule = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_DescriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_Pipeline = VK_NULL_HANDLE;
    VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_DescriptorSet = VK_NULL_HANDLE;

    std::vector<VkBuffer> m_Buffers;
    std::vector<VkDeviceMemory> m_BuffersMemory;
    std::vector<VkDeviceSize> m_BufferSizes;

    // push-constant storage and declared size used when creating pipeline layout
    std::vector<uint8_t> m_PushConstants;
    uint32_t m_PushConstantSize = sizeof(uint32_t);

private:
    // Helper to clean up pipeline/descriptor/buffer related resources created by CreateBuffers.
    // Keeps the shader module intact so LoadShader can be called independently.
    void CleanupCreatedResources();

    uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
    void CreateBuffer(VkDeviceSize size, VkBuffer& buffer, VkDeviceMemory& memory);
};
