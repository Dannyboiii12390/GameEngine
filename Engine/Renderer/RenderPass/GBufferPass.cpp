#pragma once
#include "GBufferPass.h"
#include "../Camera.h"

void GBufferPass::Setup(RenderGraphBuilder& builder)
{
	// Intentionally minimal: avoid referencing RenderGraphBuilder internals here so this compiles
	// Expand this function to:
	// - declare G-buffer color attachments (albedo, normal, material, etc.)
	// - declare a depth attachment
	// - set load/store ops, formats and final layouts
	// - expose the produced textures for later passes (lighting, postprocess)
	(void)builder;
}

void GBufferPass::Execute(RHICommandBuffer& cmd)
{
	// Minimal safe implementation: no-op if no input set.
	// Replace the body with actual command recording:
	// - bind G-buffer pipeline (MRT shader)
	// - bind camera descriptor set(s)
	// - iterate the RenderQueue and issue draw calls (push model matrix per object)
	// - ensure proper image layout transitions or let RenderGraph handle them
	(void)cmd;

	if (!m_RenderQueue || !m_Camera)
		return;

	// Example pseudocode to implement later:
	// for (const Renderable& r : *m_RenderQueue) {
	//     Pipeline* pipeline = r.GetPipeline();
	//     Mesh* mesh = r.GetMesh();
	//     pipeline->Bind(cmd);
	//     vkCmdBindDescriptorSets(... camera UBO ...);
	//     pipeline->PushConstants(... model matrix ...);
	//     mesh->Bind(cmd);
	//     mesh->Draw(cmd);
	// }
}

std::string GBufferPass::GetName() const
{
	return "GBufferPass";
}

void GBufferPass::SetRenderables(const RenderQueue& queue)
{
	m_RenderQueue = &queue;
}

void GBufferPass::SetCamera(const Camera& camera)
{
	m_Camera = &camera;
}