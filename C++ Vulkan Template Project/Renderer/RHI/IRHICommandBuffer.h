#pragma once

#include <span>

class RenderPassDesc;
class PipelineHandle;
class DescriptorSetHandle;
class BufferHandle;

class IRHICommandBuffer
{
public:
	virtual void Begin() = 0;
	virtual void End() = 0;
	virtual void BeginRenderPass(const RenderPassDesc& desc) = 0;
	virtual void EndRenderPass() = 0;
	virtual void BindPipeline(const PipelineHandle& pipeline) = 0;
	virtual void BindDescriptorSet(const DescriptorSetHandle& descriptorSet) = 0;
	virtual void BindVertexBuffer(const BufferHandle& buffer) = 0;
	virtual void BindIndexBuffer(const BufferHandle& buffer) = 0;
	virtual void Draw(std::span<uint32_t> vertices) = 0;
	virtual void DrawIndexed(std::span<uint32_t> indices) = 0;
};
