#pragma once

#include "IRenderPass.h"

class RenderGraphBuilder;
class IRHICommandBuffer;
class TextureHandle;

class PostProcessPass : public IRenderPass
{
public:
	void Setup(RenderGraphBuilder& builder) override;
	void Execute(RHICommandBuffer& cmd) override;
	std::string GetName() const override;

	void SetInputTexture(TextureHandle intex);
	void SetOutputTexture(TextureHandle outtex);

};
/*
PostProcessPass
•	Purpose: Apply post‑processing effects and convert HDR result to final LDR image for presentation.
•	Typical steps: bloom (extract + blur + composite), tone mapping, color grading (LUT), FXAA/TAA/temporal filters, film grain.
•	Inputs: lit color buffer, depth (for some effects), motion vectors (for TAA).
•	Outputs: final swapchain image (or intermediate that gets presented).
•	GPU work: chain of fullscreen passes (ping‑pong framebuffers), separable blurs for performance.
•	Notes: order matters (e.g., bloom before tone map), keep HDR precision until tone mapping.
*/