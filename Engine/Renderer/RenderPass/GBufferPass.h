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
/*
GBufferPass
•	Purpose: Geometry (albedo/material) pass that rasterizes all opaque geometry and writes per‑pixel material attributes into multiple render targets (G‑buffer).
•	Typical outputs: albedo/color, world or view normal, material properties (roughness/metalness/specular), material ID, and depth.
•	Inputs: meshes, per‑object transforms, camera matrices, material data, per‑frame UBOs.
•	GPU work: bind geometry pipeline (MRT), set camera descriptor sets, push model matrix, draw meshes.
•	Shaders: VS outputs position/normal/tangent; FS outputs to MRTs.
•	Notes: produce a depth buffer for later use (occlusion, linearize depth); useful to debug by visualizing buffers individually.
*/
