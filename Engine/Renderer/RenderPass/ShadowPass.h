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

/*
ShadowPass
•	Purpose: Render scene from a light’s point of view into a depth map (shadow map) used to test occlusion when shading.
•	Typical outputs: one or more depth textures (cascade maps for directional lights).
•	Inputs: light transform/projection, meshes.
•	GPU work: depth‑only rendering (no color), set viewport to shadow map size, use depth bias/polygon offset to reduce acne.
•	Shaders: simple vertex shader writing depth; fragment shader often omitted.
•	Notes: for directional lights use cascaded shadow maps (CSM); sample shadow maps in lighting pass with PCF or variance/ESM for softer shadows.
*/