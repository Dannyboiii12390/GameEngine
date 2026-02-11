#pragma once

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <cstdint>

class VulkanRHI;
class Window;

class GUI final
{
public:
    GUI() = default;
    ~GUI() { Shutdown(); }

    bool Create(GLFWwindow* window,
        VkInstance instance,
        VkPhysicalDevice physicalDevice,
        VkDevice device,
        uint32_t queueFamily,
        VkQueue queue,
        VkPipelineCache pipelineCache,
        VkRenderPass renderPass,
        uint32_t imageCount,
        VkAllocationCallbacks* allocator = nullptr);

    bool Create(VulkanRHI& rhi, Window& window);

    void NewFrame() const;
    void Render(VkCommandBuffer commandBuffer) const;
    void Shutdown() noexcept;

    // Prevent copying
    GUI(const GUI&) = delete;
    GUI& operator=(const GUI&) = delete;

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkAllocationCallbacks* m_allocator = nullptr;
    bool m_initialized = false;
};
