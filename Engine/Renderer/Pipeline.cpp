#include "Pipeline.h"
#include "VulkanRHI.h"
#include "Mesh.h" // for Vertex binding/attribute descriptions

#include <fstream>
#include <stdexcept>
#include <vector>
#include <cstring>

Pipeline::Pipeline() = default;

Pipeline::~Pipeline()
{
    Destroy();
}

Pipeline::Pipeline(Pipeline&& other) noexcept
{
    m_RHI = other.m_RHI;
    m_Device = other.m_Device;
    m_Pipeline = other.m_Pipeline;
    m_PipelineLayout = other.m_PipelineLayout;
    m_VertModule = other.m_VertModule;
    m_FragModule = other.m_FragModule;
    m_RenderPass = other.m_RenderPass;
    m_Extent = other.m_Extent;

    other.m_RHI = nullptr;
    other.m_Device = VK_NULL_HANDLE;
    other.m_Pipeline = VK_NULL_HANDLE;
    other.m_PipelineLayout = VK_NULL_HANDLE;
    other.m_VertModule = VK_NULL_HANDLE;
    other.m_FragModule = VK_NULL_HANDLE;
    other.m_RenderPass = VK_NULL_HANDLE;
    other.m_Extent = {};
}

Pipeline& Pipeline::operator=(Pipeline&& other) noexcept
{
    if (this != &other)
    {
        Destroy();

        m_RHI = other.m_RHI;
        m_Device = other.m_Device;
        m_Pipeline = other.m_Pipeline;
        m_PipelineLayout = other.m_PipelineLayout;
        m_VertModule = other.m_VertModule;
        m_FragModule = other.m_FragModule;
        m_RenderPass = other.m_RenderPass;
        m_Extent = other.m_Extent;

        other.m_RHI = nullptr;
        other.m_Device = VK_NULL_HANDLE;
        other.m_Pipeline = VK_NULL_HANDLE;
        other.m_PipelineLayout = VK_NULL_HANDLE;
        other.m_VertModule = VK_NULL_HANDLE;
        other.m_FragModule = VK_NULL_HANDLE;
        other.m_RenderPass = VK_NULL_HANDLE;
        other.m_Extent = {};
    }
    return *this;
}

std::vector<char> Pipeline::ReadFile(const std::string& path) const
{
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error("Failed to open file: " + path);

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    return buffer;
}

VkShaderModule Pipeline::CreateShaderModule(const std::vector<char>& code) const
{
    if (code.empty())
        throw std::runtime_error("Shader code is empty");

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule module = VK_NULL_HANDLE;
    if (vkCreateShaderModule(m_Device, &createInfo, nullptr, &module) != VK_SUCCESS)
        throw std::runtime_error("Failed to create shader module");

    return module;
}

bool Pipeline::CreateGraphicsPipeline(const std::vector<char>& vertCode, const std::vector<char>& fragCode)
{
    m_VertModule = CreateShaderModule(vertCode);
    m_FragModule = CreateShaderModule(fragCode);

    VkPipelineShaderStageCreateInfo vertStage{};
    vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = m_VertModule;
    vertStage.pName = "main";

    VkPipelineShaderStageCreateInfo fragStage{};
    fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = m_FragModule;
    fragStage.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertStage, fragStage };

    // Vertex input (use Mesh::Vertex)
    auto bindingDescription = Mesh::Vertex::GetBindingDescription();
    auto attributeDescriptions = Mesh::Vertex::GetAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(m_Extent.width);
    viewport.height = static_cast<float>(m_Extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = m_Extent;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    // Pipeline layout (no descriptor sets by default). The application can create descriptor set layouts and update pipeline layout if needed later.
    // Create a pipeline layout that includes a push-constant range for a 4x4 matrix (per-object transform).
    // This enables the application to upload a glm::mat4 (16 floats = 64 bytes) as a push constant.
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(float) * 16; // 4x4 matrix

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    // If the pipeline is created with an RHI that provides a descriptor set layout (e.g. camera UBO),
    // include it here so shaders can access descriptors at set=0.
    VkDescriptorSetLayout rhiLayout = VK_NULL_HANDLE;
    if (m_RHI) {
        rhiLayout = m_RHI->GetDescriptorSetLayout();
    }
    if (rhiLayout != VK_NULL_HANDLE) {
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &rhiLayout;
    } else {
    pipelineLayoutInfo.setLayoutCount = 0;
    pipelineLayoutInfo.pSetLayouts = nullptr;
    }
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(m_Device, &pipelineLayoutInfo, nullptr, &m_PipelineLayout) != VK_SUCCESS)
        throw std::runtime_error("Failed to create pipeline layout");

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = nullptr; // no depth/stencil by default
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = nullptr;
    pipelineInfo.layout = m_PipelineLayout;
    pipelineInfo.renderPass = m_RenderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;

    if (vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_Pipeline) != VK_SUCCESS)
        throw std::runtime_error("Failed to create graphics pipeline");

    return true;
}

bool Pipeline::Initialize(VulkanRHI* rhi, VkRenderPass renderPass, VkExtent2D extent, const std::string& vertSpvPath, const std::string& fragSpvPath)
{
    if (!rhi)
        return false;

    m_RHI = rhi;
    m_Device = m_RHI->GetDevice();
    m_RenderPass = renderPass;
    m_Extent = extent;

    if (m_Device == VK_NULL_HANDLE)
        return false;

    try
    {
        auto vertCode = ReadFile(vertSpvPath);
        auto fragCode = ReadFile(fragSpvPath);
        return CreateGraphicsPipeline(vertCode, fragCode);
    }
    catch (const std::exception&)
    {
        Destroy();
        return false;
    }
}

void Pipeline::Destroy()
{
    if (m_Device == VK_NULL_HANDLE)
        return;

    if (m_Pipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(m_Device, m_Pipeline, nullptr);
        m_Pipeline = VK_NULL_HANDLE;
    }
    if (m_PipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(m_Device, m_PipelineLayout, nullptr);
        m_PipelineLayout = VK_NULL_HANDLE;
    }
    if (m_VertModule != VK_NULL_HANDLE)
    {
        vkDestroyShaderModule(m_Device, m_VertModule, nullptr);
        m_VertModule = VK_NULL_HANDLE;
    }
    if (m_FragModule != VK_NULL_HANDLE)
    {
        vkDestroyShaderModule(m_Device, m_FragModule, nullptr);
        m_FragModule = VK_NULL_HANDLE;
    }

    m_Device = VK_NULL_HANDLE;
    m_RHI = nullptr;
    m_RenderPass = VK_NULL_HANDLE;
    m_Extent = {};
}

void Pipeline::Bind(VkCommandBuffer cmd) const
{
    if (m_Pipeline != VK_NULL_HANDLE && cmd != VK_NULL_HANDLE)
    {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline);

        // Bind descriptor sets (if any)
        const auto& descSets = m_RHI->GetDescriptorSets();
        if (!descSets.empty())
        {
            vkCmdBindDescriptorSets(cmd,
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    m_PipelineLayout,
                                    0,
                                    1,
                                    &descSets[0],
                                    0,
                                    nullptr);
        }
    }
}