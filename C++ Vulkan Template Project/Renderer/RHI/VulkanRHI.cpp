#include "VulkanRHI.h"

#include <GLFW/glfw3.h>
#include "../../Graphics/Window.h"

#include <stdexcept>
#include <iostream>
#include <vector>
#include <set>
#include <optional>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <cassert>

static const std::vector<const char*> kDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

// File-scope resource caches for created Vulkan objects so RHI methods
// can be minimal while still keeping objects alive for the app lifetime.
// (You can later move these into VulkanRHI class members if preferred.)
static std::vector<VkBuffer>               s_Buffers;
static std::vector<VkDeviceMemory>         s_BufferMemories;
static std::vector<VkImage>                s_Images;
static std::vector<VkDeviceMemory>         s_ImageMemories;
static std::vector<VkImageView>            s_ImageViews;
static std::vector<VkShaderModule>         s_ShaderModules;
static std::vector<VkPipelineLayout>       s_PipelineLayouts;
static std::vector<VkDescriptorSetLayout>  s_DescriptorSetLayouts;

// Helpers
static uint32_t FindMemoryTypeIndex(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProperties{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
        if ((typeFilter & (1u << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
            return i;
    }

    throw std::runtime_error("FindMemoryTypeIndex: suitable memory type not found");
}

static VkFormat MapTextureFormatToVk(int formatEnum)
{
    // The project uses a project-agnostic TextureFormat enum definition.
    // Try to map common formats. If unknown, return VK_FORMAT_UNDEFINED.
    // The integer values must match your project's TextureFormat enum values.
    // If your TextureFormat is an enum class, replace casting appropriately.
    switch (formatEnum)
    {
    case 0: // Unknown
        return VK_FORMAT_UNDEFINED;
    case 1: // R8_UNORM
        return VK_FORMAT_R8_UNORM;
    case 2: // RG8_UNORM
        return VK_FORMAT_R8G8_UNORM;
    case 3: // RGBA8_UNORM
        return VK_FORMAT_R8G8B8A8_UNORM;
    case 4: // BGRA8_UNORM
        return VK_FORMAT_B8G8R8A8_UNORM;
    case 5: // R8G8B8A8_SRGB
        return VK_FORMAT_R8G8B8A8_SRGB;
    case 6: // R16_FLOAT
        return VK_FORMAT_R16_SFLOAT;
    case 7: // RG16_FLOAT
        return VK_FORMAT_R16G16_SFLOAT;
    case 8: // RGBA16_FLOAT
        return VK_FORMAT_R16G16B16A16_SFLOAT;
    case 9: // RGBA32_FLOAT
        return VK_FORMAT_R32G32B32A32_SFLOAT;
    case 10: // Depth24Stencil8
        return VK_FORMAT_D24_UNORM_S8_UINT;
    case 11: // Depth32_FLOAT
        return VK_FORMAT_D32_SFLOAT;
    default:
        return VK_FORMAT_UNDEFINED;
    }
}

void VulkanRHI::Initialise(Window* window)
{
    if (!window || !window->GetGLFWwindow())
        throw std::runtime_error("VulkanRHI::Initialise - invalid window");

    CreateInstance();
    CreateSurface(window);
    PickPhysicalDevice();
    CreateDevice();
    CreateSwapchain();
    CreateCommandPool();
    AllocateCommandBuffers();
    CreateSyncObjects();
}

void VulkanRHI::Shutdown()
{
    vkDeviceWaitIdle(m_Device);

    if (m_ImageAvailable != VK_NULL_HANDLE) vkDestroySemaphore(m_Device, m_ImageAvailable, nullptr);
    if (m_RenderFinished != VK_NULL_HANDLE) vkDestroySemaphore(m_Device, m_RenderFinished, nullptr);
    if (m_InFlightFence != VK_NULL_HANDLE) vkDestroyFence(m_Device, m_InFlightFence, nullptr);

    for (auto view : m_SwapchainImageViews)
        if (view != VK_NULL_HANDLE)
            vkDestroyImageView(m_Device, view, nullptr);

    if (m_Swapchain != VK_NULL_HANDLE) vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);

    if (m_CommandPool != VK_NULL_HANDLE) vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);

    if (m_Device != VK_NULL_HANDLE) vkDestroyDevice(m_Device, nullptr);
    if (m_Surface != VK_NULL_HANDLE) vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
    if (m_Instance != VK_NULL_HANDLE) vkDestroyInstance(m_Instance, nullptr);

    // Reset members
    m_CommandBuffers.clear();
    m_SwapchainImages.clear();
    m_SwapchainImageViews.clear();
    m_PhysicalDevice = VK_NULL_HANDLE;
    m_Device = VK_NULL_HANDLE;
    m_Instance = VK_NULL_HANDLE;
    m_Surface = VK_NULL_HANDLE;
    m_Swapchain = VK_NULL_HANDLE;
    m_GraphicsQueue = VK_NULL_HANDLE;
    m_CommandPool = VK_NULL_HANDLE;
    m_ImageAvailable = VK_NULL_HANDLE;
    m_RenderFinished = VK_NULL_HANDLE;
    m_InFlightFence = VK_NULL_HANDLE;
}

void VulkanRHI::BeginFrame()
{
    // Wait for previous frame to finish
    vkWaitForFences(m_Device, 1, &m_InFlightFence, VK_TRUE, UINT64_MAX);
    vkResetFences(m_Device, 1, &m_InFlightFence);

    VkResult res = vkAcquireNextImageKHR(m_Device, m_Swapchain, UINT64_MAX, m_ImageAvailable, VK_NULL_HANDLE, &m_CurrentImageIndex);
    if (res == VK_ERROR_OUT_OF_DATE_KHR) {
        CreateSwapchain();
        return;
    }
    else if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Failed to acquire swapchain image");
    }

    // Begin command buffer
    VkCommandBuffer cmd = m_CommandBuffers[m_CurrentImageIndex];

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkResetCommandBuffer(cmd, 0);
    if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin command buffer");
    }

    // Minimal: here you'd normally record render pass / draw commands.
}

void VulkanRHI::EndFrame()
{
    VkCommandBuffer cmd = m_CommandBuffers[m_CurrentImageIndex];

    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
        throw std::runtime_error("Failed to record command buffer");
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = { m_ImageAvailable };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    VkSemaphore signalSemaphores[] = { m_RenderFinished };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, m_InFlightFence) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit draw command buffer");
    }
}

void VulkanRHI::Present()
{
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    VkSemaphore signalSemaphores[] = { m_RenderFinished };
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapchains[] = { m_Swapchain };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &m_CurrentImageIndex;

    VkResult res = vkQueuePresentKHR(m_GraphicsQueue, &presentInfo);

    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
        CreateSwapchain();
    } else if (res != VK_SUCCESS) {
        throw std::runtime_error("Failed to present swapchain image");
    }

    m_CurrentFrame++;
}

void VulkanRHI::WaitIdle()
{
    vkDeviceWaitIdle(m_Device);
}

void VulkanRHI::CreateDevice()
{
    // Query queue family
    m_GraphicsQueueFamily = FindGraphicsQueueFamily();
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = m_GraphicsQueueFamily;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    VkPhysicalDeviceFeatures deviceFeatures{};

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pQueueCreateInfos = &queueCreateInfo;
    createInfo.queueCreateInfoCount = 1;
    createInfo.pEnabledFeatures = &deviceFeatures;

    createInfo.enabledExtensionCount = static_cast<uint32_t>(kDeviceExtensions.size());
    createInfo.ppEnabledExtensionNames = kDeviceExtensions.data();

    if (vkCreateDevice(m_PhysicalDevice, &createInfo, nullptr, &m_Device) != VK_SUCCESS)
        throw std::runtime_error("Failed to create logical device");

    vkGetDeviceQueue(m_Device, m_GraphicsQueueFamily, 0, &m_GraphicsQueue);
}

void VulkanRHI::CreateSwapchain()
{
    // Query surface capabilities
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_PhysicalDevice, m_Surface, &capabilities);

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    if (formatCount > 0)
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface, &formatCount, formats.data());

    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_PhysicalDevice, m_Surface, &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    if (presentModeCount > 0)
        vkGetPhysicalDeviceSurfacePresentModesKHR(m_PhysicalDevice, m_Surface, &presentModeCount, presentModes.data());

    VkSurfaceFormatKHR surfaceFormat = (formats.empty() ? VkSurfaceFormatKHR{VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR} : ChooseSurfaceFormat());
    VkPresentModeKHR presentMode = ChoosePresentMode();
    VkExtent2D extent;

    if (capabilities.currentExtent.width != UINT32_MAX) {
        extent = capabilities.currentExtent;
    } else {
        // fallback size
        extent = {800, 600};
        extent.width = std::clamp(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        extent.height = std::clamp(extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    }

    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount)
        imageCount = capabilities.maxImageCount;

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = m_Surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t queueFamilyIndices[] = { m_GraphicsQueueFamily };
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.queueFamilyIndexCount = 0;
    createInfo.pQueueFamilyIndices = nullptr;

    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = m_Swapchain;

    if (vkCreateSwapchainKHR(m_Device, &createInfo, nullptr, &m_Swapchain) != VK_SUCCESS)
        throw std::runtime_error("Failed to create swap chain");

    vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &imageCount, nullptr);
    m_SwapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &imageCount, m_SwapchainImages.data());

    m_SwapchainFormat = surfaceFormat.format;
    m_SwapchainExtent = extent;

    CreateSwapchainImageViews();
}

void VulkanRHI::CreateCommandQueue()
{
    if (m_Device == VK_NULL_HANDLE)
        throw std::runtime_error("CreateCommandQueue: logical device not created");

    // Ensure we have a graphics queue handle (CreateDevice typically sets this)
    if (m_GraphicsQueue == VK_NULL_HANDLE) {
        m_GraphicsQueueFamily = FindGraphicsQueueFamily();
        vkGetDeviceQueue(m_Device, m_GraphicsQueueFamily, 0, &m_GraphicsQueue);
    }
}

void VulkanRHI::CreateCommandBuffer()
{
    if (m_Device == VK_NULL_HANDLE)
        throw std::runtime_error("CreateCommandBuffer: logical device not created");

    if (m_CommandPool == VK_NULL_HANDLE)
        CreateCommandPool();

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_CommandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd;
    if (vkAllocateCommandBuffers(m_Device, &allocInfo, &cmd) != VK_SUCCESS)
        throw std::runtime_error("CreateCommandBuffer: failed to allocate command buffer");

    m_CommandBuffers.push_back(cmd);
}

void VulkanRHI::CreateBuffer(const RHI::BufferDesc& desc)
{
    
    if (m_Device == VK_NULL_HANDLE || m_PhysicalDevice == VK_NULL_HANDLE)
        throw std::runtime_error("CreateBuffer: device or physical device not initialized");

    VkBufferUsageFlags usage = 0;
    VkMemoryPropertyFlags memProps = 0;

    switch (desc.Type) {
    case RHI::BufferType::Vertex:
        usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        break;
    case RHI::BufferType::Index:
        usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        break;
    case RHI::BufferType::Uniform:
        usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        break;
    case RHI::BufferType::Storage:
        usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        break;
    case RHI::BufferType::Indirect:
        usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        break;
    case RHI::BufferType::Staging:
        usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        break;
    default:
        usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        break;
    }

    if (desc.CpuMappable) {
        memProps = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    } else {
        memProps = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    }

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = desc.Size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buffer = VK_NULL_HANDLE;
    if (vkCreateBuffer(m_Device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
        throw std::runtime_error("CreateBuffer: failed to create buffer");

    VkMemoryRequirements memReq{};
    vkGetBufferMemoryRequirements(m_Device, buffer, &memReq);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = FindMemoryTypeIndex(m_PhysicalDevice, memReq.memoryTypeBits, memProps);

    VkDeviceMemory bufferMemory = VK_NULL_HANDLE;
    if (vkAllocateMemory(m_Device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        vkDestroyBuffer(m_Device, buffer, nullptr);
        throw std::runtime_error("CreateBuffer: failed to allocate buffer memory");
    }

    vkBindBufferMemory(m_Device, buffer, bufferMemory, 0);

    s_Buffers.push_back(buffer);
    s_BufferMemories.push_back(bufferMemory);

    // Optionally set debug name if the Vulkan debug utils are available (not implemented here).
}
void VulkanRHI::CreateTexture(const RHI::TextureDesc& desc)
{
    if (m_Device == VK_NULL_HANDLE || m_PhysicalDevice == VK_NULL_HANDLE)
        throw std::runtime_error("CreateTexture: device or physical device not initialized");

    VkFormat vkFormat = MapTextureFormatToVk(static_cast<int>(desc.Format));
    if (vkFormat == VK_FORMAT_UNDEFINED)
        throw std::runtime_error("CreateTexture: unsupported/undefined texture format");

    VkImageUsageFlags usage = 0;
    if (desc.Usage & RHI::TextureUsage_Sampled) usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if (desc.Usage & RHI::TextureUsage_TransferDst) usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (desc.Usage & RHI::TextureUsage_TransferSrc) usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if (desc.Usage & RHI::TextureUsage_ColorAttachment) usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (desc.Usage & RHI::TextureUsage_DepthStencil) usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if (desc.Usage & RHI::TextureUsage_Storage) usage |= VK_IMAGE_USAGE_STORAGE_BIT;

    if (usage == 0)
        usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT; // sensible default

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = vkFormat;
    imageInfo.extent.width = desc.Width;
    imageInfo.extent.height = desc.Height;
    imageInfo.extent.depth = desc.Depth;
    imageInfo.mipLevels = desc.MipLevels;
    imageInfo.arrayLayers = desc.ArrayLayers;
    imageInfo.samples = static_cast<VkSampleCountFlagBits>(desc.Samples);
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = usage;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (desc.IsCubemap) {
        imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        imageInfo.arrayLayers = 6;
    }

    VkImage image = VK_NULL_HANDLE;
    if (vkCreateImage(m_Device, &imageInfo, nullptr, &image) != VK_SUCCESS)
        throw std::runtime_error("CreateTexture: failed to create image");

    VkMemoryRequirements memReq{};
    vkGetImageMemoryRequirements(m_Device, image, &memReq);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = FindMemoryTypeIndex(m_PhysicalDevice, memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkDeviceMemory imageMemory = VK_NULL_HANDLE;
    if (vkAllocateMemory(m_Device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        vkDestroyImage(m_Device, image, nullptr);
        throw std::runtime_error("CreateTexture: failed to allocate image memory");
    }

    vkBindImageMemory(m_Device, image, imageMemory, 0);

    // Create an image view for shader sampling
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = desc.ArrayLayers > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
    if (desc.IsCubemap)
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    viewInfo.format = vkFormat;
    viewInfo.subresourceRange.aspectMask = (desc.Usage & RHI::TextureUsage_DepthStencil) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = desc.MipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = desc.ArrayLayers;

    VkImageView imageView = VK_NULL_HANDLE;
    if (vkCreateImageView(m_Device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        vkDestroyImage(m_Device, image, nullptr);
        vkFreeMemory(m_Device, imageMemory, nullptr);
        throw std::runtime_error("CreateTexture: failed to create image view");
    }

    s_Images.push_back(image);
    s_ImageMemories.push_back(imageMemory);
    s_ImageViews.push_back(imageView);
}
void VulkanRHI::CreateShader(const RHI::ShaderDesc& desc)
{
    if (m_Device == VK_NULL_HANDLE)
        throw std::runtime_error("CreateShader: logical device not created");

    // Prefer using Bytecode (SPIR-V) if present.
    std::vector<uint8_t> code;
    if (!desc.Bytecode.empty()) {
        code = desc.Bytecode;
    } else if (!desc.SourceFile.empty()) {
        // Try to load the file as binary (SPIR-V) - not a full GLSL -> SPV compiler here.
        std::ifstream ifs(desc.SourceFile, std::ios::binary | std::ios::ate);
        if (!ifs)
            throw std::runtime_error("CreateShader: failed to open shader file: " + desc.SourceFile);
        auto size = static_cast<size_t>(ifs.tellg());
        ifs.seekg(0);
        code.resize(size);
        ifs.read(reinterpret_cast<char*>(code.data()), size);
    } else {
        throw std::runtime_error("CreateShader: no bytecode or source file provided");
    }

    if (code.size() % 4 != 0)
        throw std::runtime_error("CreateShader: SPIR-V bytecode size must be multiple of 4");

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule = VK_NULL_HANDLE;
    if (vkCreateShaderModule(m_Device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
        throw std::runtime_error("CreateShader: failed to create shader module");

    s_ShaderModules.push_back(shaderModule);
}
void VulkanRHI::CreatePipelineState(const RHI::PipelineDesc& desc)
{
    if (m_Device == VK_NULL_HANDLE)
        throw std::runtime_error("CreatePipelineState: logical device not created");

    // Minimal implementation: create a pipeline layout (no descriptor sets / push constants by default).
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 0;
    layoutInfo.pSetLayouts = nullptr;
    layoutInfo.pushConstantRangeCount = 0;
    layoutInfo.pPushConstantRanges = nullptr;

    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    if (vkCreatePipelineLayout(m_Device, &layoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
        throw std::runtime_error("CreatePipelineState: failed to create pipeline layout");

    s_PipelineLayouts.push_back(pipelineLayout);

    // Note:
    // Creating a full VkPipeline requires:
    //  - shaders (VkPipelineShaderStageCreateInfo)
    //  - render pass / subpass (or dynamic rendering info)
    //  - vertex input state (mapped from desc.VertexLayoutDesc)
    //  - rasterization, multisample, depth-stencil, color blend states
    //  - pipeline cache, and many other params.
    // For a minimal RHI it's appropriate to create the pipeline when
    // renderpass and shader modules are known. This stub creates the pipeline layout
    // so higher-level code can create the full pipeline when render targets are available.
}
void VulkanRHI::CreateDescriptorSet(const RHI::DescriptorDesc& desc)
{
    if (m_Device == VK_NULL_HANDLE)
        throw std::runtime_error("CreateDescriptorSet: logical device not created");

    // Translate DescriptorDesc -> VkDescriptorSetLayout
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    bindings.reserve(desc.Bindings.size());
    for (const RHI::DescriptorBinding& b : desc.Bindings) {
        VkDescriptorSetLayoutBinding layoutBinding{};
        layoutBinding.binding = b.Binding;

        // Map our generic DescriptorType to VkDescriptorType
        switch (b.Type) {
        case RHI::DescriptorType::CombinedImageSampler:
            layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            break;
        case RHI::DescriptorType::SampledImage:
            layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            break;
        case RHI::DescriptorType::StorageImage:
            layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            break;
        case RHI::DescriptorType::UniformBuffer:
            layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            break;
        case RHI::DescriptorType::StorageBuffer:
            layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            break;
        case RHI::DescriptorType::Sampler:
            layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
            break;
        default:
            layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            break;
        }

        layoutBinding.descriptorCount = b.Count;
        layoutBinding.stageFlags = static_cast<VkShaderStageFlags>(b.StageFlags);
        layoutBinding.pImmutableSamplers = nullptr;
        bindings.push_back(layoutBinding);
    }

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.empty() ? nullptr : bindings.data();

    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    if (vkCreateDescriptorSetLayout(m_Device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS)
        throw std::runtime_error("CreateDescriptorSet: failed to create descriptor set layout");

    s_DescriptorSetLayouts.push_back(descriptorSetLayout);

    // Note:
    // Creating descriptor sets themselves requires a descriptor pool and knowledge
    // of how many sets/descriptor types you will allocate. This function creates the
    // layout only; allocation of sets from a pool can be implemented later when the
    // pool is available / descriptor counts are known.
}

void VulkanRHI::CreateInstance()
{
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "VulkanRHI";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "MinimalEngine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    createInfo.enabledExtensionCount = glfwExtensionCount;
    createInfo.ppEnabledExtensionNames = glfwExtensions;

    if (vkCreateInstance(&createInfo, nullptr, &m_Instance) != VK_SUCCESS)
        throw std::runtime_error("Failed to create Vulkan instance");
}

void VulkanRHI::CreateSurface(Window* window)
{
    if (!window || !window->GetGLFWwindow())
        throw std::runtime_error("CreateSurface - invalid window");

    if (glfwCreateWindowSurface(m_Instance, window->GetGLFWwindow(), nullptr, &m_Surface) != VK_SUCCESS)
        throw std::runtime_error("Failed to create window surface");
}

void VulkanRHI::PickPhysicalDevice()
{
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);
    if (deviceCount == 0)
        throw std::runtime_error("Failed to find GPUs with Vulkan support");

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices.data());

    for (const auto& dev : devices) {
        // check required extensions
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(dev, nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(dev, nullptr, &extensionCount, availableExtensions.data());

        std::set<std::string> required(kDeviceExtensions.begin(), kDeviceExtensions.end());
        for (const auto& ext : availableExtensions) {
            required.erase(ext.extensionName);
        }
        if (!required.empty())
            continue; // missing swapchain support

        // check queue families and surface support
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &queueFamilyCount, queueFamilies.data());

        bool suitable = false;
        for (uint32_t i = 0; i < queueFamilyCount; ++i) {
            if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                VkBool32 presentSupport = VK_FALSE;
                vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, m_Surface, &presentSupport);
                if (presentSupport) { suitable = true; break; }
            }
        }
        if (suitable) {
            m_PhysicalDevice = dev;
            break;
        }
    }

    if (m_PhysicalDevice == VK_NULL_HANDLE)
        throw std::runtime_error("Failed to find a suitable GPU");
}

void VulkanRHI::CreateCommandPool()
{
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = m_GraphicsQueueFamily = FindGraphicsQueueFamily();
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(m_Device, &poolInfo, nullptr, &m_CommandPool) != VK_SUCCESS)
        throw std::runtime_error("Failed to create command pool");
}

void VulkanRHI::AllocateCommandBuffers()
{
    m_CommandBuffers.resize(m_SwapchainImages.size());

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_CommandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(m_CommandBuffers.size());

    if (vkAllocateCommandBuffers(m_Device, &allocInfo, m_CommandBuffers.data()) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate command buffers");
}

void VulkanRHI::CreateSyncObjects()
{
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    if (vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &m_ImageAvailable) != VK_SUCCESS ||
        vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &m_RenderFinished) != VK_SUCCESS ||
        vkCreateFence(m_Device, &fenceInfo, nullptr, &m_InFlightFence) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create synchronization objects for a frame");
    }
}

void VulkanRHI::CreateSwapchainImageViews()
{
    m_SwapchainImageViews.resize(m_SwapchainImages.size());
    for (size_t i = 0; i < m_SwapchainImages.size(); ++i) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_SwapchainImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = m_SwapchainFormat;
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(m_Device, &viewInfo, nullptr, &m_SwapchainImageViews[i]) != VK_SUCCESS)
            throw std::runtime_error("Failed to create image views");
    }
}

uint32_t VulkanRHI::FindGraphicsQueueFamily()
{
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, queueFamilies.data());

    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            VkBool32 presentSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(m_PhysicalDevice, i, m_Surface, &presentSupport);
            if (presentSupport) return i;
        }
    }

    throw std::runtime_error("Failed to find graphics queue family");
}

VkSurfaceFormatKHR VulkanRHI::ChooseSurfaceFormat()
{
    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface, &formatCount, nullptr);
    if (formatCount == 0) return { VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };

    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface, &formatCount, formats.data());

    for (const auto& availableFormat : formats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
            availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }

    return formats[0];
}

VkPresentModeKHR VulkanRHI::ChoosePresentMode()
{
    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_PhysicalDevice, m_Surface, &presentModeCount, nullptr);
    if (presentModeCount == 0) return VK_PRESENT_MODE_FIFO_KHR;

    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_PhysicalDevice, m_Surface, &presentModeCount, presentModes.data());

    // Prefer MAILBOX, fallback to FIFO
    for (const auto& mode : presentModes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) return mode;
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}




