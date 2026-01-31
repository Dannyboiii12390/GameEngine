#pragma once

#include <string>

class RenderGraphBuilder;
class IRHICommandBuffer;

class IRenderPass 
{
public:
	virtual void Setup(RenderGraphBuilder& builder) = 0;
	virtual void Execute(IRHICommandBuffer& cmd) = 0;
	virtual std::string GetName() const = 0;

};