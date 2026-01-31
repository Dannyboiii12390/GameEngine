#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <vector>

class VulkanRHI;

/// Lightweight wrapper for a Vulkan graphics pipeline (shader modules, pipeline layout, VkPipeline).
/// - Creates a simple pipeline suitable for rendering meshes.
/// - Exposes Bind/Destroy and getters for pipeline/layout.
/// - Loads SPIR-V shader files from disk.
class Pipeline
{
public:
    Pipeline();
    ~Pipeline();

    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;

    Pipeline(Pipeline&&) noexcept;
    Pipeline& operator=(Pipeline&&) noexcept;

    // Initialize the pipeline.
    // - rhi: pointer to VulkanRHI (used to obtain VkDevice/queues/etc).
    // - renderPass: VkRenderPass to be used by the pipeline (ownership remains with caller/RHI).
    // - extent: swapchain extent (used for viewport/scissor).
    // - vertSpvPath/fragSpvPath: filesystem paths to SPIR-V shader files.
    // Returns true on success.
    bool Initialize(VulkanRHI* rhi, VkRenderPass renderPass, VkExtent2D extent, const std::string& vertSpvPath, const std::string& fragSpvPath);

    // Release GPU resources
    void Destroy();

    // Bind pipeline for drawing
    void Bind(VkCommandBuffer cmd) const;

    // Convenience wrapper to push constants using this pipeline's layout.
    // Requires that pipeline layout was created with a matching VkPushConstantRange.
    void PushConstants(VkCommandBuffer cmd, VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void* pValues) const
    {
        if (cmd == VK_NULL_HANDLE || m_PipelineLayout == VK_NULL_HANDLE || pValues == nullptr || size == 0)
            return;
        vkCmdPushConstants(cmd, m_PipelineLayout, stageFlags, offset, size, pValues);
    }

    VkPipeline GetPipeline() const { return m_Pipeline; }
    VkPipelineLayout GetLayout() const { return m_PipelineLayout; }
	VkRenderPass GetRenderPass() const { return m_RenderPass; }

    bool IsValid() const { return m_Device != VK_NULL_HANDLE && m_Pipeline != VK_NULL_HANDLE; }

private:
    std::vector<char> ReadFile(const std::string& path) const;
    VkShaderModule CreateShaderModule(const std::vector<char>& code) const;
    bool CreateGraphicsPipeline(const std::vector<char>& vertCode, const std::vector<char>& fragCode);

private:
    VulkanRHI* m_RHI = nullptr;
    VkDevice m_Device = VK_NULL_HANDLE;
    VkPipeline m_Pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
    VkShaderModule m_VertModule = VK_NULL_HANDLE;
    VkShaderModule m_FragModule = VK_NULL_HANDLE;
    VkRenderPass m_RenderPass = VK_NULL_HANDLE;
    VkExtent2D m_Extent{};
};