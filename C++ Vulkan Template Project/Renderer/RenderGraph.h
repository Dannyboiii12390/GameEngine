#pragma once

class RenderPass;
class IRHI;

class RenderGraph
{
public:
	void AddPass(RenderPass* pass);
	void Compile();
	void Execute(IRHI& rhi);
	void Reset();
};
