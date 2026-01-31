
#include "Mesh.h"
#include "VulkanRHI.h"

#include <stdexcept>
#include <cstring>
#include <cassert>

Mesh::Mesh() = default;

Mesh::~Mesh()
{
    Destroy();
}

Mesh::Mesh(Mesh&& other) noexcept
{
    m_RHI = other.m_RHI;
    m_Device = other.m_Device;
    m_PhysicalDevice = other.m_PhysicalDevice;
    m_GraphicsQueue = other.m_GraphicsQueue;
    m_CommandPool = other.m_CommandPool;

    m_VertexBuffer = other.m_VertexBuffer;
    m_VertexBufferMemory = other.m_VertexBufferMemory;
    m_IndexBuffer = other.m_IndexBuffer;
    m_IndexBufferMemory = other.m_IndexBufferMemory;
    m_IndexCount = other.m_IndexCount;

    other.m_RHI = nullptr;
    other.m_Device = VK_NULL_HANDLE;
    other.m_PhysicalDevice = VK_NULL_HANDLE;
    other.m_GraphicsQueue = VK_NULL_HANDLE;
    other.m_CommandPool = VK_NULL_HANDLE;
    other.m_VertexBuffer = VK_NULL_HANDLE;
    other.m_VertexBufferMemory = VK_NULL_HANDLE;
    other.m_IndexBuffer = VK_NULL_HANDLE;
    other.m_IndexBufferMemory = VK_NULL_HANDLE;
    other.m_IndexCount = 0;
}

Mesh& Mesh::operator=(Mesh&& other) noexcept
{
    if (this != &other)
    {
        Destroy();

        m_RHI = other.m_RHI;
        m_Device = other.m_Device;
        m_PhysicalDevice = other.m_PhysicalDevice;
        m_GraphicsQueue = other.m_GraphicsQueue;
        m_CommandPool = other.m_CommandPool;

        m_VertexBuffer = other.m_VertexBuffer;
        m_VertexBufferMemory = other.m_VertexBufferMemory;
        m_IndexBuffer = other.m_IndexBuffer;
        m_IndexBufferMemory = other.m_IndexBufferMemory;
        m_IndexCount = other.m_IndexCount;

        other.m_RHI = nullptr;
        other.m_Device = VK_NULL_HANDLE;
        other.m_PhysicalDevice = VK_NULL_HANDLE;
        other.m_GraphicsQueue = VK_NULL_HANDLE;
        other.m_CommandPool = VK_NULL_HANDLE;
        other.m_VertexBuffer = VK_NULL_HANDLE;
        other.m_VertexBufferMemory = VK_NULL_HANDLE;
        other.m_IndexBuffer = VK_NULL_HANDLE;
        other.m_IndexBufferMemory = VK_NULL_HANDLE;
        other.m_IndexCount = 0;
    }
    return *this;
}

VkVertexInputBindingDescription Mesh::Vertex::GetBindingDescription()
{
    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(Vertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return binding;
}

std::array<VkVertexInputAttributeDescription, 3> Mesh::Vertex::GetAttributeDescriptions()
{
    std::array<VkVertexInputAttributeDescription, 3> attributes{};

    // position
    attributes[0].binding = 0;
    attributes[0].location = 0;
    attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributes[0].offset = offsetof(Vertex, position);

    // normal
    attributes[1].binding = 0;
    attributes[1].location = 1;
    attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributes[1].offset = offsetof(Vertex, normal);

    // uv
    attributes[2].binding = 0;
    attributes[2].location = 2;
    attributes[2].format = VK_FORMAT_R32G32_SFLOAT;
    attributes[2].offset = offsetof(Vertex, uv);

    return attributes;
}

bool Mesh::Initialize(VulkanRHI* rhi, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
{
    if (!rhi)
        return false;

    m_RHI = rhi;
    m_Device = m_RHI->GetDevice();
    m_PhysicalDevice = m_RHI->GetPhysicalDevice();
    m_GraphicsQueue = m_RHI->GetGraphicsQueue();
    m_CommandPool = m_RHI->GetCommandPool();

    if (m_Device == VK_NULL_HANDLE || m_PhysicalDevice == VK_NULL_HANDLE)
        return false;

    if (!CreateVertexBuffer(vertices))
        return false;
	m_VertexCount = static_cast<uint32_t>(vertices.size());
    if (!indices.empty())
    {
        if (!CreateIndexBuffer(indices))
            return false;
        m_IndexCount = static_cast<uint32_t>(indices.size());
    }
    else
    {
        m_IndexCount = 0;
    }

    return true;
}

void Mesh::Destroy()
{
    if (m_Device == VK_NULL_HANDLE)
        return;

    if (m_IndexBuffer != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(m_Device, m_IndexBuffer, nullptr);
        m_IndexBuffer = VK_NULL_HANDLE;
    }
    if (m_IndexBufferMemory != VK_NULL_HANDLE)
    {
        vkFreeMemory(m_Device, m_IndexBufferMemory, nullptr);
        m_IndexBufferMemory = VK_NULL_HANDLE;
    }
    if (m_VertexBuffer != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(m_Device, m_VertexBuffer, nullptr);
        m_VertexBuffer = VK_NULL_HANDLE;
    }
    if (m_VertexBufferMemory != VK_NULL_HANDLE)
    {
        vkFreeMemory(m_Device, m_VertexBufferMemory, nullptr);
        m_VertexBufferMemory = VK_NULL_HANDLE;
    }

    m_Device = VK_NULL_HANDLE;
    m_PhysicalDevice = VK_NULL_HANDLE;
    m_GraphicsQueue = VK_NULL_HANDLE;
    m_CommandPool = VK_NULL_HANDLE;
    m_RHI = nullptr;
    m_IndexCount = 0;
}

void Mesh::Bind(VkCommandBuffer cmd) const
{
    assert(cmd != VK_NULL_HANDLE);
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(cmd, 0, 1, &m_VertexBuffer, offsets);
    if (m_IndexBuffer != VK_NULL_HANDLE)
    {
        vkCmdBindIndexBuffer(cmd, m_IndexBuffer, 0, VK_INDEX_TYPE_UINT32);
    }
}

void Mesh::Draw(VkCommandBuffer cmd) const
{
    if (m_IndexCount > 0 && m_IndexBuffer != VK_NULL_HANDLE)
    {
        vkCmdDrawIndexed(cmd, m_IndexCount, 1, 0, 0, 0);
    }
    else
    {
        vkCmdDraw(cmd, m_VertexCount, 1, 0, 0);
    }
}

bool Mesh::CreateVertexBuffer(const std::vector<Vertex>& vertices)
{
    if (vertices.empty())
        return false;

    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

    // staging buffer
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingBufferMemory = VK_NULL_HANDLE;
    if (!CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory))
        return false;

    void* data = nullptr;
    vkMapMemory(m_Device, stagingBufferMemory, 0, bufferSize, 0, &data);
    std::memcpy(data, vertices.data(), static_cast<size_t>(bufferSize));
    vkUnmapMemory(m_Device, stagingBufferMemory);

    if (!CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_VertexBuffer, m_VertexBufferMemory))
    {
        vkDestroyBuffer(m_Device, stagingBuffer, nullptr);
        vkFreeMemory(m_Device, stagingBufferMemory, nullptr);
        return false;
    }

    CopyBuffer(stagingBuffer, m_VertexBuffer, bufferSize);

    vkDestroyBuffer(m_Device, stagingBuffer, nullptr);
    vkFreeMemory(m_Device, stagingBufferMemory, nullptr);

    return true;
}

bool Mesh::CreateIndexBuffer(const std::vector<uint32_t>& indices)
{
    if (indices.empty())
        return false;

    VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingBufferMemory = VK_NULL_HANDLE;
    if (!CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory))
        return false;

    void* data = nullptr;
    vkMapMemory(m_Device, stagingBufferMemory, 0, bufferSize, 0, &data);
    std::memcpy(data, indices.data(), static_cast<size_t>(bufferSize));
    vkUnmapMemory(m_Device, stagingBufferMemory);

    if (!CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_IndexBuffer, m_IndexBufferMemory))
    {
        vkDestroyBuffer(m_Device, stagingBuffer, nullptr);
        vkFreeMemory(m_Device, stagingBufferMemory, nullptr);
        return false;
    }

    CopyBuffer(stagingBuffer, m_IndexBuffer, bufferSize);

    vkDestroyBuffer(m_Device, stagingBuffer, nullptr);
    vkFreeMemory(m_Device, stagingBufferMemory, nullptr);

    return true;
}

bool Mesh::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(m_Device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
        return false;

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(m_Device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(m_Device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS)
    {
        vkDestroyBuffer(m_Device, buffer, nullptr);
        return false;
    }

    vkBindBufferMemory(m_Device, buffer, bufferMemory, 0);
    return true;
}

void Mesh::CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
    VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    EndSingleTimeCommands(commandBuffer);
}

uint32_t Mesh::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(m_PhysicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i)
    {
        if ((typeFilter & (1u << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type!");
}

VkCommandBuffer Mesh::BeginSingleTimeCommands() const
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_CommandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(m_Device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void Mesh::EndSingleTimeCommands(VkCommandBuffer commandBuffer) const
{
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_GraphicsQueue);

    vkFreeCommandBuffers(m_Device, m_CommandPool, 1, &commandBuffer);
}