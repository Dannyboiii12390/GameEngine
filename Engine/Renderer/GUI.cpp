#include "GUI.h"
#include <array>
#include <cstdio>

#include "../IMGUI/imgui.h"
#include "../IMGUI/imgui_impl_glfw.h"
#include "../IMGUI/imgui_impl_vulkan.h"

#include "VulkanRHI.h"
#include "Window.h"

bool GUI::Create(GLFWwindow* window,
    VkInstance instance,
    VkPhysicalDevice physicalDevice,
    VkDevice device,
    uint32_t queueFamily,
    VkQueue queue,
    VkPipelineCache pipelineCache,
    VkRenderPass renderPass,
    uint32_t imageCount,
    VkAllocationCallbacks* allocator)
{
    if (m_initialized) return true;

    m_allocator = allocator;
    m_device = device;

    // Basic ImGui context + GLFW init
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls

    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForVulkan(window, true);

    // Create descriptor pool for ImGui
    std::array<VkDescriptorPoolSize, 11> pool_sizes = {
        VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000 * static_cast<uint32_t>(pool_sizes.size());
    pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
    pool_info.pPoolSizes = pool_sizes.data();

    const VkResult err = vkCreateDescriptorPool(device, &pool_info, allocator, &m_descriptorPool);

    // Setup ImGui Vulkan init info
    ImGui_ImplVulkan_InitInfo init_info{};
    init_info.Instance = instance;
    init_info.PhysicalDevice = physicalDevice;
    init_info.Device = device;
    init_info.QueueFamily = queueFamily;
    init_info.Queue = queue;
    init_info.PipelineCache = pipelineCache;
    init_info.DescriptorPool = m_descriptorPool;
    init_info.PipelineInfoMain.RenderPass = renderPass;
    init_info.PipelineInfoMain.Subpass = 0;
    init_info.MinImageCount = imageCount;
    init_info.ImageCount = imageCount;
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.Allocator = allocator;
    init_info.CheckVkResultFn = [](VkResult result) {
        if (result != VK_SUCCESS) {
            std::fprintf(stderr, "ImGui Vulkan error: VkResult = %d\n", result);
        }
        };

    const bool initResult = ImGui_ImplVulkan_Init(&init_info);

    m_initialized = true;
    return true;
}

bool GUI::Create(VulkanRHI& rhi, Window& window)
{
    return Create(window.getGLFWwindow(),
        rhi.GetInstance(),
        rhi.GetPhysicalDevice(),
        rhi.GetDevice(),
        0u, // Assuming graphics queue supports presentation, otherwise need separate present queue family index
        rhi.GetGraphicsQueue(),
        VK_NULL_HANDLE, // ImGui doesn't use a pipeline cache, so we can pass VK_NULL_HANDLE here
        rhi.GetRenderPass(),
        2u,
        nullptr);
}
void GUI::NewFrame() const
{
    if (!m_initialized) return;
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void GUI::Render(VkCommandBuffer commandBuffer) const
{
    if (!m_initialized) return;
    ImGui::Render();
    ImDrawData* const draw_data = ImGui::GetDrawData();
    if (draw_data)
        ImGui_ImplVulkan_RenderDrawData(draw_data, commandBuffer);
}

void GUI::Shutdown() noexcept
{
    if (!m_initialized) return;

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (m_descriptorPool != VK_NULL_HANDLE && m_device != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_device, m_descriptorPool, m_allocator);
        m_descriptorPool = VK_NULL_HANDLE;
    }

    m_initialized = false;
}