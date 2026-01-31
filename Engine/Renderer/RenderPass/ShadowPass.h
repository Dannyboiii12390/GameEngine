#pragma once
#include "IRenderPass.h"
class LightProxy;

class ShadowPass : public IRenderPass
{
public:
	void Setup(RenderGraphBuilder& builder) override;
	void Execute(IRHICommandBuffer& cmd) override;
	std::string GetName() const override;

	void SetLight(const LightProxy& light);
	void SetShadowMapSize(int size);
}
