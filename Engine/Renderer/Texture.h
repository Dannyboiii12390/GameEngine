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

	VkDevice* device = nullptr; // Pointer to VulkanRHI's device for cleanup

    ~TextureGPUResources()
    {
        if (ImageView != VK_NULL_HANDLE)
        {
            vkDestroyImageView(*device, ImageView, nullptr);
            ImageView = VK_NULL_HANDLE;
        }

        if (Sampler != VK_NULL_HANDLE)
        {
            vkDestroySampler(*device, Sampler, nullptr);
            Sampler = VK_NULL_HANDLE;
        }

        if (Image != VK_NULL_HANDLE)
        {
            vkDestroyImage(*device, Image, nullptr);
            Image = VK_NULL_HANDLE;
        }

        if (ImageMemory != VK_NULL_HANDLE)
        {
            vkFreeMemory(*device, ImageMemory, nullptr);
            ImageMemory = VK_NULL_HANDLE;
        }
    }
};

class Texture
{
public:
    Texture();
    ~Texture();

    // Non-copyable
    Texture(const Texture&);
    Texture& operator=(const Texture&);

    // Movable
    Texture(Texture&&) noexcept;
    Texture& operator=(Texture&&) noexcept;

    // Load a texture from file (supports PNG/JPEG via stb_image). srgb = true for color maps.
    // Returns true on success. rhi must be initialized.
    bool LoadFromFile(VulkanRHI* rhi, const std::string& path, TextureType type = TextureType::Unknown, bool srgb = false);

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

private:
	std::shared_ptr<TextureGPUResources> m_Resources;
};