#pragma once

#include "IRenderPass.h"
class RenderQueue;
class CameraProxy;

class GBufferPass : public IRenderPass
{
public:
	void Setup(RenderGraphBuilder& builder) override;
	void Execute(IRHICommandBuffer& cmd) override;
	std::string GetName() const override;

	void SetRenderables(const RenderQueue& queue);
	void SetCamera(const CameraProxy& camera);
};
