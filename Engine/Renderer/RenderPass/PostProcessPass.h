#pragma once

#include "IRenderPass.h"

class RenderGraphBuilder;
class IRHICommandBuffer;
class TextureHandle;

class PostProcessPass : public IRenderPass
{
public:
	void Setup(RenderGraphBuilder& builder) override;
	void Execute(IRHICommandBuffer& cmd) override;
	std::string GetName() const override;

	void SetInputTexture(TextureHandle intex);
	void SetOutputTexture(TextureHandle outtex);

};