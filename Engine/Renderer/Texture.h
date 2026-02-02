#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <memory>

class VulkanRHI;

enum class TextureType
{
    Unknown,
    Albedo,
    Normal,
    MetallicRoughness,
    AO,
    ORM // occlusion-roughness-metallic packed
};

class Texture
{
public:
    Texture() = default;
    ~Texture();

    // Non-copyable
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    // Movable
    Texture(Texture&&) noexcept;
    Texture& operator=(Texture&&) noexcept;

    // Load a texture from file (supports PNG/JPEG via stb_image). srgb = true for color maps.
    // Returns true on success. rhi must be initialized.
    bool LoadFromFile(VulkanRHI* rhi, const std::string& path, TextureType type = TextureType::Unknown, bool srgb = false);

    // Destroy GPU resources (safe to call multiple times)
    void Destroy(VulkanRHI* rhi);

    VkImage GetImage() const { return m_Image; }
    VkImageView GetImageView() const { return m_ImageView; }
    VkSampler GetSampler() const { return m_Sampler; }
    uint32_t Width() const { return m_Width; }
    uint32_t Height() const { return m_Height; }

    // Write this texture into the RHI descriptor sets (binding 1). Safe to call after descriptors allocated.
    void WriteToDescriptorSets(VulkanRHI* rhi) const;

private:
    // helpers
    bool CreateImageAndUpload(VulkanRHI* rhi, const void* pixels, int texWidth, int texHeight, int channels, VkFormat format);
    uint32_t FindMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
    VkDeviceSize BytesPerPixel(int channels) const { return static_cast<VkDeviceSize>(channels); }

private:
    VkImage m_Image = VK_NULL_HANDLE;
    VkDeviceMemory m_ImageMemory = VK_NULL_HANDLE;
    VkImageView m_ImageView = VK_NULL_HANDLE;
    VkSampler m_Sampler = VK_NULL_HANDLE;

    uint32_t m_Width = 0;
    uint32_t m_Height = 0;
    uint32_t m_MipLevels = 1;
    TextureType m_Type = TextureType::Unknown;
};