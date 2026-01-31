#pragma once

#include "IRenderPass.h"

class LightList;
class CameraProxy;

class GBufferPass : public IRenderPass
{
public:
	void Setup(RenderGraphBuilder& builder) override;
	void Execute(IRHICommandBuffer& cmd) override;
	std::string GetName() const override;

	void SetLights(const LightList& lights);
	void SetCamera(const CameraProxy& camera);
};
