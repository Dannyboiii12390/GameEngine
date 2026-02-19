	#include "ComponentGeometry.h"

ComponentGeometry::ComponentGeometry()
	: IComponent(EComponentType::Component_Geometry)
{
}
ComponentGeometry::ComponentGeometry(VulkanRHI* rhi, const std::vector<Mesh::Vertex>& vertices, const std::vector<uint32_t>& indices)
	: IComponent(EComponentType::Component_Geometry)
{
	m_RHI = rhi;
	// Attempt to initialize mesh immediately. Failure is non-fatal here;
	// caller can check IsValid() and handle errors (or call InitializeMesh again).
	InitializeMesh(rhi, vertices, indices);
}
ComponentGeometry::~ComponentGeometry()
{
	Destroy();
}
bool ComponentGeometry::InitializeMesh(VulkanRHI* rhi, const std::vector<Mesh::Vertex>& vertices, const std::vector<uint32_t>& indices)
{
	if (!rhi)
		return false;

	// Lazily create mesh
	if (!m_Mesh)
		m_Mesh = std::make_unique<Mesh>();

	// Mesh::Initialize takes VulkanRHI*, vertices, indices
	if (!m_Mesh->Initialize(rhi, vertices, indices))
	{
		m_Mesh.reset();
		return false;
	}
	return true;
}
bool ComponentGeometry::InitializePipeline(VulkanRHI* rhi, VkRenderPass renderPass, VkExtent2D extent, const std::string& vertSpvPath, const std::string& fragSpvPath)
{
	if (!rhi)
		return false;

	m_RHI = rhi;

	if (!m_Pipeline)
		m_Pipeline = std::make_unique<Pipeline>();

	if (!m_Pipeline->Initialize(rhi, renderPass, extent, vertSpvPath, fragSpvPath))
	{
		m_Pipeline.reset();
		return false;
	}
	return true;
}
bool ComponentGeometry::CreateTexture(VulkanRHI* rhi, const std::string& path, TextureType type, bool srgb)
{
	if (!rhi) return false;

	// Load texture from file
	m_Texture = std::make_unique<Texture>(rhi, path, type, srgb);

	// Write the texture into RHI descriptor sets (binding = 1 in RHI layout)
	m_Texture->WriteToDescriptorSets(rhi);

	// remember RHI for later destroy
	m_RHI = rhi;
	return true;
}
bool ComponentGeometry::AddTexture(VulkanRHI* rhi, const Texture& texture)
{
	if (!rhi) return false;

	m_Texture = std::make_unique<Texture>(texture);
	m_RHI = rhi;

	// Allocate a dedicated descriptor set for this entity.
	m_DescriptorSet = rhi->AllocateTextureDescriptorSet();
	if (m_DescriptorSet == VK_NULL_HANDLE)
		return false;

	VkDevice device = rhi->GetDevice();

	// --- Binding 0: camera UBO ---
	// Copy the UBO buffer info from the first global descriptor set so this
	// per-entity set has a valid camera binding at set=0, binding=0.
	const auto& globalSets = rhi->GetDescriptorSets();
	if (!globalSets.empty())
	{
		// Re-point binding 0 to the same camera UBO region as the first global set.
		// We use a VkCopyDescriptorSet to copy binding 0 from the global set rather
		// than re-specifying the buffer, avoiding any dependency on the UBO internals here.
		VkCopyDescriptorSet uboKopy{};
		uboKopy.sType = VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET;
		uboKopy.srcSet = globalSets[0];
		uboKopy.srcBinding = 0;
		uboKopy.srcArrayElement = 0;
		uboKopy.dstSet = m_DescriptorSet;
		uboKopy.dstBinding = 0;
		uboKopy.dstArrayElement = 0;
		uboKopy.descriptorCount = 1;

		vkUpdateDescriptorSets(device, 0, nullptr, 1, &uboKopy);
	}

	// --- Binding 1: texture sampler ---
	VkDescriptorImageInfo imgInfo{};
	imgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imgInfo.imageView = m_Texture->GetImageView();
	imgInfo.sampler = m_Texture->GetSampler();

	VkWriteDescriptorSet texWrite{};
	texWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	texWrite.dstSet = m_DescriptorSet;
	texWrite.dstBinding = 1;
	texWrite.dstArrayElement = 0;
	texWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	texWrite.descriptorCount = 1;
	texWrite.pImageInfo = &imgInfo;

	vkUpdateDescriptorSets(device, 1, &texWrite, 0, nullptr);

	return true;
}
Texture ComponentGeometry::GetTexture() const
{
	auto* texPtr = m_Texture.get();
	return Texture(*texPtr);
}
void ComponentGeometry::BindAndDraw(VkCommandBuffer cmd) const
{
	if (!cmd)
		return;

	// Bind pipeline and inject this entity's own descriptor set (texture binding).
	if (m_Pipeline && m_Pipeline->IsValid())
		m_Pipeline->Bind(cmd, m_DescriptorSet);

	if (m_Mesh && m_Mesh->IsValid())
	{
		m_Mesh->Bind(cmd);
		m_Mesh->Draw(cmd);
	}
}
void ComponentGeometry::Destroy()
{
	// Only destroy GPU resources while RHI/device is still valid
	if (m_RHI)
	{
		// Destroy owned texture (image, view, sampler, memory)
		if (m_Texture)
		{
			m_Texture->Destroy(m_RHI);
			// Release ownership so we don't hold stale pointers after destruction
			m_Texture.reset();
		}

		// Destroy mesh GPU buffers
		if (m_Mesh && m_Mesh->IsValid())
		{
			m_Mesh->Destroy();
			m_Mesh.reset();
		}

		// Destroy pipeline and shader modules
		if (m_Pipeline && m_Pipeline->IsValid())
		{
			m_Pipeline->Destroy();
			m_Pipeline.reset();
		}

		// If you allocated descriptor sets or pools in this component, free them here too.
		// Example: vkFreeDescriptorSets(...); vkDestroyDescriptorPool(...);

		m_RHI = nullptr;
	}
}
bool ComponentGeometry::IsValid() const
{
	return (m_Mesh && m_Mesh->IsValid()) && (m_Pipeline && m_Pipeline->IsValid());
}