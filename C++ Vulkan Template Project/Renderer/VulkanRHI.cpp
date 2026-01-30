#include "VulkanRHI.h"
#include "Window.h"

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <vector>
#include <set>
#include <string>
#include <stdexcept>
#include <cstring>
#include <algorithm>
#include <iostream>

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
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
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
	const int MAX_FRAMES_IN_FLIGHT = 2;
	m_ImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	m_RenderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	m_InFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
		if (vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &m_ImageAvailableSemaphores[i]) != VK_SUCCESS ||
			vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &m_RenderFinishedSemaphores[i]) != VK_SUCCESS ||
			vkCreateFence(m_Device, &fenceInfo, nullptr, &m_InFlightFences[i]) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create synchronization objects for a frame");
		}
	}
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
	m_SwapchainImageViews.clear();

	if (m_Swapchain != VK_NULL_HANDLE) {
		vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);
		m_Swapchain = VK_NULL_HANDLE;
	}
	m_SwapchainImages.clear();
	m_SwapchainImageFormat = VK_FORMAT_UNDEFINED;
	m_SwapchainExtent = {};

	if (m_RenderPass != VK_NULL_HANDLE) {
		vkDestroyRenderPass(m_Device, m_RenderPass, nullptr);
		m_RenderPass = VK_NULL_HANDLE;
	}
}

