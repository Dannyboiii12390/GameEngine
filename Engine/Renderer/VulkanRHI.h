#pragma once
#include <vulkan/vulkan.h>
#include <string_view>
#include <functional>
#include <vector>
#include <tuple>
#include <cstdint>
#include <algorithm>

class Window;
class Mesh;
class Pipeline;
class Entity;
class Texture;
class Scene;
class Shader;
class Camera; // forward

class VulkanRHI 
{
public:
	void Initialise(Window* window);
	void Shutdown();
	void WaitIdle();
	void BeginFrame();
	void EndFrame();
	void Present();
	
	// --- Useful getters for other systems ---
	VkDevice GetDevice() const { return m_Device; }
	VkPhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }
	VkQueue GetGraphicsQueue() const { return m_GraphicsQueue; }
	VkQueue GetPresentQueue() const { return m_PresentQueue; }
	VkCommandPool GetCommandPool() const { return m_CommandPool; }
	VkExtent2D GetSwapchainExtent() const { return m_SwapchainExtent; }
	VkFormat GetSwapchainFormat() const { return m_SwapchainImageFormat; }
    // Expose the RHI-owned render pass so systems/pipelines can use it.
    VkRenderPass GetRenderPass() const { return m_RenderPass; }

	void HandleWindowResize();
	void RecreateSwapchainAndResources();
	void ToggleVSync(bool enabled);
	void EnableValidationLayers(bool enabled);
	bool AreValidationLayersEnabled() const;

	// Return the command buffer associated with the currently-acquired swapchain image
	// or VK_NULL_HANDLE if no image is currently acquired. Useful for recording
	// draw commands after calling BeginFrame().
	VkCommandBuffer GetCurrentCommandBuffer() const;

    // Camera integration
    // VulkanRHI does not take ownership. Caller must ensure camera lifetime > VulkanRHI usage.
    void SetActiveCamera(Camera* camera);
    Camera* GetActiveCamera() const;

    // Descriptor helpers (expose RHI-owned descriptor set layout + descriptor sets so pipelines/systems can bind them)
    VkDescriptorSetLayout GetDescriptorSetLayout() const;
    const std::vector<VkDescriptorSet>& GetDescriptorSets() const;

	VkSampler CreateSampler();

    // Register / unregister textures so RHI can keep descriptor sets valid
    void RegisterTexture(class Texture* texture);
    void UnregisterTexture(class Texture* texture);

	void CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
	void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
	void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
	VkImageView CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);

private:

	// Initialization helpers
	void CreateInstance();
	void SetupDebugMessenger();
	void CreateSurface(Window* window);
	void PickPhysicalDevice();
	void CreateLogicalDevice();
	void CreateCommandPool();
	void CreateSyncObjects();

	// Swapchain lifecycle
	void CreateSwapchain();
	void CreateImageViews();
	void CreateRenderPass(); // if VulkanRHI owns renderpass creation
	void CreateFramebuffers();
	void CleanupSwapchain();

	// Command buffers
	void AllocateCommandBuffers();
	void FreeCommandBuffers();
	void RecordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex);

	// Single-time command helpers
	VkCommandBuffer BeginSingleTimeCommands();
	void EndSingleTimeCommands(VkCommandBuffer commandBuffer);

	// Resource creation & helpers
	void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
	void CopyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size);

	uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
	void GenerateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);

	// Descriptors & pipelines
	void CreateDescriptorSetLayout();
	void CreateDescriptorPool();
	void AllocateDescriptorSets();
	void CreatePipelineLayout();

    // Camera UBO helpers
    void CreateCameraUniformBufferAndWriteDescriptors();
    void UpdateCameraBuffer(); // called each frame before submit

	// Utility / queries
	/*bool IsDeviceSuitable(VkPhysicalDevice device) const;
	bool CheckDeviceExtensionSupport(VkPhysicalDevice device) const;
	VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const;*/
	bool HasStencilComponent(VkFormat format) const;
	void CreateDepthResources();

	// Helpers to create/destroy the fallback texture. Call CreateDefaultTexture() after device and descriptor sets are ready.
	void CreateDefaultTexture();
	void DestroyDefaultTexture();

private:
	// Vulkan core objects
	VkInstance m_Instance = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT m_DebugMessenger = VK_NULL_HANDLE;
	VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
	VkDevice m_Device = VK_NULL_HANDLE;
	VkSurfaceKHR m_Surface = VK_NULL_HANDLE;

	// Queues
	VkQueue m_GraphicsQueue = VK_NULL_HANDLE;
	VkQueue m_PresentQueue = VK_NULL_HANDLE;

	// Swapchain & images
	VkSwapchainKHR m_Swapchain = VK_NULL_HANDLE;
	std::vector<VkImage> m_SwapchainImages;
	VkFormat m_SwapchainImageFormat = VK_FORMAT_UNDEFINED;
	VkExtent2D m_SwapchainExtent{};

	std::vector<VkImageView> m_SwapchainImageViews;
	std::vector<VkFramebuffer> m_SwapchainFramebuffers;

	// Render pass owned by RHI
	VkRenderPass m_RenderPass = VK_NULL_HANDLE;

	// Command & sync
	VkCommandPool m_CommandPool = VK_NULL_HANDLE;
	std::vector<VkCommandBuffer> m_CommandBuffers;
	std::vector<VkSemaphore> m_ImageAvailableSemaphores;
	std::vector<VkSemaphore> m_RenderFinishedSemaphores;
	std::vector<VkFence> m_InFlightFences;

	// Track which fence is using each swapchain image (prevents acquiring an image still in use)
	std::vector<VkFence> m_ImagesInFlight;

	// Runtime flags / bookkeeping
	bool m_EnableValidationLayers = true;
	bool m_VSyncEnabled = false;
	size_t m_CurrentFrame = 0;

	// Track currently acquired swapchain image index between Begin/End/Present
	uint32_t m_CurrentImageIndex = UINT32_MAX;

    // Active camera (not owned)
    Camera* m_ActiveCamera = nullptr;

    // Camera uniform buffer (single buffer used by descriptor sets). Size: view + proj (two mat4)
    VkBuffer m_CameraUniformBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_CameraUniformBufferMemory = VK_NULL_HANDLE;
    size_t m_CameraUniformBufferSize = sizeof(float) * 16 * 2; // two mat4s

    // Fallback 1x1 white texture owned by RHI — used to replace descriptors when textures are destroyed
    VkImage      m_DefaultImage = VK_NULL_HANDLE;
    VkDeviceMemory m_DefaultImageMemory = VK_NULL_HANDLE;
    VkImageView  m_DefaultImageView = VK_NULL_HANDLE;

	// Depth resources
	VkImage m_DepthImage = VK_NULL_HANDLE;
	VkDeviceMemory m_DepthImageMemory = VK_NULL_HANDLE;
	VkImageView m_DepthImageView = VK_NULL_HANDLE;


};