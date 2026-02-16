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
	// Take ownership of the provided texture (shallow copy of GPU resources)
	m_Texture = std::make_unique<Texture>(texture);
	// Write the texture into RHI descriptor sets (binding = 1 in RHI layout)
	m_Texture->WriteToDescriptorSets(rhi);
	// remember RHI for later destroy
	m_RHI = rhi;
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

	// Bind pipeline first (shaders/state)
	if (m_Pipeline && m_Pipeline->IsValid())
		m_Pipeline->Bind(cmd);

	// Bind mesh and issue draw
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