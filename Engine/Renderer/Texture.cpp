#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "Texture.h"
#include "VulkanRHI.h"

#include <stdexcept>
#include <vector>
#include <cstring>
#include <iostream>
#include <memory>

Texture::Texture()
    : m_Resources(std::make_shared<TextureGPUResources>())
{
}
Texture::Texture(VulkanRHI* rhi, const std::string& path, TextureType type, bool srgb)
    : m_Resources(std::make_shared<TextureGPUResources>())
{
    if (!LoadFromFile(rhi, path, type, srgb))
    {
        throw std::runtime_error("Failed to load texture from file: " + path);
    }
}
Texture::~Texture()
{
    if (m_Resources)
    {
        long count = m_Resources.use_count();
        std::cout << "Texture destroyed (ref_count=" << count << ")";

        if (count == 1)
        {
            std::cout << " - Freeing GPU resources";
        }
        else
        {
            std::cout << " - GPU resources still shared by " << (count - 1) << " other instance(s)";
        }

        std::cout << std::endl;
    }
}

// Copy - shallow copy (share GPU resources)
Texture::Texture(const Texture& other)
{
    m_Resources = other.m_Resources;
}

Texture& Texture::operator=(const Texture& other)
{
    if (this != &other)
    {
        m_Resources = other.m_Resources;
    }
    return *this;
}

// Move - take ownership of the shared_ptr and leave other in a valid empty state.
Texture::Texture(Texture&& other) noexcept
{
    m_Resources = std::move(other.m_Resources);
    // Ensure the moved-from object still has a valid (empty) resources struct because
    // other methods expect m_Resources to be non-null.
    other.m_Resources = std::make_shared<TextureGPUResources>();
}

Texture& Texture::operator=(Texture&& other) noexcept
{
    if (this != &other)
    {
        m_Resources = std::move(other.m_Resources);
        other.m_Resources = std::make_shared<TextureGPUResources>();
    }
    return *this;
}

bool Texture::LoadFromFile(VulkanRHI* rhi, const std::string& path, TextureType type, bool srgb)
{
    if (!rhi) return false;

    int texWidth = 0, texHeight = 0, texChannels = 0;
    stbi_uc* pixels = stbi_load(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_default);
    if (!pixels) {
        std::cerr << "Texture::LoadFromFile: failed to load image: " << path << std::endl;
        const char* reason = stbi_failure_reason();
        if (reason && reason[0] != '\0') {
            std::cerr << "stb_image reason: " << reason << std::endl;
        }
        else {
            std::cerr << "stb_image provided no failure reason." << std::endl;
        }
        return false;
    }

    // Choose format
    VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
    if (texChannels == 1) {
        format = VK_FORMAT_R8_UNORM;
    }
    else if (texChannels == 3) {
        // we will expand to RGBA
        format = srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
    }
    else if (texChannels == 4) {
        format = srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
    }

    // If image is RGB (3 channels) expand to RGBA for simplicity
    std::vector<unsigned char> rgbaPixels;
    const void* uploadPixels = pixels;
    if (texChannels == 3) {
        rgbaPixels.resize(static_cast<size_t>(texWidth) * texHeight * 4);
        for (int y = 0; y < texHeight; ++y) {
            for (int x = 0; x < texWidth; ++x) {
                size_t srcIdx = (y * texWidth + x) * 3;
                size_t dstIdx = (y * texWidth + x) * 4;
                rgbaPixels[dstIdx + 0] = pixels[srcIdx + 0];
                rgbaPixels[dstIdx + 1] = pixels[srcIdx + 1];
                rgbaPixels[dstIdx + 2] = pixels[srcIdx + 2];
                rgbaPixels[dstIdx + 3] = 255;
            }
        }
        uploadPixels = rgbaPixels.data();
        texChannels = 4;
    }
    else if (texChannels == 1) {
        // expand to RGBA (greyscale -> R=G=B=val, A=255)
        rgbaPixels.resize(static_cast<size_t>(texWidth) * texHeight * 4);
        for (int i = 0; i < texWidth * texHeight; ++i) {
            unsigned char v = pixels[i];
            rgbaPixels[i * 4 + 0] = v;
            rgbaPixels[i * 4 + 1] = v;
            rgbaPixels[i * 4 + 2] = v;
            rgbaPixels[i * 4 + 3] = 255;
        }
        uploadPixels = rgbaPixels.data();
        texChannels = 4;
        format = VK_FORMAT_R8G8B8A8_UNORM;
    }
    else if (texChannels == 4) {
        // no change
    }

    bool ok = CreateImageAndUpload(rhi, uploadPixels, texWidth, texHeight, texChannels, format);

    stbi_image_free(pixels);

    if (ok) {
        // Register the texture with RHI using a weak_ptr.
        // If the Texture instance is owned by a shared_ptr, weak_from_this() will be valid.
        // If not, the weak_ptr will be empty and RegisterTexture will ignore it.
        rhi->RegisterTexture(this->weak_from_this());
    }
    return ok;
}

bool Texture::CreateImageAndUpload(VulkanRHI* rhi, const void* pixels, int texWidth, int texHeight, int channels, VkFormat format)
{
    VkDevice device = rhi->GetDevice();
    VkPhysicalDevice physical = rhi->GetPhysicalDevice();
    VkQueue graphicsQueue = rhi->GetGraphicsQueue();
    VkCommandPool commandPool = rhi->GetCommandPool();

    if (device == VK_NULL_HANDLE || physical == VK_NULL_HANDLE || graphicsQueue == VK_NULL_HANDLE || commandPool == VK_NULL_HANDLE) {
        return false;
    }

    m_Resources->Width = static_cast<uint32_t>(texWidth);
    m_Resources->Height = static_cast<uint32_t>(texHeight);
    m_Resources->MipLevels = 1;

    VkDeviceSize imageSize = static_cast<VkDeviceSize>(m_Resources->Width) * m_Resources->Height * 4; // we always upload as RGBA8

    // Create staging buffer
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingBufferMemory = VK_NULL_HANDLE;

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = imageSize;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &stagingBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create staging buffer");
    }

    VkMemoryRequirements memReq{};
    vkGetBufferMemoryRequirements(device, stagingBuffer, &memReq);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = FindMemoryType(physical, memReq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &stagingBufferMemory) != VK_SUCCESS) {
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        throw std::runtime_error("Failed to allocate staging buffer memory");
    }

    vkBindBufferMemory(device, stagingBuffer, stagingBufferMemory, 0);

    // copy pixels into staging buffer
    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
    std::memcpy(data, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(device, stagingBufferMemory);

    // Create optimal tiling image
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = m_Resources->Width;
    imageInfo.extent.height = m_Resources->Height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = m_Resources->MipLevels;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device, &imageInfo, nullptr, &m_Resources->Image) != VK_SUCCESS) {
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingBufferMemory, nullptr);
        throw std::runtime_error("Failed to create image");
    }

    vkGetImageMemoryRequirements(device, m_Resources->Image, &memReq);
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = FindMemoryType(physical, memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_Resources->ImageMemory) != VK_SUCCESS) {
        vkDestroyImage(device, m_Resources->Image, nullptr);
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingBufferMemory, nullptr);
        throw std::runtime_error("Failed to allocate image memory");
    }

    vkBindImageMemory(device, m_Resources->Image, m_Resources->ImageMemory, 0);

    // Begin single time command buffer
    VkCommandBufferAllocateInfo cmdAllocInfo{};
    cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAllocInfo.commandPool = commandPool;
    cmdAllocInfo.commandBufferCount = 1;

    VkCommandBuffer cmdBuf;
    if (vkAllocateCommandBuffers(device, &cmdAllocInfo, &cmdBuf) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffer for image upload");
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(cmdBuf, &beginInfo);

    // Transition image to TRANSFER_DST_OPTIMAL
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_Resources->Image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = m_Resources->MipLevels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(
        cmdBuf,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    // Copy buffer to image
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0; // tightly packed
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = { m_Resources->Width, m_Resources->Height, 1 };

    vkCmdCopyBufferToImage(cmdBuf, stagingBuffer, m_Resources->Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    // Transition to SHADER_READ_ONLY_OPTIMAL
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(
        cmdBuf,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    if (vkEndCommandBuffer(cmdBuf) != VK_SUCCESS) {
        throw std::runtime_error("Failed to end command buffer");
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuf;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, commandPool, 1, &cmdBuf);

    // Cleanup staging buffer
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);

    // Create image view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_Resources->Image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = m_Resources->MipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device, &viewInfo, nullptr, &m_Resources->ImageView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create texture image view");
    }

    // Create sampler via RHI helper (respects device/physical)
    m_Resources->Sampler = rhi->CreateSampler();

    return true;
}

void Texture::Destroy(VulkanRHI* rhi)
{
    if (!rhi)
        return;

    // Ensure any submitted work that might reference this image has finished.
    // This prevents errors like "vkDestroyImageView(): can't be called on VkImageView ... that is currently in use".
    rhi->WaitIdle();

    // Let RHI remove its references (descriptor bookkeeping) to this texture before destroying Vulkan objects.
    // Keep compatibility: UnregisterTexture accepts raw pointer overload.
    rhi->UnregisterTexture(this);

    VkDevice device = rhi->GetDevice();

    // Destroy only if we are the last owner of the GPU resources.
    // If other Texture instances share the same GPU resources (shallow copy),
    // they will keep the shared_ptr alive and handle destruction or they will call Destroy themselves.
    if (m_Resources && m_Resources.use_count() == 1)
    {
        if (m_Resources->Sampler != VK_NULL_HANDLE) {
            vkDestroySampler(device, m_Resources->Sampler, nullptr);
            m_Resources->Sampler = VK_NULL_HANDLE;
        }
        if (m_Resources->ImageView != VK_NULL_HANDLE) {
            vkDestroyImageView(device, m_Resources->ImageView, nullptr);
            m_Resources->ImageView = VK_NULL_HANDLE;
        }
        if (m_Resources->Image != VK_NULL_HANDLE) {
            vkDestroyImage(device, m_Resources->Image, nullptr);
            m_Resources->Image = VK_NULL_HANDLE;
        }
        if (m_Resources->ImageMemory != VK_NULL_HANDLE) {
            vkFreeMemory(device, m_Resources->ImageMemory, nullptr);
            m_Resources->ImageMemory = VK_NULL_HANDLE;
        }
    }
}

uint32_t Texture::FindMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) const
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
        if ((typeFilter & (1u << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("Failed to find suitable memory type");
}

void Texture::WriteToDescriptorSets(VulkanRHI* rhi) const
{
    if (!rhi) return;
    VkDevice device = rhi->GetDevice();
    const auto& sets = rhi->GetDescriptorSets();
    if (sets.empty()) return;
    if (m_Resources->ImageView == VK_NULL_HANDLE || m_Resources->Sampler == VK_NULL_HANDLE) return;

    std::vector<VkWriteDescriptorSet> descriptorWrites;
    std::vector<VkDescriptorImageInfo> imageInfos;
    descriptorWrites.reserve(sets.size());
    imageInfos.reserve(sets.size());

    for (size_t i = 0; i < sets.size(); ++i)
    {
        VkDescriptorImageInfo imgInfo{};
        imgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imgInfo.imageView = m_Resources->ImageView;
        imgInfo.sampler = m_Resources->Sampler;
        imageInfos.push_back(imgInfo);

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = sets[i];
        descriptorWrite.dstBinding = 1; // binding 1 reserved for combined sampler in RHI layout
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pImageInfo = &imageInfos.back();
        descriptorWrites.push_back(descriptorWrite);
    }

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
}