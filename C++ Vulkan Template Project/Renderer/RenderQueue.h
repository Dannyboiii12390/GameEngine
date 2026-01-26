#pragma once

class MeshRenderProxy;

class RenderQueue {
public:
	virtual void AddMesh(const MeshRenderProxy& mesh) = 0; 
	virtual void Clear() = 0;
};