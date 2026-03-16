#include "ComputeShader.h"
#include "VulkanRHI.h"
#include <fstream>
#include <stdexcept>
#include <cstring>
#include <algorithm>

ComputeShader::ComputeShader(VulkanRHI* rhi)
    : m_RHI(rhi)
{
    if (!m_RHI) throw std::runtime_error("ComputeShader: VulkanRHI is null");
    m_Device = m_RHI->GetDevice();
    m_Physical = m_RHI->GetPhysicalDevice();
    m_Queue = m_RHI->GetGraphicsQueue();
    m_CmdPool = m_RHI->GetCommandPool();

    if (m_Device == VK_NULL_HANDLE || m_Physical == VK_NULL_HANDLE)
        throw std::runtime_error("ComputeShader: VulkanRHI not initialised");
}

ComputeShader::~ComputeShader()
{
    Destroy();
}

void ComputeShader::CleanupCreatedResources()
{
    // Ensure GPU is idle before destroying resources that might be in-flight
    if (m_Device != VK_NULL_HANDLE)
        vkDeviceWaitIdle(m_Device);

    if (m_Pipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(m_Device, m_Pipeline, nullptr);
        m_Pipeline = VK_NULL_HANDLE;
    }

    if (m_PipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(m_Device, m_PipelineLayout, nullptr);
        m_PipelineLayout = VK_NULL_HANDLE;
    }

    if (m_DescriptorPool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(m_Device, m_DescriptorPool, nullptr);
        m_DescriptorPool = VK_NULL_HANDLE;
    }

    if (m_DescriptorSetLayout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(m_Device, m_DescriptorSetLayout, nullptr);
        m_DescriptorSetLayout = VK_NULL_HANDLE;
    }

    // Destroy buffers and free memories.
    for (size_t i = 0; i < m_Buffers.size(); ++i)
    {
        if (m_Buffers[i] != VK_NULL_HANDLE)
        {
            vkDestroyBuffer(m_Device, m_Buffers[i], nullptr);
            m_Buffers[i] = VK_NULL_HANDLE;
        }
        if (i < m_BuffersMemory.size() && m_BuffersMemory[i] != VK_NULL_HANDLE)
        {
            vkFreeMemory(m_Device, m_BuffersMemory[i], nullptr);
            m_BuffersMemory[i] = VK_NULL_HANDLE;
        }
    }

    m_Buffers.clear();
    m_BuffersMemory.clear();
    m_BufferSizes.clear();

    m_DescriptorSet = VK_NULL_HANDLE;
    m_PushConstants.clear();
    m_PushConstantSize = sizeof(uint32_t);
}

void ComputeShader::LoadShader(const std::string& spvPath)
{
    // If a shader module already exists, destroy it before loading a new one (allows reloading)
    if (m_ShaderModule != VK_NULL_HANDLE)
    {
        if (m_Device != VK_NULL_HANDLE)
            vkDeviceWaitIdle(m_Device);
        vkDestroyShaderModule(m_Device, m_ShaderModule, nullptr);
        m_ShaderModule = VK_NULL_HANDLE;
    }

    // load SPV
    std::ifstream file(spvPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) throw std::runtime_error("ComputeShader: failed to open SPV: " + spvPath);
    size_t codeSize = static_cast<size_t>(file.tellg());
    file.seekg(0);
    std::vector<char> code(codeSize);
    file.read(code.data(), codeSize);
    file.close();

    VkShaderModuleCreateInfo smci{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    smci.codeSize = code.size();
    smci.pCode = reinterpret_cast<const uint32_t*>(code.data());
    if (vkCreateShaderModule(m_Device, &smci, nullptr, &m_ShaderModule) != VK_SUCCESS)
        throw std::runtime_error("ComputeShader: failed to create shader module");

    // If a pipeline was already created (CreateBuffers called earlier), recreate the pipeline with new module.
    if (m_PipelineLayout != VK_NULL_HANDLE)
    {
        if (m_Pipeline != VK_NULL_HANDLE)
        {
            vkDestroyPipeline(m_Device, m_Pipeline, nullptr);
            m_Pipeline = VK_NULL_HANDLE;
        }

        VkPipelineShaderStageCreateInfo stage{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
        stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stage.module = m_ShaderModule;
        stage.pName = "main";

        VkComputePipelineCreateInfo cpci{ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
        cpci.stage = stage;
        cpci.layout = m_PipelineLayout;
        if (vkCreateComputePipelines(m_Device, VK_NULL_HANDLE, 1, &cpci, nullptr, &m_Pipeline) != VK_SUCCESS)
            throw std::runtime_error("ComputeShader: failed to create compute pipeline");
    }

    // Note: descriptor set layout / pipeline layout are created in CreateBuffers where buffer count is known.
}

void ComputeShader::CreateBuffers(const std::vector<VkDeviceSize>& sizes)
{
    // Clean up any previous buffers/pipeline/descriptor resources to avoid leaks when CreateBuffers is called multiple times.
    CleanupCreatedResources();

    size_t count = sizes.size();
    m_Buffers.resize(count, VK_NULL_HANDLE);
    m_BuffersMemory.resize(count, VK_NULL_HANDLE);
    m_BufferSizes = sizes;

    for (size_t i = 0; i < count; ++i)
    {
        CreateBuffer(sizes[i], m_Buffers[i], m_BuffersMemory[i]);
    }

    // Descriptor set layout
    std::vector<VkDescriptorSetLayoutBinding> bindings(count);
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
    if (vkCreateDescriptorSetLayout(m_Device, &dslci, nullptr, &m_DescriptorSetLayout) != VK_SUCCESS)
        throw std::runtime_error("ComputeShader: failed to create descriptor set layout");

    // Pipeline layout with push constant size set from m_PushConstantSize
    VkPushConstantRange pcr{};
    pcr.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pcr.offset = 0;
    pcr.size = m_PushConstantSize; // allows arbitrary push-constant size configured by PushConstants()

    VkPipelineLayoutCreateInfo plci{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    plci.setLayoutCount = 1;
    plci.pSetLayouts = &m_DescriptorSetLayout;
    plci.pushConstantRangeCount = 1;
    plci.pPushConstantRanges = &pcr;
    if (vkCreatePipelineLayout(m_Device, &plci, nullptr, &m_PipelineLayout) != VK_SUCCESS)
        throw std::runtime_error("ComputeShader: failed to create pipeline layout");

    // Compute pipeline
    if (m_ShaderModule == VK_NULL_HANDLE)
        throw std::runtime_error("ComputeShader: shader module not loaded (call LoadShader first)");

    VkPipelineShaderStageCreateInfo stage{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
    stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = m_ShaderModule;
    stage.pName = "main";

    VkComputePipelineCreateInfo cpci{ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
    cpci.stage = stage;
    cpci.layout = m_PipelineLayout;
    if (vkCreateComputePipelines(m_Device, VK_NULL_HANDLE, 1, &cpci, nullptr, &m_Pipeline) != VK_SUCCESS)
        throw std::runtime_error("ComputeShader: failed to create compute pipeline");

    // Descriptor pool and set
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize.descriptorCount = static_cast<uint32_t>(count);

    VkDescriptorPoolCreateInfo dpci{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    dpci.maxSets = 1;
    dpci.poolSizeCount = 1;
    dpci.pPoolSizes = &poolSize;
    if (vkCreateDescriptorPool(m_Device, &dpci, nullptr, &m_DescriptorPool) != VK_SUCCESS)
        throw std::runtime_error("ComputeShader: failed to create descriptor pool");

    VkDescriptorSetAllocateInfo dsai{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    dsai.descriptorPool = m_DescriptorPool;
    dsai.descriptorSetCount = 1;
    dsai.pSetLayouts = &m_DescriptorSetLayout;
    if (vkAllocateDescriptorSets(m_Device, &dsai, &m_DescriptorSet) != VK_SUCCESS)
        throw std::runtime_error("ComputeShader: failed to allocate descriptor set");

    std::vector<VkDescriptorBufferInfo> bufInfos(count);
    for (size_t i = 0; i < count; ++i)
    {
        bufInfos[i].buffer = m_Buffers[i];
        bufInfos[i].offset = 0;
        bufInfos[i].range = sizes[i];
    }

    std::vector<VkWriteDescriptorSet> writes(count);
    for (size_t i = 0; i < count; ++i)
    {
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = m_DescriptorSet;
        writes[i].dstBinding = static_cast<uint32_t>(i);
        writes[i].dstArrayElement = 0;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[i].descriptorCount = 1;
        writes[i].pBufferInfo = &bufInfos[i];
    }
    vkUpdateDescriptorSets(m_Device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void ComputeShader::PushConstants(const void* data, uint32_t size)
{
    if (size == 0)
    {
        ClearPushConstants();
        return;
    }

    // If a pipeline layout already exists, ensure the requested push constant size matches it.
    if (m_PipelineLayout != VK_NULL_HANDLE && size != m_PushConstantSize)
    {
        throw std::runtime_error("ComputeShader::PushConstants: size does not match existing pipeline layout push-constant size");
    }

    m_PushConstants.assign(reinterpret_cast<const uint8_t*>(data), reinterpret_cast<const uint8_t*>(data) + size);
    m_PushConstantSize = size;
}

void ComputeShader::ClearPushConstants()
{
    m_PushConstants.clear();
    m_PushConstantSize = sizeof(uint32_t);
}

void ComputeShader::Upload(size_t bufferIndex, const void* data, VkDeviceSize size)
{
    if (bufferIndex >= m_BuffersMemory.size()) throw std::out_of_range("ComputeShader::Upload: invalid buffer index");
    if (size > m_BufferSizes[bufferIndex]) throw std::runtime_error("ComputeShader::Upload: size exceeds buffer size");
    void* mapped = nullptr;
    vkMapMemory(m_Device, m_BuffersMemory[bufferIndex], 0, size, 0, &mapped);
    std::memcpy(mapped, data, static_cast<size_t>(size));
    vkUnmapMemory(m_Device, m_BuffersMemory[bufferIndex]);
}

void ComputeShader::Readback(size_t bufferIndex, void* dst, VkDeviceSize size)
{
    if (bufferIndex >= m_BuffersMemory.size()) throw std::out_of_range("ComputeShader::Readback: invalid buffer index");
    if (size > m_BufferSizes[bufferIndex]) throw std::runtime_error("ComputeShader::Readback: size exceeds buffer size");
    void* mapped = nullptr;
    vkMapMemory(m_Device, m_BuffersMemory[bufferIndex], 0, size, 0, &mapped);
    std::memcpy(dst, mapped, static_cast<size_t>(size));
    vkUnmapMemory(m_Device, m_BuffersMemory[bufferIndex]);
}

void ComputeShader::Dispatch(uint32_t numElements, uint32_t localSizeX)
{
    // allocate command buffer
    VkCommandBufferAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_CmdPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(m_Device, &allocInfo, &cmd) != VK_SUCCESS)
        throw std::runtime_error("ComputeShader: failed to allocate command buffer");

    VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_Pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_PipelineLayout, 0, 1, &m_DescriptorSet, 0, nullptr);

    // push constants: use user-specified data if present, otherwise push numElements as uint32_t
    if (!m_PushConstants.empty())
    {
        vkCmdPushConstants(cmd, m_PipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, m_PushConstantSize, m_PushConstants.data());
    }
    else
    {
        vkCmdPushConstants(cmd, m_PipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &numElements);
    }

    uint32_t groups = (numElements + localSizeX - 1) / localSizeX;
    vkCmdDispatch(cmd, groups, 1, 1);

    // barrier so host can read buffers written by shader
    // add barrier for each buffer we might read on host (simplified: barrier on all buffers)
    std::vector<VkBufferMemoryBarrier> barriers(m_Buffers.size());
    for (size_t i = 0; i < m_Buffers.size(); ++i)
    {
        barriers[i].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barriers[i].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barriers[i].dstAccessMask = VK_ACCESS_HOST_READ_BIT;
        barriers[i].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[i].buffer = m_Buffers[i];
        barriers[i].offset = 0;
        barriers[i].size = m_BufferSizes[i];
    }

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
        0, 0, nullptr,
        static_cast<uint32_t>(barriers.size()), barriers.data(),
        0, nullptr);

    vkEndCommandBuffer(cmd);

    VkFence fence = VK_NULL_HANDLE;
    VkFenceCreateInfo fci{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    vkCreateFence(m_Device, &fci, nullptr, &fence);

    VkSubmitInfo submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    if (vkQueueSubmit(m_Queue, 1, &submitInfo, fence) != VK_SUCCESS)
        throw std::runtime_error("ComputeShader: failed to submit compute work");

    vkWaitForFences(m_Device, 1, &fence, VK_TRUE, UINT64_MAX);
    vkDestroyFence(m_Device, fence, nullptr);

    vkFreeCommandBuffers(m_Device, m_CmdPool, 1, &cmd);
}

uint32_t ComputeShader::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(m_Physical, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i)
    {
        if ((typeFilter & (1u << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
            return i;
    }
    throw std::runtime_error("ComputeShader: failed to find memory type");
}

void ComputeShader::CreateBuffer(VkDeviceSize size, VkBuffer& buffer, VkDeviceMemory& memory)
{
    VkBufferCreateInfo bufInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufInfo.size = size;
    bufInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(m_Device, &bufInfo, nullptr, &buffer) != VK_SUCCESS)
        throw std::runtime_error("ComputeShader: failed to create buffer");

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(m_Device, buffer, &memReq);

    VkMemoryAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(m_Device, &allocInfo, nullptr, &memory) != VK_SUCCESS)
        throw std::runtime_error("ComputeShader: failed to allocate buffer memory");

    vkBindBufferMemory(m_Device, buffer, memory, 0);
}

void ComputeShader::Destroy()
{
    // Ensure GPU is idle before destroying any device objects
    if (m_Device != VK_NULL_HANDLE)
        vkDeviceWaitIdle(m_Device);

    // Cleanup everything created by CreateBuffers (buffers, descriptors, pipeline)
    CleanupCreatedResources();

    // Destroy shader module last (keeps consistent ordering)
    if (m_ShaderModule != VK_NULL_HANDLE)
    {
        vkDestroyShaderModule(m_Device, m_ShaderModule, nullptr);
        m_ShaderModule = VK_NULL_HANDLE;
    }

    // Reset state
    m_PushConstants.clear();
    m_PushConstantSize = sizeof(uint32_t);
}