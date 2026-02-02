#pragma once

#include "IRenderPass.h"

class LightList;
class CameraProxy;

class DeferredLightingPass : public IRenderPass
{
public:
	void Setup(RenderGraphBuilder& builder) override;
	void Execute(RHICommandBuffer& cmd) override;
	std::string GetName() const override;

	void SetLights(const LightList& lights);
	void SetCamera(const CameraProxy& camera);
};
/*
DeferredLightingPass (Deferred / Lighting)
•	Purpose: Read the G‑buffer and shadow maps and compute final lighting per pixel (PBR/Blinn‑Phong/etc.). Usually a fullscreen quad or compute pass.
•	Typical outputs: lit HDR color buffer (or an intermediate light accumulation target).
•	Inputs: G‑buffer textures, shadow maps, light list, camera, environment maps (IBL).
•	GPU work: fullscreen fragment/compute that reconstructs position from depth, computes BRDF, accumulates light contributions; apply shadows.
•	Shaders: lighting shader implementing chosen lighting model; can implement tiled/clustered or compute‑based light culling for scalability.
•	Notes: expensive if iterating many lights per pixel — use light culling (tiled/clustered) or forward+ for translucent/special cases.
*/
