#pragma once

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include "../IMGUI/imgui.h"
#include "../IMGUI/imgui_impl_glfw.h"
#include "../IMGUI/imgui_impl_vulkan.h"
#include <cstdint>

namespace Engine
{
    class GUI final
    {
    public:
        GUI() = default;
        ~GUI() { shutdown(); }

        bool create(GLFWwindow* window,
            VkInstance instance,
            VkPhysicalDevice physicalDevice,
            VkDevice device,
            uint32_t queueFamily,
            VkQueue queue,
            VkPipelineCache pipelineCache,
            VkRenderPass renderPass,
            uint32_t imageCount,
            VkAllocationCallbacks* allocator = nullptr);

        void newFrame() const;
        void render(VkCommandBuffer commandBuffer) const;
        void shutdown() noexcept;

        // Prevent copying
        GUI(const GUI&) = delete;
        GUI& operator=(const GUI&) = delete;

    private:
        VkDevice m_device = VK_NULL_HANDLE;
        VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
        VkAllocationCallbacks* m_allocator = nullptr;
        bool m_initialized = false;
    };
}
