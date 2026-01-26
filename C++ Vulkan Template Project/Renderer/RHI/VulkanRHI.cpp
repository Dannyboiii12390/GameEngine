#include "VulkanRHI.h"
#include <stdexcept>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "../../Graphics/Window.h"


void VulkanRHI::Initialise(Window* window)
{
    CreateInstance();
    CreateSurface(window);
    PickPhysicalDevice();
    CreateDevice();
    CreateSwapchain();
	CreateSwapchainImageViews();
    CreateCommandPool();
    AllocateCommandBuffers();
    CreateSyncObjects();
}
void VulkanRHI::BeginFrame()
{
    vkWaitForFences(m_Device, 1, &m_InFlightFence, VK_TRUE, UINT64_MAX);
    vkResetFences(m_Device, 1, &m_InFlightFence);

    vkAcquireNextImageKHR(
        m_Device,
        m_Swapchain,
        UINT64_MAX,
        m_ImageAvailable,
        VK_NULL_HANDLE,
        &m_CurrentImageIndex
    );

    VkCommandBuffer cmd = m_CommandBuffers[0];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    vkBeginCommandBuffer(cmd, &beginInfo);
}
void VulkanRHI::EndFrame()
{
    VkCommandBuffer cmd = m_CommandBuffers[0];
    vkEndCommandBuffer(cmd);

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &m_ImageAvailable;
    submitInfo.pWaitDstStageMask = &waitStage;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &m_RenderFinished;

    vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, m_InFlightFence);
}
void VulkanRHI::Present()
{
    VkPresentInfoKHR presentInfo{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &m_RenderFinished;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &m_Swapchain;
    presentInfo.pImageIndices = &m_CurrentImageIndex;

    vkQueuePresentKHR(m_GraphicsQueue, &presentInfo);
}
void VulkanRHI::Shutdown()
{
    vkDeviceWaitIdle(m_Device);

    vkDestroyFence(m_Device, m_InFlightFence, nullptr);
    vkDestroySemaphore(m_Device, m_RenderFinished, nullptr);
    vkDestroySemaphore(m_Device, m_ImageAvailable, nullptr);

    vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);
    vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);
    vkDestroyDevice(m_Device, nullptr);
    vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
    vkDestroyInstance(m_Instance, nullptr);
}
void VulkanRHI::WaitIdle()
{
    vkDeviceWaitIdle(m_Device);
}





// --- Private Methods ---

void VulkanRHI::CreateInstance()
{
    VkApplicationInfo appInfo{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
    appInfo.pApplicationName = "Custom Engine";
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo createInfo{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    createInfo.pApplicationInfo = &appInfo;

    // Ask GLFW which instance extensions are required for window surface support
    uint32_t glfwExtCount = 0;
    const char** glfwExt = glfwGetRequiredInstanceExtensions(&glfwExtCount);
    if (!glfwExt || glfwExtCount == 0)
        throw std::runtime_error("GLFW failed to provide required Vulkan instance extensions");

    createInfo.enabledExtensionCount = glfwExtCount;
    createInfo.ppEnabledExtensionNames = glfwExt;

    if (vkCreateInstance(&createInfo, nullptr, &m_Instance) != VK_SUCCESS)
        throw std::runtime_error("Failed to create Vulkan instance");
}
void VulkanRHI::PickPhysicalDevice()
{
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);

    if (deviceCount == 0)
        throw std::runtime_error("No Vulkan devices found");

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices.data());

    m_PhysicalDevice = devices[0]; // MVP: pick first valid
}
void VulkanRHI::CreateDevice()
{
    m_GraphicsQueueFamily = FindGraphicsQueueFamily();

    float priority = 1.0f;

    VkDeviceQueueCreateInfo queueInfo{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
    queueInfo.queueFamilyIndex = m_GraphicsQueueFamily;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &priority;

    VkDeviceCreateInfo createInfo{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    createInfo.queueCreateInfoCount = 1;
    createInfo.pQueueCreateInfos = &queueInfo;

    // Enable swapchain extension (required to create a swapchain)
    const char* deviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    createInfo.enabledExtensionCount = 1;
    createInfo.ppEnabledExtensionNames = deviceExtensions;

    if (vkCreateDevice(m_PhysicalDevice, &createInfo, nullptr, &m_Device) != VK_SUCCESS)
        throw std::runtime_error("Failed to create Vulkan device");

    vkGetDeviceQueue(m_Device, m_GraphicsQueueFamily, 0, &m_GraphicsQueue);
}
void VulkanRHI::CreateCommandPool()
{
    VkCommandPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = m_GraphicsQueueFamily;

    vkCreateCommandPool(m_Device, &poolInfo, nullptr, &m_CommandPool);
}
void VulkanRHI::AllocateCommandBuffers()
{
    m_CommandBuffers.resize(1);

    VkCommandBufferAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    allocInfo.commandPool = m_CommandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    vkAllocateCommandBuffers(m_Device, &allocInfo, m_CommandBuffers.data());
}
void VulkanRHI::CreateSwapchain()
{
    VkSurfaceCapabilitiesKHR caps{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_PhysicalDevice, m_Surface, &caps);

    VkSurfaceFormatKHR surfaceFormat = ChooseSurfaceFormat();
    VkPresentModeKHR presentMode = ChoosePresentMode();

    m_SwapchainFormat = surfaceFormat.format;
    m_SwapchainExtent = caps.currentExtent;

    VkSwapchainCreateInfoKHR info{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    info.surface = m_Surface;
    info.minImageCount = caps.minImageCount + 1;
    info.imageFormat = surfaceFormat.format;
    info.imageColorSpace = surfaceFormat.colorSpace;
    info.imageExtent = m_SwapchainExtent;
    info.imageArrayLayers = 1;
    info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.preTransform = caps.currentTransform;
    info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    info.presentMode = presentMode;
    info.clipped = VK_TRUE;

    if (vkCreateSwapchainKHR(m_Device, &info, nullptr, &m_Swapchain) != VK_SUCCESS)
        throw std::runtime_error("Failed to create swapchain");

    uint32_t count = 0;
    vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &count, nullptr);
    m_SwapchainImages.resize(count);
    vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &count, m_SwapchainImages.data());
}
void VulkanRHI::CreateSwapchainImageViews()
{
    m_SwapchainImageViews.resize(m_SwapchainImages.size());

    for (size_t i = 0; i < m_SwapchainImages.size(); ++i)
    {
        VkImageViewCreateInfo viewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        viewInfo.image = m_SwapchainImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = m_SwapchainFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;

        vkCreateImageView(
            m_Device, &viewInfo, nullptr, &m_SwapchainImageViews[i]);
    }
}
void VulkanRHI::CreateSyncObjects()
{
    VkSemaphoreCreateInfo semInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkFenceCreateInfo fenceInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    vkCreateSemaphore(m_Device, &semInfo, nullptr, &m_ImageAvailable);
    vkCreateSemaphore(m_Device, &semInfo, nullptr, &m_RenderFinished);
    vkCreateFence(m_Device, &fenceInfo, nullptr, &m_InFlightFence);
}
void VulkanRHI::CreateSurface(Window* window)
{
    VkResult result = glfwCreateWindowSurface(m_Instance, window->GetGLFWwindow(), nullptr, &m_Surface);
    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to create GLFW Vulkan surface");
}

uint32_t VulkanRHI::FindGraphicsQueueFamily()
{
    uint32_t queueCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueCount, queueFamilies.data());

    for (uint32_t i = 0; i < queueCount; i++)
    {
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(m_PhysicalDevice, i, m_Surface, &presentSupport);

        if ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && presentSupport)
            return i;
    }

    throw std::runtime_error("Failed to find a suitable queue family");
}
VkSurfaceFormatKHR VulkanRHI::ChooseSurfaceFormat()
{
    uint32_t count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface, &count, nullptr);

    std::vector<VkSurfaceFormatKHR> formats(count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface, &count, formats.data());

    for (auto& f : formats)
    {
        if (f.format == VK_FORMAT_B8G8R8A8_UNORM && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return f;
    }

    return formats[0];
}
VkPresentModeKHR VulkanRHI::ChoosePresentMode()
{
    uint32_t count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_PhysicalDevice, m_Surface, &count, nullptr);

    std::vector<VkPresentModeKHR> modes(count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_PhysicalDevice, m_Surface, &count, modes.data());

    for (auto& m : modes)
    {
        if (m == VK_PRESENT_MODE_MAILBOX_KHR)
            return m;
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}




