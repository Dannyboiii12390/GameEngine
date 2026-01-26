#pragma once

class RenderMeshProxy
{
public:
	virtual void GetVertexBuffer() = 0;
	virtual void GetIndexBuffer() = 0;
	virtual void GetMaterial() = 0;
	virtual void GetTransform() = 0;
};