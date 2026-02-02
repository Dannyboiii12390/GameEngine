#pragma once

#include "IComponent.h"
#include "../../Renderer/VulkanRHI.h"
#include "../../Renderer/Mesh.h"
#include "../../Renderer/Pipeline.h"
#include "../../Renderer/Texture.h"

#include <memory>
#include <string>
#include <vector>

class ComponentGeometry : public IComponent
{
public:
	ComponentGeometry();
	// Added constructor declaration so callers that forward args via Entity::AddComponent(...) compile.
	ComponentGeometry(VulkanRHI* rhi, const std::vector<Mesh::Vertex>& vertices, const std::vector<uint32_t>& indices);
	~ComponentGeometry();

	// non-copyable
	ComponentGeometry(const ComponentGeometry&) = delete;
	ComponentGeometry& operator=(const ComponentGeometry&) = delete;

	// movable
	ComponentGeometry(ComponentGeometry&&) noexcept = default;
	ComponentGeometry& operator=(ComponentGeometry&&) noexcept = default;

	// Initialize mesh GPU resources. Returns false on failure.
	bool InitializeMesh(VulkanRHI* rhi, const std::vector<Mesh::Vertex>& vertices, const std::vector<uint32_t>& indices);

	// Initialize pipeline resources. Returns false on failure.
	bool InitializePipeline(VulkanRHI* rhi, VkRenderPass renderPass, VkExtent2D extent, const std::string& vertSpvPath, const std::string& fragSpvPath);

	bool CreateTexture(VulkanRHI* rhi, const std::string& path, TextureType type, bool srgb);

	// Bind pipeline and mesh and issue draw commands on the provided command buffer.
	void BindAndDraw(VkCommandBuffer cmd) const;

	// Destroy owned GPU resources.
	void Destroy();

	// Accessors
	const Mesh* GetMesh() const { return m_Mesh.get(); }
	Mesh* GetMesh() { return m_Mesh.get(); }

	const Pipeline* GetPipeline() const { return m_Pipeline.get(); }
	Pipeline* GetPipeline() { return m_Pipeline.get(); }

	bool IsValid() const;

private:
	std::unique_ptr<Mesh> m_Mesh;
	std::unique_ptr<Pipeline> m_Pipeline;
	std::unique_ptr<Texture> m_Texture;

	VulkanRHI* m_RHI = nullptr;
};