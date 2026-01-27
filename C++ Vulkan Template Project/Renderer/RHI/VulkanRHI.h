#pragma once

#include "IRHI.h"
#include <vulkan/vulkan.h>
#include <vector>

class Window;

class VulkanRHI : public IRHI
{
public:
    void Initialise(Window* window) override;
    void Shutdown() override;

    void BeginFrame() override;
    void EndFrame() override;
    void Present() override;
    void WaitIdle() override;

    void CreateDevice() override;
    void CreateSwapchain() override;

    
    void CreateCommandQueue();
    void CreateCommandBuffer();
    void CreateBuffer(const RHI::BufferDesc& desc);
    void CreateTexture(const RHI::TextureDesc& desc);
    void CreateShader(const RHI::ShaderDesc& desc);
    void CreatePipelineState(const RHI::PipelineDesc& desc);
    void CreateDescriptorSet(const RHI::DescriptorDesc& desc);
    

private:
    void CreateInstance();
    void CreateSurface(Window* window);
    void PickPhysicalDevice();
    void CreateCommandPool();
    void AllocateCommandBuffers();
    void CreateSyncObjects();
    void CreateSwapchainImageViews();

    //helpers
    uint32_t FindGraphicsQueueFamily();
    VkSurfaceFormatKHR ChooseSurfaceFormat();
    VkPresentModeKHR ChoosePresentMode();

private:
    VkInstance m_Instance{};
    VkPhysicalDevice m_PhysicalDevice{};
    VkDevice m_Device{};
    VkSurfaceKHR m_Surface{};
    VkSwapchainKHR m_Swapchain{};

    VkQueue m_GraphicsQueue{};
    uint32_t m_GraphicsQueueFamily{};

    VkCommandPool m_CommandPool{};
    std::vector<VkCommandBuffer> m_CommandBuffers;

    VkSemaphore m_ImageAvailable{};
    VkSemaphore m_RenderFinished{};
    VkFence m_InFlightFence{};

    uint32_t m_CurrentImageIndex = 0;

    VkFormat m_SwapchainFormat{};
    VkExtent2D m_SwapchainExtent{};
    std::vector<VkImage> m_SwapchainImages;
    std::vector<VkImageView> m_SwapchainImageViews;

    uint32_t m_CurrentFrame = 0;
};
