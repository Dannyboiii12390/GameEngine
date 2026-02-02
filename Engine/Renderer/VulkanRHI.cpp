#include "VulkanRHI.h"
#include "Window.h"
#include "Camera.h"

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <vector>
#include <set>
#include <string>
#include <stdexcept>
#include <cstring>
#include <algorithm>
#include <iostream>

#include <glm/gtc/type_ptr.hpp>

namespace {
	const std::vector<const char*> validationLayers = {
		"VK_LAYER_KHRONOS_validation"
	};

	const std::vector<const char*> deviceExtensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

	VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* /*pUserData*/)
	{
		std::cerr << "Vulkan validation: " << (pCallbackData->pMessage ? pCallbackData->pMessage : "") << std::endl;
		return VK_FALSE;
	}

	VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
		const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
		auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
		if (func != nullptr) {
			return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
		}
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}

	void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
		auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
		if (func != nullptr) {
			func(instance, debugMessenger, pAllocator);
		}
	}
}
// Descriptor and pipeline related static variables
namespace {
	constexpr uint32_t s_MaxDescriptorFrames =2;
	VkDescriptorSetLayout s_DescriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorPool s_DescriptorPool = VK_NULL_HANDLE;
	std::vector<VkDescriptorSet> s_DescriptorSets;
	VkPipelineLayout s_PipelineLayout = VK_NULL_HANDLE;
}

// Add getter implementation
VkCommandBuffer VulkanRHI::GetCurrentCommandBuffer() const
{
	if (m_CurrentImageIndex == UINT32_MAX)
		return VK_NULL_HANDLE;
	if (m_CurrentImageIndex >= m_CommandBuffers.size())
		return VK_NULL_HANDLE;
	return m_CommandBuffers[m_CurrentImageIndex];
}

void VulkanRHI::Initialise(Window* window)
{
	if (!window) throw std::invalid_argument("Initialise called with null window");

	// Create Vulkan instance and optional debug messenger
	CreateInstance();
	SetupDebugMessenger();

	// Create window surface (GLFW)
	CreateSurface(window);

	// Select GPU and create logical device + queues
	PickPhysicalDevice();
	CreateLogicalDevice();

	// Create swapchain and dependent resources
	CreateSwapchain();
	CreateImageViews();
	CreateRenderPass();
	CreateFramebuffers();

	// Command pool + command buffers
	CreateCommandPool();
	AllocateCommandBuffers();

	// Synchronization objects
	CreateSyncObjects();

	// Descriptor/pipeline related objects (optional but useful defaults)
	CreateDescriptorSetLayout();
	CreateDescriptorPool();
	AllocateDescriptorSets();
	CreatePipelineLayout();

	// Ensure CurrentFrame index valid
	if (!m_InFlightFences.empty())
		m_CurrentFrame =0;
	m_CurrentImageIndex = UINT32_MAX;
}
void VulkanRHI::Shutdown()
{
	// Wait for device idle before tearing down
	WaitIdle();

	// Destroy descriptor / pipeline static objects
	if (s_PipelineLayout != VK_NULL_HANDLE) {
		vkDestroyPipelineLayout(m_Device, s_PipelineLayout, nullptr);
		s_PipelineLayout = VK_NULL_HANDLE;
	}
	if (s_DescriptorPool != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(m_Device, s_DescriptorPool, nullptr);
		s_DescriptorPool = VK_NULL_HANDLE;
	}
	if (s_DescriptorSetLayout != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(m_Device, s_DescriptorSetLayout, nullptr);
		s_DescriptorSetLayout = VK_NULL_HANDLE;
	}
	s_DescriptorSets.clear();

	// Destroy camera UBO
	if (m_CameraUniformBuffer != VK_NULL_HANDLE) {
		vkDestroyBuffer(m_Device, m_CameraUniformBuffer, nullptr);
		m_CameraUniformBuffer = VK_NULL_HANDLE;
	}
	if (m_CameraUniformBufferMemory != VK_NULL_HANDLE) {
		vkFreeMemory(m_Device, m_CameraUniformBufferMemory, nullptr);
		m_CameraUniformBufferMemory = VK_NULL_HANDLE;
	}

	// Cleanup swapchain related resources
	CleanupSwapchain();

	// Destroy semaphores and fences
	for (auto sem : m_ImageAvailableSemaphores) {
		if (sem != VK_NULL_HANDLE) vkDestroySemaphore(m_Device, sem, nullptr);
	}
	m_ImageAvailableSemaphores.clear();

	for (auto sem : m_RenderFinishedSemaphores) {
		if (sem != VK_NULL_HANDLE) vkDestroySemaphore(m_Device, sem, nullptr);
	}
	m_RenderFinishedSemaphores.clear();

	for (auto f : m_InFlightFences) {
		if (f != VK_NULL_HANDLE) vkDestroyFence(m_Device, f, nullptr);
	}
	m_InFlightFences.clear();

	// Free command buffers and destroy command pool
	FreeCommandBuffers();
	if (m_CommandPool != VK_NULL_HANDLE) {
		vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);
		m_CommandPool = VK_NULL_HANDLE;
	}
	// Destroy device
	if (m_Device != VK_NULL_HANDLE) {
		vkDestroyDevice(m_Device, nullptr);
		m_Device = VK_NULL_HANDLE;
	}

	// Destroy debug messenger and instance
	if (m_Instance != VK_NULL_HANDLE) {
		if (m_DebugMessenger != VK_NULL_HANDLE) {
			DestroyDebugUtilsMessengerEXT(m_Instance, m_DebugMessenger, nullptr);
			m_DebugMessenger = VK_NULL_HANDLE;
		}
		if (m_Surface != VK_NULL_HANDLE) {
			vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
			m_Surface = VK_NULL_HANDLE;
		}
		vkDestroyInstance(m_Instance, nullptr);
		m_Instance = VK_NULL_HANDLE;
	}
	// Clear bookkeeping
	m_SwapchainImages.clear();
	m_SwapchainImageViews.clear();
	m_SwapchainFramebuffers.clear();
	m_CurrentImageIndex = UINT32_MAX;
	m_CurrentFrame = 0;
}
void VulkanRHI::WaitIdle()
{
	if (m_Device != VK_NULL_HANDLE) {
		vkDeviceWaitIdle(m_Device);
	}
}
void VulkanRHI::BeginFrame()
{
	if (m_Device == VK_NULL_HANDLE || m_Swapchain == VK_NULL_HANDLE) {
		// Nothing to do yet
		return;
	}

	// Wait for previous frame (this frame index) to finish
	if (m_InFlightFences.empty()) {
		throw std::runtime_error("BeginFrame: In-flight fences not created");
	}

	VkFence inFlightFence = m_InFlightFences[m_CurrentFrame];
	if (vkWaitForFences(m_Device, 1, &inFlightFence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
		throw std::runtime_error("Failed to wait for in-flight fence");
	}
	// Acquire next image. Use per-frame imageAvailable semaphore.
	uint32_t imageIndex = UINT32_MAX;
	// Use a finite timeout to avoid potential deadlock when forward progress cannot be guaranteed.
	// With per-swapchain-image sync objects above, infinite timeout would normally be safe,
	// but a finite timeout makes the behaviour more robust if something goes wrong.
	const uint64_t acquireTimeout = 1000000000ULL; // 1 second
	VkResult acquireResult = vkAcquireNextImageKHR(m_Device, m_Swapchain, acquireTimeout, m_ImageAvailableSemaphores[m_CurrentFrame], VK_NULL_HANDLE, &imageIndex);
	if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
		// Swapchain incompatible with surface, recreate
		RecreateSwapchainAndResources();
		m_CurrentImageIndex = UINT32_MAX;
		return;
	}
	else if (acquireResult == VK_TIMEOUT) {
		// Timed out waiting for an image; bail out of this frame. Caller can try again next loop.
		m_CurrentImageIndex = UINT32_MAX;
		return;
	}
	else if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
		throw std::runtime_error("Failed to acquire swapchain image");
	}

	// If the image we just acquired is already being rendered to by another frame,
	// wait on the fence that is using it to ensure it's free. This prevents acquiring
	// more images than frames-in-flight (fixes the validation messages).
	if (m_ImagesInFlight.size() > imageIndex && m_ImagesInFlight[imageIndex] != VK_NULL_HANDLE) {
		if (vkWaitForFences(m_Device, 1, &m_ImagesInFlight[imageIndex], VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
			throw std::runtime_error("Failed to wait for image-in-flight fence");
		}
	}

	// Record that this image will now be in use by this frame's fence
	if (imageIndex < m_ImagesInFlight.size()) {
		m_ImagesInFlight[imageIndex] = inFlightFence;
	}
	// Reset fence for this frame (we'll signal it after submit)
	// It's safe to reset now because we've waited on it at start of frame.
	if (vkResetFences(m_Device, 1, &inFlightFence) != VK_SUCCESS) {
		throw std::runtime_error("Failed to reset in-flight fence");
	}

	// Reset and record the command buffer for the acquired image
	if (imageIndex >= m_CommandBuffers.size()) {
		throw std::runtime_error("Acquire returned image index out of range");
	}

	VkCommandBuffer cmd = m_CommandBuffers[imageIndex];
	// Reset the command buffer to be able to re-record
	if (vkResetCommandBuffer(cmd, 0) != VK_SUCCESS) {
		throw std::runtime_error("Failed to reset command buffer");
	}

	RecordCommandBuffer(cmd, imageIndex);
	
	// Store current image index for EndFrame/Present
	m_CurrentImageIndex = imageIndex;
}
void VulkanRHI::BeginFrame(const Scene& scene)
{
	// todo implement full scene drawFor now, just call the basic BeginFrame
	BeginFrame();
	// Future: could pass scene data to RecordCommandBuffer or other systems here
}
void VulkanRHI::EndFrame()
{
	if (m_Device == VK_NULL_HANDLE || m_Swapchain == VK_NULL_HANDLE) {
		return;
	}
	if (m_CurrentImageIndex == UINT32_MAX) {
		// Nothing to submit (e.g. during swapchain recreation)
		return;
	}

	// Before submitting, ensure the command buffer has its render pass ended and recording finished.
	VkCommandBuffer cmdBuf = VK_NULL_HANDLE;
	if (m_CurrentImageIndex < m_CommandBuffers.size()) {
		cmdBuf = m_CommandBuffers[m_CurrentImageIndex];
	}
	if (cmdBuf == VK_NULL_HANDLE) {
		throw std::runtime_error("EndFrame: current command buffer is VK_NULL_HANDLE");
	}

	// End render pass and end command buffer if still open.
	// It's safe to call vkCmdEndRenderPass even if no draw calls were recorded.
	vkCmdEndRenderPass(cmdBuf);

	if (vkEndCommandBuffer(cmdBuf) != VK_SUCCESS) {
		throw std::runtime_error("Failed to record command buffer");
	}

	// Update camera uniform buffer with latest camera matrices before submission
	UpdateCameraBuffer();

	VkSemaphore waitSem = m_ImageAvailableSemaphores[m_CurrentFrame];
	// Use the semaphore specific to the currently acquired image to avoid reuse while presentation may still hold it
	VkSemaphore signalSem = m_RenderFinishedSemaphores[m_CurrentImageIndex];
	VkFence inFlightFence = m_InFlightFences[m_CurrentFrame];

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	// Wait on image available
	submitInfo.waitSemaphoreCount =1;
	submitInfo.pWaitSemaphores = &waitSem;
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submitInfo.pWaitDstStageMask = waitStages;

	// Command buffer to submit
	submitInfo.commandBufferCount =1;
	submitInfo.pCommandBuffers = &cmdBuf;

	// Signal when render finished (per-image)
	submitInfo.signalSemaphoreCount =1;
	submitInfo.pSignalSemaphores = &signalSem;

	if (vkQueueSubmit(m_GraphicsQueue,1, &submitInfo, inFlightFence) != VK_SUCCESS) {
		throw std::runtime_error("Failed to submit draw command buffer");
	}
}


void VulkanRHI::HandleWindowResize()
{
	// Wait for device idle then recreate swapchain resources.
	if (m_Device == VK_NULL_HANDLE) return;
	vkDeviceWaitIdle(m_Device);
	RecreateSwapchainAndResources();
}
void VulkanRHI::RecreateSwapchainAndResources()
{
	if (m_Device == VK_NULL_HANDLE) {
		throw std::runtime_error("RecreateSwapchainAndResources called but device is not initialized");
	}

	// Ensure the device is idle before modifying swapchain-owned resources
	vkDeviceWaitIdle(m_Device);

	// Cleanup existing swapchain resources
	CleanupSwapchain();

	// Recreate swapchain and dependent resources
	CreateSwapchain();
	CreateImageViews();
	CreateRenderPass();
	CreateFramebuffers();

	// Reallocate and record command buffers for new framebuffers
	AllocateCommandBuffers();

	// Ensure images-in-flight mapping matches the new swapchain image count
	m_ImagesInFlight.assign(m_SwapchainImages.size(), VK_NULL_HANDLE);

	// Recreate per-image render-finished semaphores to match new image count
	for (auto sem : m_RenderFinishedSemaphores) {
		if (sem != VK_NULL_HANDLE) vkDestroySemaphore(m_Device, sem, nullptr);
	}
	m_RenderFinishedSemaphores.clear();
	m_RenderFinishedSemaphores.resize(std::max<size_t>(1, m_SwapchainImages.size()));
	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	for (size_t i = 0; i < m_RenderFinishedSemaphores.size(); ++i) {
		if (vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &m_RenderFinishedSemaphores[i]) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create render-finished semaphore during swapchain recreation");
		}
	}
	// Recreate descriptor / pipeline related objects if applicable.
	// These calls are safe no-ops if implementations decide not to recreate.
	CreateDescriptorSetLayout();
	CreateDescriptorPool();
	AllocateDescriptorSets();
	CreatePipelineLayout();

	// Update camera aspect ratio after swapchain recreation
	if (m_ActiveCamera != nullptr && m_SwapchainExtent.height != 0) {
		float aspect = static_cast<float>(m_SwapchainExtent.width) / static_cast<float>(m_SwapchainExtent.height);
		m_ActiveCamera->SetAspect(aspect);
	}
}
void VulkanRHI::ToggleVSync(bool enabled)
{
	// Toggle vsync and recreate swapchain to apply new present mode if device is ready.
	if (m_VSyncEnabled == enabled) return;
	m_VSyncEnabled = enabled;

	if (m_Device != VK_NULL_HANDLE) {
		RecreateSwapchainAndResources();
	}
}
void VulkanRHI::EnableValidationLayers(bool enabled)
{
	// Validation layers must be configured before instance creation.
	if (m_Instance != VK_NULL_HANDLE) {
		throw std::runtime_error("Cannot change validation layers after Vulkan instance has been created");
	}
	m_EnableValidationLayers = enabled;
}
bool VulkanRHI::AreValidationLayersEnabled() const
{
	return m_EnableValidationLayers;
}
void VulkanRHI::Present()
{
	if (m_Device == VK_NULL_HANDLE || m_Swapchain == VK_NULL_HANDLE) {
		return;
	}
	if (m_CurrentImageIndex == UINT32_MAX) {
		// Nothing to present
		return;
	}

	// Wait on the render-finished semaphore associated with the image we just rendered.
	VkSemaphore waitSem = m_RenderFinishedSemaphores[m_CurrentImageIndex];

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &waitSem;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &m_Swapchain;
	presentInfo.pImageIndices = &m_CurrentImageIndex;
	presentInfo.pResults = nullptr;
	VkResult presentResult = vkQueuePresentKHR(m_PresentQueue, &presentInfo);
	if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
		// Present did not complete successfully and therefore the render-finished semaphore
		// MAY not have been consumed by the presentation operation. Recreate the swapchain
		// and do NOT advance the frame index — keep the current semaphores/fences intact so
		// they are not reused while still signaled.
		RecreateSwapchainAndResources();
		m_CurrentImageIndex = UINT32_MAX;
		return;
	}
	else if (presentResult != VK_SUCCESS) {
		throw std::runtime_error("Failed to present swapchain image");
	}

	// Advance frame index only after a successful present (so semaphores were consumed).
	m_CurrentFrame = (m_CurrentFrame + 1) % std::max<size_t>(1, m_InFlightFences.size());
	m_CurrentImageIndex = UINT32_MAX;
}

// --- Expose descriptor layout & sets to other translation units via member functions ---
VkDescriptorSetLayout VulkanRHI::GetDescriptorSetLayout() const
{
	return s_DescriptorSetLayout;
}
const std::vector<VkDescriptorSet>& VulkanRHI::GetDescriptorSets() const
{
	return s_DescriptorSets;
}

// private methods
void VulkanRHI::CreateInstance() {
	if (m_EnableValidationLayers) {
		// Optionally verify validation layer availability here
		uint32_t layerCount = 0;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		for (const char* layerName : validationLayers) {
			bool found = false;
			for (const auto& prop : availableLayers) {
				if (std::strcmp(prop.layerName, layerName) == 0) {
					found = true;
					break;
				}
			}
			if (!found) {
				throw std::runtime_error("Validation layer requested, but not available.");
			}
		}
	}

	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Vulkan App";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "CustomEngine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_2;

	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
	std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

	if (m_EnableValidationLayers) {
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	VkInstanceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;
	createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	createInfo.ppEnabledExtensionNames = extensions.data();

	if (m_EnableValidationLayers) {
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();

		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
		debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		debugCreateInfo.messageSeverity =
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		debugCreateInfo.messageType =
			VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		debugCreateInfo.pfnUserCallback = (PFN_vkDebugUtilsMessengerCallbackEXT)DebugCallback;
		createInfo.pNext = &debugCreateInfo; // so messenger can be created during instance creation if desired
	} else {
		createInfo.enabledLayerCount = 0;
		createInfo.ppEnabledLayerNames = nullptr;
		createInfo.pNext = nullptr;
	}

	if (vkCreateInstance(&createInfo, nullptr, &m_Instance) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create Vulkan instance");
	}
}
void VulkanRHI::SetupDebugMessenger() {
	if (!m_EnableValidationLayers) return;

	VkDebugUtilsMessengerCreateInfoEXT createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	createInfo.messageSeverity =
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	createInfo.messageType =
		VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	createInfo.pfnUserCallback = (PFN_vkDebugUtilsMessengerCallbackEXT)DebugCallback;
	createInfo.pUserData = nullptr;

	if (CreateDebugUtilsMessengerEXT(m_Instance, &createInfo, nullptr, &m_DebugMessenger) != VK_SUCCESS) {
		throw std::runtime_error("Failed to set up debug messenger!");
	}
}
void VulkanRHI::CreateSurface(Window* window) {
	if (!window) throw std::invalid_argument("window is null in CreateSurface");

	GLFWwindow* glfwWin = window->GetGLFWwindow();
	if (!glfwWin) throw std::runtime_error("Window has no GLFWwindow");

	if (glfwCreateWindowSurface(m_Instance, glfwWin, nullptr, &m_Surface) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create window surface!");
	}
}
void VulkanRHI::PickPhysicalDevice() {
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);
	if (deviceCount == 0) {
		throw std::runtime_error("Failed to find GPUs with Vulkan support!");
	}

	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices.data());

	auto checkDeviceExtensionSupport = [this](VkPhysicalDevice device) -> bool {
		uint32_t extensionCount;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

		std::set<std::string> required(deviceExtensions.begin(), deviceExtensions.end());
		for (const auto& ext : availableExtensions) {
			required.erase(ext.extensionName);
		}
		return required.empty();
	};

	auto findQueueFamilies = [this](VkPhysicalDevice device) -> std::pair<int, int> {
		int graphicsFamily = -1;
		int presentFamily = -1;

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

		for (uint32_t i = 0; i < queueFamilyCount; ++i) {
			if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				graphicsFamily = static_cast<int>(i);
			}
			VkBool32 presentSupport = VK_FALSE;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_Surface, &presentSupport);
			if (presentSupport) {
				presentFamily = static_cast<int>(i);
			}
			if (graphicsFamily >= 0 && presentFamily >= 0) break;
		}
		return { graphicsFamily, presentFamily };
	};

	for (const auto& device : devices) {
		auto [graphicsFamily, presentFamily] = findQueueFamilies(device);
		bool extensionsSupported = checkDeviceExtensionSupport(device);

		if (graphicsFamily >= 0 && presentFamily >= 0 && extensionsSupported) {
			// You may add more checks here (features, swapchain adequacy etc.)
			m_PhysicalDevice = device;
			break;
		}
	}

	if (m_PhysicalDevice == VK_NULL_HANDLE) {
		throw std::runtime_error("Failed to find a suitable GPU!");
	}
}
void VulkanRHI::CreateLogicalDevice() {
	// find queue families
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, nullptr);
	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, queueFamilies.data());

	int graphicsFamily = -1;
	int presentFamily = -1;
	for (uint32_t i = 0; i < queueFamilyCount; ++i) {
		if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			graphicsFamily = static_cast<int>(i);
		}
		VkBool32 presentSupport = VK_FALSE;
		vkGetPhysicalDeviceSurfaceSupportKHR(m_PhysicalDevice, i, m_Surface, &presentSupport);
		if (presentSupport) {
			presentFamily = static_cast<int>(i);
		}
		if (graphicsFamily >= 0 && presentFamily >= 0) break;
	}

	if (graphicsFamily < 0 || presentFamily < 0) {
		throw std::runtime_error("Failed to find required queue families for logical device");
	}

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<uint32_t> uniqueQueueFamilies = { (uint32_t)graphicsFamily, (uint32_t)presentFamily };

	float queuePriority = 1.0f;
	for (uint32_t queueFamily : uniqueQueueFamilies) {
		VkDeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamily;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.push_back(queueCreateInfo);
	}

	VkPhysicalDeviceFeatures deviceFeatures{};
	deviceFeatures.samplerAnisotropy = VK_TRUE;

	VkDeviceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	createInfo.pQueueCreateInfos = queueCreateInfos.data();
	createInfo.pEnabledFeatures = &deviceFeatures;

	createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	createInfo.ppEnabledExtensionNames = deviceExtensions.data();

	if (m_EnableValidationLayers) {
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();
	} else {
		createInfo.enabledLayerCount = 0;
	}

	if (vkCreateDevice(m_PhysicalDevice, &createInfo, nullptr, &m_Device) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create logical device");
	}

	vkGetDeviceQueue(m_Device, graphicsFamily, 0, &m_GraphicsQueue);
	vkGetDeviceQueue(m_Device, presentFamily, 0, &m_PresentQueue);
}
void VulkanRHI::CreateCommandPool() {
	// Find graphics queue family index again
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, nullptr);
	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, queueFamilies.data());

	int graphicsFamily = -1;
	for (uint32_t i = 0; i < queueFamilyCount; ++i) {
		if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			graphicsFamily = static_cast<int>(i);
			break;
		}
	}

	if (graphicsFamily < 0) {
		throw std::runtime_error("Failed to find graphics queue family for command pool");
	}

	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = static_cast<uint32_t>(graphicsFamily);
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	if (vkCreateCommandPool(m_Device, &poolInfo, nullptr, &m_CommandPool) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create command pool");
	}
}
void VulkanRHI::CreateSyncObjects() {
	// Use a small fixed number of frames-in-flight (classic pattern).
	constexpr size_t MAX_FRAMES_IN_FLIGHT = 2;

	// Destroy any previously-created render-finished semaphores (recreate path safety)
	for (auto sem : m_RenderFinishedSemaphores) {
		if (sem != VK_NULL_HANDLE) vkDestroySemaphore(m_Device, sem, nullptr);
	}
	m_RenderFinishedSemaphores.clear();

	m_ImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	// One render-finished semaphore per swapchain image to avoid reuse while still in use by presentation
	m_RenderFinishedSemaphores.resize(std::max<size_t>(1, m_SwapchainImages.size()));
	m_InFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	// Create per-frame image-available semaphores + in-flight fences
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
		if (vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &m_ImageAvailableSemaphores[i]) != VK_SUCCESS ||
			vkCreateFence(m_Device, &fenceInfo, nullptr, &m_InFlightFences[i]) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create synchronization objects for a frame");
		}
	}

	// Create one render-finished semaphore per swapchain image
	for (size_t i = 0; i < m_RenderFinishedSemaphores.size(); ++i) {
		if (vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &m_RenderFinishedSemaphores[i]) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create render-finished semaphore");
		}
	}

	// Ensure current frame index valid
	m_CurrentFrame = 0;
}
void VulkanRHI::CreateSwapchain()
{
	// Query surface capabilities
	VkSurfaceCapabilitiesKHR capabilities{};
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_PhysicalDevice, m_Surface, &capabilities);

	// Choose surface format
	uint32_t formatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface, &formatCount, nullptr);
	if (formatCount == 0) throw std::runtime_error("No surface formats available");

	std::vector<VkSurfaceFormatKHR> formats(formatCount);
	vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface, &formatCount, formats.data());

	VkSurfaceFormatKHR chosenFormat = formats[0];
	for (const auto& f : formats) {
		if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			chosenFormat = f;
			break;
		}
	}
	// Choose present mode
	VkPresentModeKHR chosenPresentMode = VK_PRESENT_MODE_FIFO_KHR; // guaranteed
	uint32_t presentModeCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(m_PhysicalDevice, m_Surface, &presentModeCount, nullptr);
	if (presentModeCount > 0) {
		std::vector<VkPresentModeKHR> presentModes(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(m_PhysicalDevice, m_Surface, &presentModeCount, presentModes.data());

		if (m_VSyncEnabled) {
			chosenPresentMode = VK_PRESENT_MODE_FIFO_KHR;
		}
		else {
			// prefer mailbox, then immediate, then fallback to FIFO
			if (std::find(presentModes.begin(), presentModes.end(), VK_PRESENT_MODE_MAILBOX_KHR) != presentModes.end()) {
				chosenPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
			}
			else if (std::find(presentModes.begin(), presentModes.end(), VK_PRESENT_MODE_IMMEDIATE_KHR) != presentModes.end()) {
				chosenPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
			}
			else {
				chosenPresentMode = VK_PRESENT_MODE_FIFO_KHR;
			}
		}
	}
	// Choose extent
	VkExtent2D extent{};
	if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
		extent = capabilities.currentExtent;
	}
	else {
		// if currentExtent is undefined, clamp to allowed range. Pick a reasonable default.
		uint32_t width = std::clamp<uint32_t>(800, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		uint32_t height = std::clamp<uint32_t>(600, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
		extent = { width, height };
	}

	// Image count
	uint32_t imageCount = capabilities.minImageCount + 1;
	if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
		imageCount = capabilities.maxImageCount;
	}
	// Determine queue family indices for sharing mode
	uint32_t graphicsFamilyIndex = UINT32_MAX;
	uint32_t presentFamilyIndex = UINT32_MAX;
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, nullptr);
	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, queueFamilies.data());
	for (uint32_t i = 0; i < queueFamilyCount; ++i) {
		if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) graphicsFamilyIndex = i;
		VkBool32 presentSupport = VK_FALSE;
		vkGetPhysicalDeviceSurfaceSupportKHR(m_PhysicalDevice, i, m_Surface, &presentSupport);
		if (presentSupport) presentFamilyIndex = i;
		if (graphicsFamilyIndex != UINT32_MAX && presentFamilyIndex != UINT32_MAX) break;
	}
	VkSwapchainCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = m_Surface;
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = chosenFormat.format;
	createInfo.imageColorSpace = chosenFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	// Sharing mode
	if (graphicsFamilyIndex != presentFamilyIndex && graphicsFamilyIndex != UINT32_MAX && presentFamilyIndex != UINT32_MAX) {
		uint32_t queueFamilyIndices[] = { graphicsFamilyIndex, presentFamilyIndex };
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else {
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.queueFamilyIndexCount = 0;
		createInfo.pQueueFamilyIndices = nullptr;
	}
	createInfo.preTransform = (capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR : capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = chosenPresentMode;
	createInfo.clipped = VK_TRUE;
	createInfo.oldSwapchain = VK_NULL_HANDLE;

	if (vkCreateSwapchainKHR(m_Device, &createInfo, nullptr, &m_Swapchain) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create swap chain!");
	}

	// Retrieve images
	uint32_t actualImageCount = 0;
	vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &actualImageCount, nullptr);
	m_SwapchainImages.resize(actualImageCount);
	vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &actualImageCount, m_SwapchainImages.data());
	m_SwapchainImageFormat = chosenFormat.format;
	m_SwapchainExtent = extent;

	// Ensure the per-image "in-flight" fence tracking matches the number of swapchain images
	m_ImagesInFlight.resize(m_SwapchainImages.size(), VK_NULL_HANDLE);

	// If a camera is active, update its aspect ratio to match the swapchain extent.
	if (m_ActiveCamera != nullptr && m_SwapchainExtent.height != 0) {
		float aspect = static_cast<float>(m_SwapchainExtent.width) / static_cast<float>(m_SwapchainExtent.height);
		m_ActiveCamera->SetAspect(aspect);
	}
}
void VulkanRHI::CreateImageViews()
{
	if (m_SwapchainImages.empty()) return;

	m_SwapchainImageViews.clear();
	m_SwapchainImageViews.reserve(m_SwapchainImages.size());

	for (const auto& image : m_SwapchainImages) {
		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = m_SwapchainImageFormat;
		viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;
		VkImageView imageView;
		if (vkCreateImageView(m_Device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create image views!");
		}
		m_SwapchainImageViews.push_back(imageView);
	}
}
void VulkanRHI::CreateRenderPass()
{
	// Simple single color attachment render pass
	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = m_SwapchainImageFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorAttachmentRef{};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;

	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &colorAttachment;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	if (vkCreateRenderPass(m_Device, &renderPassInfo, nullptr, &m_RenderPass) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create render pass!");
	}
}
void VulkanRHI::CreateFramebuffers()
{
	if (m_SwapchainImageViews.empty() || m_RenderPass == VK_NULL_HANDLE) return;

	m_SwapchainFramebuffers.clear();
	m_SwapchainFramebuffers.reserve(m_SwapchainImageViews.size());

	for (const auto& imageView : m_SwapchainImageViews) {
		VkImageView attachments[] = { imageView };

		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = m_RenderPass;
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = attachments;
		framebufferInfo.width = m_SwapchainExtent.width;
		framebufferInfo.height = m_SwapchainExtent.height;
		framebufferInfo.layers = 1;

		VkFramebuffer framebuffer;
		if (vkCreateFramebuffer(m_Device, &framebufferInfo, nullptr, &framebuffer) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create framebuffer!");
		}
		m_SwapchainFramebuffers.push_back(framebuffer);
	}
}
void VulkanRHI::CleanupSwapchain()
{
	// Ensure device is idle before destroying resources
	if (m_Device != VK_NULL_HANDLE) {
		vkDeviceWaitIdle(m_Device);
	}

	for (auto fb : m_SwapchainFramebuffers) {
		if (fb != VK_NULL_HANDLE) {
			vkDestroyFramebuffer(m_Device, fb, nullptr);
		}
	}
	m_SwapchainFramebuffers.clear();

	for (auto iv : m_SwapchainImageViews) {
		if (iv != VK_NULL_HANDLE) {
			vkDestroyImageView(m_Device, iv, nullptr);
		}
	}
	m_SwapchainImageViews.clear();

	if (m_Swapchain != VK_NULL_HANDLE) {
		vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);
		m_Swapchain = VK_NULL_HANDLE;
	}
	m_SwapchainImages.clear();
	m_SwapchainImageFormat = VK_FORMAT_UNDEFINED;
	m_SwapchainExtent = {};

	// Reset per-image fences
	m_ImagesInFlight.clear();

	if (m_RenderPass != VK_NULL_HANDLE) {
		vkDestroyRenderPass(m_Device, m_RenderPass, nullptr);
		m_RenderPass = VK_NULL_HANDLE;
	}
}
void VulkanRHI::AllocateCommandBuffers()
{
	if (m_CommandPool == VK_NULL_HANDLE) {
		throw std::runtime_error("Attempted to allocate command buffers but command pool is VK_NULL_HANDLE");
	}

	// Free any existing command buffers first
	if (!m_CommandBuffers.empty()) {
		FreeCommandBuffers();
	}

	// Allocate one command buffer per framebuffer (swapchain image)
	size_t count = m_SwapchainFramebuffers.size();
	if (count == 0) {
		// nothing to allocate yet
		m_CommandBuffers.clear();
		return;
	}

	m_CommandBuffers.resize(count);

	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = m_CommandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = static_cast<uint32_t>(count);

	if (vkAllocateCommandBuffers(m_Device, &allocInfo, m_CommandBuffers.data()) != VK_SUCCESS) {
		throw std::runtime_error("Failed to allocate command buffers");
	}
}
void VulkanRHI::FreeCommandBuffers()
{
	if (m_CommandPool == VK_NULL_HANDLE) return;

	if (!m_CommandBuffers.empty()) {
		vkFreeCommandBuffers(m_Device, m_CommandPool, static_cast<uint32_t>(m_CommandBuffers.size()), m_CommandBuffers.data());
		m_CommandBuffers.clear();
	}
}
void VulkanRHI::RecordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex)
{
	if (cmd == VK_NULL_HANDLE) {
		throw std::runtime_error("RecordCommandBuffer called with VK_NULL_HANDLE command buffer");
	}
	if (imageIndex >= m_SwapchainFramebuffers.size()) {
		throw std::runtime_error("RecordCommandBuffer imageIndex out of range");
	}
	if (m_RenderPass == VK_NULL_HANDLE) {
		throw std::runtime_error("RecordCommandBuffer called but render pass is not created");
	}

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	// allow re-recording / simultaneous use depending on usage; choose simultaneous to be safe
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
	beginInfo.pInheritanceInfo = nullptr;

	if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
		throw std::runtime_error("Failed to begin recording command buffer");
	}

	VkClearValue clearColor{};
	clearColor.color = { {0.2f,0.2f,0.2f,1.0f} };

	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = m_RenderPass;
	renderPassInfo.framebuffer = m_SwapchainFramebuffers[imageIndex];
	renderPassInfo.renderArea.offset = {0,0 };
	renderPassInfo.renderArea.extent = m_SwapchainExtent;
	renderPassInfo.clearValueCount =1;
	renderPassInfo.pClearValues = &clearColor;

	// Begin the render pass but DO NOT end it here. Higher-level code should record draw commands
	// using the command buffer returned by GetCurrentCommandBuffer(), and EndFrame() will end
	// the render pass and finish recording before submitting.
	vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	// Do not call vkCmdEndRenderPass or vkEndCommandBuffer here to allow the caller to record draws.
}
VkCommandBuffer VulkanRHI::BeginSingleTimeCommands() {
	if (m_CommandPool == VK_NULL_HANDLE || m_Device == VK_NULL_HANDLE) {
		throw std::runtime_error("BeginSingleTimeCommands called but command pool or device is not initialized");
	}

	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = m_CommandPool;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	if (vkAllocateCommandBuffers(m_Device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
		throw std::runtime_error("Failed to allocate single-time command buffer");
	}

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	beginInfo.pInheritanceInfo = nullptr;

	if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
		// free the allocated buffer on failure to avoid leaks
		vkFreeCommandBuffers(m_Device, m_CommandPool, 1, &commandBuffer);
		throw std::runtime_error("Failed to begin single-time command buffer");
	}

	return commandBuffer;
}
void VulkanRHI::EndSingleTimeCommands(VkCommandBuffer commandBuffer) {
	if (commandBuffer == VK_NULL_HANDLE) {
		throw std::runtime_error("EndSingleTimeCommands called with VK_NULL_HANDLE");
	}
	if (m_Device == VK_NULL_HANDLE || m_CommandPool == VK_NULL_HANDLE || m_GraphicsQueue == VK_NULL_HANDLE) {
		throw std::runtime_error("EndSingleTimeCommands called but device/command pool/graphics queue is not initialized");
	}

	if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
		throw std::runtime_error("Failed to record single-time command buffer");
	}

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	if (vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
		// still attempt to free the buffer before throwing
		vkFreeCommandBuffers(m_Device, m_CommandPool, 1, &commandBuffer);
		throw std::runtime_error("Failed to submit single-time command buffer");
	}

	// Wait for the operation to finish, then free the command buffer
	vkQueueWaitIdle(m_GraphicsQueue);
	vkFreeCommandBuffers(m_Device, m_CommandPool, 1, &commandBuffer);
}
void VulkanRHI::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
{
	if (m_Device == VK_NULL_HANDLE)
		throw std::runtime_error("CreateBuffer called but device is not initialized");

	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateBuffer(m_Device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create buffer");
	}

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(m_Device, buffer, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, properties);

	if (vkAllocateMemory(m_Device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
		vkDestroyBuffer(m_Device, buffer, nullptr);
		throw std::runtime_error("Failed to allocate buffer memory");
	}

	vkBindBufferMemory(m_Device, buffer, bufferMemory, 0);
}
void VulkanRHI::CopyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size)
{
	VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

	VkBufferCopy copyRegion{};
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = 0;
	copyRegion.size = size;
	vkCmdCopyBuffer(commandBuffer, src, dst, 1, &copyRegion);

	EndSingleTimeCommands(commandBuffer);
}
void VulkanRHI::CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory)
{
	if (m_Device == VK_NULL_HANDLE)
		throw std::runtime_error("CreateImage called but device is not initialized");

	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = format;
	imageInfo.tiling = tiling;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = usage;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	if (vkCreateImage(m_Device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create image");
	}

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(m_Device, image, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, properties);

	if (vkAllocateMemory(m_Device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
		vkDestroyImage(m_Device, image, nullptr);
		throw std::runtime_error("Failed to allocate image memory");
	}

	vkBindImageMemory(m_Device, image, imageMemory, 0);
}
VkImageView VulkanRHI::CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)
{
	if (m_Device == VK_NULL_HANDLE)
		throw std::runtime_error("CreateImageView called but device is not initialized");

	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = format;
	viewInfo.subresourceRange.aspectMask = aspectFlags;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	VkImageView imageView;
	if (vkCreateImageView(m_Device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create texture image view");
	}
	return imageView;
}
void VulkanRHI::TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
{
	VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;

	// Determine aspect
	if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL || newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL ||
		oldLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL || oldLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL) {
		// depth/stencil
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		// If format has stencil component, include it
		if (HasStencilComponent(format)) {
			barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
	}
	else {
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	}

	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	VkPipelineStageFlags sourceStage = 0;
	VkPipelineStageFlags destinationStage = 0;

	// Source/destination access masks and pipeline stages depend on layouts
	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else {
		// Fallback: try to handle generic transitions (conservative)
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = 0;
		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	}

	vkCmdPipelineBarrier(
		commandBuffer,
		sourceStage, destinationStage,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier
	);

	EndSingleTimeCommands(commandBuffer);
}
void VulkanRHI::CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
{
	VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

	VkBufferImageCopy region{};
	region.bufferOffset = 0;
	region.bufferRowLength = 0; // tightly packed
	region.bufferImageHeight = 0;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageOffset = { 0, 0, 0 };
	region.imageExtent = { width, height, 1 };

	vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	EndSingleTimeCommands(commandBuffer);
}
VkSampler VulkanRHI::CreateSampler()
{
	if (m_Device == VK_NULL_HANDLE || m_PhysicalDevice == VK_NULL_HANDLE)
		throw std::runtime_error("CreateSampler called but device or physical device is not initialized");

	VkPhysicalDeviceProperties properties{};
	vkGetPhysicalDeviceProperties(m_PhysicalDevice, &properties);

	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.anisotropyEnable = VK_TRUE;
	samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy > 1.0f ? properties.limits.maxSamplerAnisotropy : 1.0f;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
	samplerInfo.mipLodBias = 0.0f;

	VkSampler sampler;
	if (vkCreateSampler(m_Device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create texture sampler");
	}
	return sampler;
}
uint32_t VulkanRHI::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(m_PhysicalDevice, &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}

	throw std::runtime_error("Failed to find suitable memory type");
}
void VulkanRHI::GenerateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels)
{
	// Check if image format supports linear blitting
	VkFormatProperties formatProperties;
	vkGetPhysicalDeviceFormatProperties(m_PhysicalDevice, imageFormat, &formatProperties);

	if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
		throw std::runtime_error("Texture image format does not support linear blitting for mipmap generation!");
	}

	VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.image = image;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.subresourceRange.levelCount = 1;

	int32_t mipWidth = texWidth;
	int32_t mipHeight = texHeight;

	for (uint32_t i = 1; i < mipLevels; i++) {
		// transition i-1 level to TRANSFER_SRC
		barrier.subresourceRange.baseMipLevel = i - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

		vkCmdPipelineBarrier(commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
			0, nullptr,
			0, nullptr,
			1, &barrier);

		// Blit from i-1 to i
		VkImageBlit blit{};
		blit.srcOffsets[0] = { 0, 0, 0 };
		blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
		blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.srcSubresource.mipLevel = i - 1;
		blit.srcSubresource.baseArrayLayer = 0;
		blit.srcSubresource.layerCount = 1;

		blit.dstOffsets[0] = { 0, 0, 0 };
		blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
		blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.dstSubresource.mipLevel = i;
		blit.dstSubresource.baseArrayLayer = 0;
		blit.dstSubresource.layerCount = 1;

		vkCmdBlitImage(commandBuffer,
			image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &blit,
			VK_FILTER_LINEAR);

		// Transition i-1 level to SHADER_READ_ONLY
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
			0, nullptr,
			0, nullptr,
			1, &barrier);

		if (mipWidth > 1) mipWidth /= 2;
		if (mipHeight > 1) mipHeight /= 2;
	}

	// Transition last mip level to SHADER_READ_ONLY
	barrier.subresourceRange.baseMipLevel = mipLevels - 1;
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	vkCmdPipelineBarrier(commandBuffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
		0, nullptr,
		0, nullptr,
		1, &barrier);

	EndSingleTimeCommands(commandBuffer);
}

// --- Camera integration methods ---
void VulkanRHI::SetActiveCamera(Camera* camera)
{
    m_ActiveCamera = camera;
    if (m_ActiveCamera != nullptr && m_SwapchainExtent.height != 0) {
        float aspect = static_cast<float>(m_SwapchainExtent.width) / static_cast<float>(m_SwapchainExtent.height);
        m_ActiveCamera->SetAspect(aspect);
    }
}
Camera* VulkanRHI::GetActiveCamera() const
{
    return m_ActiveCamera;
}
void VulkanRHI::CreateCameraUniformBufferAndWriteDescriptors()
{
    // Create a single host-visible uniform buffer large enough for view+proj matrices.
    if (m_Device == VK_NULL_HANDLE) return;

    if (m_CameraUniformBuffer == VK_NULL_HANDLE)
    {
        CreateBuffer(static_cast<VkDeviceSize>(m_CameraUniformBufferSize),
                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     m_CameraUniformBuffer,
                     m_CameraUniformBufferMemory);
    }

    // Update each allocated descriptor set to reference the camera uniform buffer at binding 0.
    for (size_t i = 0; i < s_DescriptorSets.size(); ++i)
    {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = m_CameraUniformBuffer;
        bufferInfo.offset = 0;
        bufferInfo.range = static_cast<VkDeviceSize>(m_CameraUniformBufferSize);

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = s_DescriptorSets[i];
        descriptorWrite.dstBinding = 0; // binding 0 is the UBO in CreateDescriptorSetLayout
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &bufferInfo;
        descriptorWrite.pImageInfo = nullptr;
        descriptorWrite.pTexelBufferView = nullptr;

        vkUpdateDescriptorSets(m_Device, 1, &descriptorWrite, 0, nullptr);
    }
}
void VulkanRHI::UpdateCameraBuffer()
{
    if (m_Device == VK_NULL_HANDLE || m_CameraUniformBuffer == VK_NULL_HANDLE) return;

    // Prepare two mat4's: view followed by projection (in column-major order as glm)
    glm::mat4 view = glm::mat4(1.0f);
    glm::mat4 proj = glm::mat4(1.0f);

    if (m_ActiveCamera != nullptr) {
        view = m_ActiveCamera->GetViewMatrix();
        proj = m_ActiveCamera->GetProjectionMatrix();

        // GLM's perspective uses OpenGL clip-space by default; for Vulkan flip Y.
        proj[1][1] *= -1.0f;
    }

    // Map, copy, and unmap
    void* data;
    if (vkMapMemory(m_Device, m_CameraUniformBufferMemory, 0, m_CameraUniformBufferSize, 0, &data) == VK_SUCCESS)
    {
        // copy view then proj
        std::memcpy(data, glm::value_ptr(view), sizeof(float) * 16);
        std::memcpy(static_cast<uint8_t*>(data) + sizeof(float) * 16, glm::value_ptr(proj), sizeof(float) * 16);
        vkUnmapMemory(m_Device, m_CameraUniformBufferMemory);
    }
}
void VulkanRHI::CreateDescriptorSetLayout()
{
    if (m_Device == VK_NULL_HANDLE) throw std::runtime_error("CreateDescriptorSetLayout called but device is not initialized");

    VkDescriptorSetLayoutBinding uboBinding{};
    uboBinding.binding = 0;
    uboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboBinding.descriptorCount = 1;
    uboBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    uboBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutBinding samplerBinding{};
    samplerBinding.binding = 1;
    samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerBinding.descriptorCount = 1;
    samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    samplerBinding.pImmutableSamplers = nullptr;

    std::vector<VkDescriptorSetLayoutBinding> bindings = { uboBinding, samplerBinding };

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (s_DescriptorSetLayout != VK_NULL_HANDLE) {
        // reuse/avoid double-create in recreate path
        return;
    }

    if (vkCreateDescriptorSetLayout(m_Device, &layoutInfo, nullptr, &s_DescriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor set layout");
    }
}
void VulkanRHI::CreateDescriptorPool()
{
	if (m_Device == VK_NULL_HANDLE) throw std::runtime_error("CreateDescriptorPool called but device is not initialized");

	// Determine count similarly to AllocateDescriptorSets logic so pool sizes match expected allocations.
	uint32_t count = static_cast<uint32_t>(m_InFlightFences.size());
	if (count == 0) count = s_MaxDescriptorFrames;
	if (!m_SwapchainImages.empty()) {
		count = std::max<uint32_t>(count, static_cast<uint32_t>(m_SwapchainImages.size()));
	}
	count = std::max<uint32_t>(count, s_MaxDescriptorFrames);

	VkDescriptorPoolSize poolSizes[2]{};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = count;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[1].descriptorCount = count;

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = 2;
	poolInfo.pPoolSizes = poolSizes;
	poolInfo.maxSets = count;

	// If a pool already exists (recreate path), destroy it first to avoid leaks.
	if (s_DescriptorPool != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(m_Device, s_DescriptorPool, nullptr);
		s_DescriptorPool = VK_NULL_HANDLE;
	}

	if (vkCreateDescriptorPool(m_Device, &poolInfo, nullptr, &s_DescriptorPool) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create descriptor pool");
	}
}
void VulkanRHI::AllocateDescriptorSets()
{
	if (m_Device == VK_NULL_HANDLE) throw std::runtime_error("AllocateDescriptorSets called but device is not initialized");
	if (s_DescriptorSetLayout == VK_NULL_HANDLE) throw std::runtime_error("AllocateDescriptorSets called but descriptor set layout is not created");
	if (s_DescriptorPool == VK_NULL_HANDLE) throw std::runtime_error("AllocateDescriptorSets called but descriptor pool is not created");

	// Match the same logic used when creating the pool so counts align.
	uint32_t count = static_cast<uint32_t>(m_InFlightFences.size());
	if (count == 0) count = s_MaxDescriptorFrames;
	if (!m_SwapchainImages.empty()) {
		count = std::max<uint32_t>(count, static_cast<uint32_t>(m_SwapchainImages.size()));
	}
	count = std::max<uint32_t>(count, s_MaxDescriptorFrames);

	std::vector<VkDescriptorSetLayout> layouts(count, s_DescriptorSetLayout);

	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = s_DescriptorPool;
	allocInfo.descriptorSetCount = count;
	allocInfo.pSetLayouts = layouts.data();

	s_DescriptorSets.resize(count);
	if (vkAllocateDescriptorSets(m_Device, &allocInfo, s_DescriptorSets.data()) != VK_SUCCESS) {
		throw std::runtime_error("Failed to allocate descriptor sets");
	}

	// Create and bind camera uniform buffer and write descriptors to point at it.
	CreateCameraUniformBufferAndWriteDescriptors();
}
void VulkanRHI::CreatePipelineLayout()
{
	if (m_Device == VK_NULL_HANDLE) throw std::runtime_error("CreatePipelineLayout called but device is not initialized");

	// If a pipeline layout already exists, destroy it first (recreate path)
	if (s_PipelineLayout != VK_NULL_HANDLE) {
		vkDestroyPipelineLayout(m_Device, s_PipelineLayout, nullptr);
		s_PipelineLayout = VK_NULL_HANDLE;
	}

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

	if (s_DescriptorSetLayout != VK_NULL_HANDLE) {
		pipelineLayoutInfo.setLayoutCount = 1;
		pipelineLayoutInfo.pSetLayouts = &s_DescriptorSetLayout;
	}
	else {
		pipelineLayoutInfo.setLayoutCount = 0;
		pipelineLayoutInfo.pSetLayouts = nullptr;
	}

	// No push constants from RHI-level layout here; individual pipelines may add their own push-constant ranges.
	pipelineLayoutInfo.pushConstantRangeCount = 0;
	pipelineLayoutInfo.pPushConstantRanges = nullptr;

	if (vkCreatePipelineLayout(m_Device, &pipelineLayoutInfo, nullptr, &s_PipelineLayout) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create pipeline layout");
	}
}
bool VulkanRHI::HasStencilComponent(VkFormat format) const
{
	return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}