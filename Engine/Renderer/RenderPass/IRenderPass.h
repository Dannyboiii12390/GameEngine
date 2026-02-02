#pragma once

#include <string>

class RenderGraphBuilder;
class RHICommandBuffer;

class IRenderPass 
{
public:
	virtual void Setup(RenderGraphBuilder& builder) = 0;
	virtual void Execute(RHICommandBuffer& cmd) = 0;
	virtual std::string GetName() const = 0;

};

/*
Common ordering and sync
•	Typical order: GBuffer -> ShadowPass -> DeferredLighting -> PostProcess -> Present.
•	Ensure correct image layout transitions and barriers (depth as sampled vs depth attachment) and descriptor updates between passes.
•	Use debug views to render each G‑buffer / shadow map to screen when diagnosing.
*/