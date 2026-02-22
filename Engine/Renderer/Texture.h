#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <memory>
#include <iostream>

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
struct TextureGPUResources
{
    VkImage        Image = VK_NULL_HANDLE;
    VkDeviceMemory ImageMemory = VK_NULL_HANDLE;
    VkImageView    ImageView = VK_NULL_HANDLE;
    VkSampler      Sampler = VK_NULL_HANDLE;

    uint32_t Width = 0;
    uint32_t Height = 0;
    uint32_t MipLevels = 1;
    TextureType Type = TextureType::Unknown;

	VkDevice device = nullptr; // Pointer to VulkanRHI's device for cleanup

    // to ensure proper ordering with VulkanRHI lifetime
    ~TextureGPUResources()
    {
        // Warn if resources weren't cleaned up properly
        if (Image != VK_NULL_HANDLE || ImageView != VK_NULL_HANDLE ||
            Sampler != VK_NULL_HANDLE || ImageMemory != VK_NULL_HANDLE)
        {
            std::cerr << "WARNING: TextureGPUResources destroyed without explicit cleanup. "
                << "Call Texture::Destroy() before VulkanRHI shutdown." << std::endl;
        }
    }
};

class Texture : public std::enable_shared_from_this<Texture>
{
public:
    Texture();
	Texture(VulkanRHI* rhi, const std::string& path, TextureType type = TextureType::Unknown, bool srgb = false);
    ~Texture();

    Texture(const Texture&);
    Texture& operator=(const Texture&);

    // Movable
    Texture(Texture&&) noexcept;
    Texture& operator=(Texture&&) noexcept;

    // Destroy GPU resources (safe to call multiple times)
    void Destroy(VulkanRHI* rhi);

    VkImage GetImage() const { return m_Resources->Image; }
    VkImageView GetImageView() const { return m_Resources->ImageView; }
    VkSampler GetSampler() const { return m_Resources->Sampler; }
    uint32_t Width() const { return m_Resources->Width; }
    uint32_t Height() const { return m_Resources->Height; }

    // Write this texture into the RHI descriptor sets (binding 1). Safe to call after descriptors allocated.
    void WriteToDescriptorSets(VulkanRHI* rhi) const;

private:
    // helpers
    bool CreateImageAndUpload(VulkanRHI* rhi, const void* pixels, int texWidth, int texHeight, int channels, VkFormat format);
    uint32_t FindMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
    VkDeviceSize BytesPerPixel(int channels) const { return static_cast<VkDeviceSize>(channels); }

    // Load a texture from file (supports PNG/JPEG via stb_image). srgb = true for color maps.
    // Returns true on success. rhi must be initialized.
    bool LoadFromFile(VulkanRHI* rhi, const std::string& path, TextureType type = TextureType::Unknown, bool srgb = false);


private:
	std::shared_ptr<TextureGPUResources> m_Resources;
};