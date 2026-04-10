#pragma once

#include "ISystem.h"
#include "../../Renderer/ComputeShader.h"
#include "../NetworkTypes.h"

#include <glm/vec4.hpp>
#include <memory>
#include <vector>
#include <cstdint>
#include <atomic>

class VulkanRHI;
class Entity;


bool constexpr USE_SPATIAL_HASH = false; // Toggle to switch between naive O(N^2) and spatial-hash-accelerated compute shader variants (requires different shader and push-constant layout)
struct alignas(16) FlockingPushConstants
{
	uint32_t entityCount;
	uint32_t gridDimX;
	uint32_t gridDimY;
	uint32_t gridDimZ;
	float gridMin[3];
	float cellSize;
	float neighborRadius;
	float separationRadius;
	float maxForce;
	float cohesionWeight;
	float separationWeight;
	uint32_t useSpatialHash;
	uint32_t pad0;
	uint32_t pad1;
};

class SystemFlocking final : public ISystem
{
public:
	SystemFlocking(VulkanRHI* rhi, std::atomic<PeerID>* localPeerId);
	~SystemFlocking() override;

	void OnUpdate(std::span<Entity> entities, float deltaTime) override;

private:
	void EnsureComputeReady(const FlockingPushConstants& params, uint32_t boidCount, uint32_t totalCells);

private:
	VulkanRHI* m_rhi = nullptr;
	std::atomic<PeerID>* m_localPeerId = nullptr;
	std::unique_ptr<ComputeShader> m_compute;
	bool m_disabled = false;
	std::vector<VkDeviceSize> m_bufferSizes;

	float m_neighborRadius = 8.0f;
	float m_separationRadius = 3.0f;
	float m_maxForce = 25.0f;
	float m_cohesionWeight = 1.0f;
	float m_separationWeight = 1.5f;
	float m_forceScale = 30.0f;
	uint32_t m_useSpatialHash = 1u;
	uint32_t m_maxGridDim = 64u;
};