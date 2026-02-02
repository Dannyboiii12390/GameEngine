#include "SystemRenderer.h"

#include "../../Renderer/VulkanRHI.h"
#include "../Entity.h"
#include "../Components/ComponentGeometry.h"
#include "../Components/ComponentTranslation.h"

#include <glm/gtc/type_ptr.hpp>
#include <Windows.h> // for OutputDebugStringA

SystemRenderer::~SystemRenderer()
{
	Shutdown();
}

void SystemRenderer::Initialize(VulkanRHI* rhi)
{
	m_RHI = rhi;
}

void SystemRenderer::Shutdown()
{
	m_RHI = nullptr;
}

void SystemRenderer::Render(VkCommandBuffer cmd, const std::vector<Entity*>& entities)
{
	if (cmd == VK_NULL_HANDLE)
		return;

	OutputDebugStringA("SystemRenderer::Render - entry\n");

	// If swapchain extent is available, set viewport/scissor here in case pipeline uses dynamic state.
	if (m_RHI)
	{
		VkExtent2D extent = m_RHI->GetSwapchainExtent();
		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(extent.width);
		viewport.height = static_cast<float>(extent.height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(cmd, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = extent;
		vkCmdSetScissor(cmd, 0, 1, &scissor);
	}

	for (Entity* e : entities)
	{
		auto* geom = e->GetComponent<ComponentGeometry>(EComponentType::Component_Geometry);
		if (!geom || !geom->IsValid())
		{
			OutputDebugStringA("SystemRenderer: skipping entity - no valid ComponentGeometry\n");
			continue;
		}

		Pipeline* pipeline = geom->GetPipeline();
		Mesh* mesh = geom->GetMesh();

		// Fallback to simple bind/draw if pipeline or mesh missing
		if (!pipeline || !mesh)
		{
			OutputDebugStringA("SystemRenderer: pipeline or mesh missing, calling BindAndDraw fallback\n");
			geom->BindAndDraw(cmd);
			continue;
		}

		// Quick check: does pipeline use the same renderpass the RHI exposes?
		if (m_RHI && pipeline->GetRenderPass() != m_RHI->GetRenderPass())
		{
			OutputDebugStringA("SystemRenderer: WARNING - pipeline renderPass != RHI renderPass\n");
		}

		// Bind pipeline
		pipeline->Bind(cmd);
		OutputDebugStringA("SystemRenderer: pipeline bound\n");

		// Bind descriptor sets (camera UBO etc.) if RHI provides them.
		if (m_RHI)
		{
			const auto& sets = m_RHI->GetDescriptorSets();
			if (!sets.empty())
			{
				// bind only the first set (set 0) which contains the camera UBO in this implementation
				vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetLayout(), 0, 1, &sets[0], 0, nullptr);
			}
		}

		// Always push a model matrix because the vertex shader expects a push-constant mat4.
		glm::mat4 transform = glm::mat4(1.0f);
		if (e->HasComponent(EComponentType::Component_Translation))
		{
			auto* xf = e->GetComponent<ComponentTranslation>(EComponentType::Component_Translation);
			if (xf)
			{
				transform = xf->GetTransformMatrix();
			}
		}

		pipeline->PushConstants(cmd, VK_SHADER_STAGE_VERTEX_BIT, 0, static_cast<uint32_t>(sizeof(transform)), glm::value_ptr(transform));

		// Bind mesh and draw
		mesh->Bind(cmd);
		OutputDebugStringA("SystemRenderer: mesh bound\n");
		mesh->Draw(cmd);
		OutputDebugStringA("SystemRenderer: mesh draw issued\n");
	}
}