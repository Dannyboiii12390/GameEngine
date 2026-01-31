	#include "ComponentGeometry.h"

ComponentGeometry::ComponentGeometry()
	: IComponent(EComponentType::Component_Geometry)
{
}

ComponentGeometry::ComponentGeometry(VulkanRHI* rhi, const std::vector<Mesh::Vertex>& vertices, const std::vector<uint32_t>& indices)
	: IComponent(EComponentType::Component_Geometry)
{
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

	if (!m_Pipeline)
		m_Pipeline = std::make_unique<Pipeline>();

	if (!m_Pipeline->Initialize(rhi, renderPass, extent, vertSpvPath, fragSpvPath))
	{
		m_Pipeline.reset();
		return false;
	}
	return true;
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
	if (m_Mesh)
	{
		m_Mesh->Destroy();
		m_Mesh.reset();
	}

	if (m_Pipeline)
	{
		m_Pipeline->Destroy();
		m_Pipeline.reset();
	}
}

bool ComponentGeometry::IsValid() const
{
	return (m_Mesh && m_Mesh->IsValid()) && (m_Pipeline && m_Pipeline->IsValid());
}