#pragma once
#include <memory>
#include <vector>
#include "../Renderer/Mesh.h"

typedef std::tuple<std::vector<Mesh::Vertex>, std::vector<uint32_t>> MeshData;

class ResourceManager
{

public: 
	static MeshData CreateCubeMesh();
	static MeshData Create2dTriangleMesh();
	static MeshData CreateQuadMesh();

	// Required for CWRK
	static MeshData CreateSphereMesh(uint32_t sectorCount = 36, uint32_t stackCount = 18);
	static MeshData CreatePlaneMesh(float width = 1.0f, float height = 1.0f, uint32_t widthSegments = 1, uint32_t heightSegments = 1);
	static MeshData CreateCylinderMesh(float radius = 0.5f, float height = 1.0f, uint32_t sectorCount = 36);
	static MeshData CreateCapsuleMesh(float radius = 0.5f, float height = 1.0f, uint32_t sectorCount = 36, uint32_t stackCount = 18);
	

};
