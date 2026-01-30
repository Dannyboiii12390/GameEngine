#pragma once
#include <vulkan/vulkan.h>
#include <string_view>
#include <functional>
#include <vector>
#include <tuple>

class Window;
class Mesh;
class Pipeline;
class Entity;
class Texture;
class Scene;
class Shader;

class VulkanRHI 
{
public:
	void Initialise(Window* window);
	void Shutdown();
	void WaitIdle();
	void BeginFrame();
	void BeginFrame(const Scene& scene);
	void EndFrame();
	void Present();

	Mesh CreateMesh(std::string_view obj_path);
	Mesh CreateMesh(std::function < std::tuple <std::vector<float>, std::vector<uint64_t>>()> generator);

	Pipeline CreatePipeline(const std::string_view vertex_shader_path, const std::string_view fragment_shader_path);
	Pipeline CreatePipeline(const Shader& vertex_shader, const Shader& fragment_shader);
	Pipeline CreatePipeline();


	Texture CreateTexture(const std::string_view texture_path);
	Texture CreateTexture(int width, int height, int channels, const void* data);
	Texture CreateTexture(VkImage image, int width, int height, VkFormat format);
	Texture CreateTexture(VkImageView imageView, int width, int height, VkFormat format);
	Texture CreateTexture(float r, float g, float b, float a);
	
	// --- Useful getters for other systems ---
	VkDevice GetDevice() const;
	VkPhysicalDevice GetPhysicalDevice() const;
	VkQueue GetGraphicsQueue() const;
	VkQueue GetPresentQueue() const;
	VkCommandPool GetCommandPool() const;
	VkExtent2D GetSwapchainExtent() const;
	VkFormat GetSwapchainFormat() const;

	void HandleWindowResize();
	void RecreateSwapchainAndResources();
	void ToggleVSync(bool enabled);
	void EnableValidationLayers(bool enabled);
	bool AreValidationLayersEnabled() const;

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
	void CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
	VkImageView CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
	void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
	void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
	VkSampler CreateSampler();

	uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
	void GenerateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);

	// Descriptors & pipelines
	void CreateDescriptorSetLayout();
	void CreateDescriptorPool();
	void AllocateDescriptorSets();
	void UpdateDescriptorSetsForFrame(uint32_t frameIndex);

	void CreatePipelineLayout();
	void DestroyPipeline(const Pipeline& pipeline);
	void RecreatePipelines();

	// Cleanup helpers
	void DestroyResourceMesh(const Mesh& mesh);
	void DestroyResourceTexture(const Texture& texture);
	void DestroyResourcePipeline(const Pipeline& pipeline);

	// Utility / queries
	bool IsDeviceSuitable(VkPhysicalDevice device) const;
	bool CheckDeviceExtensionSupport(VkPhysicalDevice device) const;
	VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const;
	bool HasStencilComponent(VkFormat format) const;
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

	// Render pass owned by RHI (optional)
	VkRenderPass m_RenderPass = VK_NULL_HANDLE;

	// Command & sync
	VkCommandPool m_CommandPool = VK_NULL_HANDLE;
	std::vector<VkCommandBuffer> m_CommandBuffers;
	std::vector<VkSemaphore> m_ImageAvailableSemaphores;
	std::vector<VkSemaphore> m_RenderFinishedSemaphores;
	std::vector<VkFence> m_InFlightFences;

	// Runtime flags / bookkeeping
	bool m_EnableValidationLayers = true;
	bool m_VSyncEnabled = false;
	size_t m_CurrentFrame = 0;
};