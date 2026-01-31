#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <array>
#include <cstdint>

class VulkanRHI;

// Simple GPU mesh helper. Manages a vertex and optional index buffer.
// Usage:
//  - Create a Mesh instance.
//  - Call Initialize(rhi, vertices, indices).
//  - During rendering call Bind(cmd) then Draw(cmd).
//  - Call Destroy() before shutdown or when re-creating resources.
class Mesh
{
public:
    struct Vertex
    {
        float position[3];
        float normal[3];
        float uv[2];

        static VkVertexInputBindingDescription GetBindingDescription();
        static std::array<VkVertexInputAttributeDescription, 3> GetAttributeDescriptions();
    };

    Mesh();
    ~Mesh();

    // non-copyable
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    // movable (moves ownership of Vulkan resources)
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

    // Initialize GPU buffers for the mesh. Returns false on failure.
    bool Initialize(VulkanRHI* rhi, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);

    // Release GPU resources
    void Destroy();

    // Bind the mesh's vertex/index buffers to command buffer
    void Bind(VkCommandBuffer cmd) const;

    // Issue draw call. If indices were provided, draws indexed.
    void Draw(VkCommandBuffer cmd) const;

    // Bind descriptor sets relevant to the mesh (e.g. material textures).
    // - layout: pipeline layout the descriptor sets were created for
    // - firstSet: first set index to bind at
    // - descriptorSets: descriptor sets to bind
    // - dynamicOffsets: dynamic offsets if any (optional)
    void BindDescriptorSets(VkCommandBuffer cmd, VkPipelineLayout layout, uint32_t firstSet, const std::vector<VkDescriptorSet>& descriptorSets, const std::vector<uint32_t>& dynamicOffsets = {}) const;

    bool IsValid() const { return m_Device != VK_NULL_HANDLE && m_VertexBuffer != VK_NULL_HANDLE; }
    uint32_t GetIndexCount() const { return m_IndexCount; }

private:
    // helpers
    bool CreateVertexBuffer(const std::vector<Vertex>& vertices);
    bool CreateIndexBuffer(const std::vector<uint32_t>& indices);
    bool CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
    VkCommandBuffer BeginSingleTimeCommands() const;
    void EndSingleTimeCommands(VkCommandBuffer commandBuffer) const;

private:
    VulkanRHI* m_RHI = nullptr;

    VkDevice m_Device = VK_NULL_HANDLE;
    VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
    VkQueue m_GraphicsQueue = VK_NULL_HANDLE;
    VkCommandPool m_CommandPool = VK_NULL_HANDLE;

    VkBuffer m_VertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_VertexBufferMemory = VK_NULL_HANDLE;

    VkBuffer m_IndexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_IndexBufferMemory = VK_NULL_HANDLE;
    uint32_t m_IndexCount = 0;

    uint32_t m_VertexCount = NULL;
};

// Inline convenience implementation for binding descriptor sets.
inline void Mesh::BindDescriptorSets(VkCommandBuffer cmd, VkPipelineLayout layout, uint32_t firstSet, const std::vector<VkDescriptorSet>& descriptorSets, const std::vector<uint32_t>& dynamicOffsets) const
{
    if (cmd == VK_NULL_HANDLE || layout == VK_NULL_HANDLE)
        return;

    if (descriptorSets.empty())
        return;

    vkCmdBindDescriptorSets(
        cmd,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        layout,
        firstSet,
        static_cast<uint32_t>(descriptorSets.size()),
        descriptorSets.data(),
        static_cast<uint32_t>(dynamicOffsets.size()),
        dynamicOffsets.empty() ? nullptr : dynamicOffsets.data()
    );
}