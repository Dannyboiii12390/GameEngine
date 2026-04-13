#include "SystemFlocking.h"

#include "../Entity.h"
#include "../Components/ComponentTransform.h"
#include "../Components/ComponentPhysics.h"
#include "../Components/ComponentNetwork.h"
#include "../../DebugUtils.h"

#include <glm/glm.hpp>
#include <algorithm>
#include <stdexcept>

namespace
{
	constexpr uint32_t INVALID_INDEX = 0xFFFFFFFFu;

	static uint32_t FlattenCell(uint32_t x, uint32_t y, uint32_t z, uint32_t dimX, uint32_t dimY)
	{
		return x + y * dimX + z * dimX * dimY;
	}

	static glm::uvec3 ComputeGridDims(const glm::vec3& minPos, const glm::vec3& maxPos, float cellSize, uint32_t maxDim)
	{
		const glm::vec3 extent = glm::max(maxPos - minPos, glm::vec3(0.001f));
		glm::uvec3 dims = glm::uvec3(glm::floor(extent / std::max(cellSize, 0.001f))) + glm::uvec3(1u);
		dims = glm::clamp(dims, glm::uvec3(1u), glm::uvec3(maxDim));
		return dims;
	}

	static glm::uvec3 PositionToCell(const glm::vec3& position, const glm::vec3& gridMin, float cellSize, const glm::uvec3& dims)
	{
		const glm::vec3 rel = (position - gridMin) / std::max(cellSize, 0.001f);
		glm::ivec3 c = glm::ivec3(glm::floor(rel));
		c = glm::clamp(c, glm::ivec3(0), glm::ivec3(dims) - glm::ivec3(1));
		return glm::uvec3(c);
	}
}

SystemFlocking::SystemFlocking(VulkanRHI* rhi, std::atomic<PeerID>* localPeerId)
	: m_rhi(rhi), m_localPeerId(localPeerId)
{
	m_SystemType = ESystemType::System_Physics;
}

SystemFlocking::~SystemFlocking()
{
	if (m_compute)
	{
		m_compute->Destroy();
		m_compute.reset();
	}
}

void SystemFlocking::EnsureComputeReady(const FlockingPushConstants& params, uint32_t boidCount, uint32_t totalCells)
{
	if (!m_compute)
	{
		m_compute = std::make_unique<ComputeShader>(m_rhi);
		USE_SPATIAL_HASH ? m_compute->LoadShader("SHADERS/flockingSpatial.comp.spv") : m_compute->LoadShader("SHADERS/flocking.comp.spv");

		m_compute->PushConstants(&params, static_cast<uint32_t>(sizeof(FlockingPushConstants)));
	}

	const VkDeviceSize positionsSize = static_cast<VkDeviceSize>(boidCount) * sizeof(glm::vec4);
	const VkDeviceSize forcesSize = static_cast<VkDeviceSize>(boidCount) * sizeof(glm::vec4);
	const VkDeviceSize sortedSize = static_cast<VkDeviceSize>(boidCount) * sizeof(uint32_t);
	const VkDeviceSize cellStartSize = static_cast<VkDeviceSize>(totalCells) * sizeof(uint32_t);
	const VkDeviceSize cellCountSize = static_cast<VkDeviceSize>(totalCells) * sizeof(uint32_t);

	const std::vector<VkDeviceSize> sizes = { positionsSize, forcesSize, sortedSize, cellStartSize, cellCountSize };

	if (sizes != m_bufferSizes)
	{
		m_compute->CreateBuffers(sizes);
		m_bufferSizes = sizes;
	}
}

void SystemFlocking::OnUpdate(std::span<Entity> entities, float)
{
	if (m_disabled || !m_rhi)
		return;

	std::vector<uint32_t> boidEntityIndices;
	std::vector<glm::vec4> positions;

	boidEntityIndices.reserve(entities.size());
	positions.reserve(entities.size());

	for (uint32_t i = 0; i < static_cast<uint32_t>(entities.size()); ++i)
	{
		Entity& entity = entities[i];
		auto* transform = entity.GetComponent<ComponentTransform>(EComponentType::Component_Transform);
		if (!transform)
			continue;

		positions.emplace_back(transform->Position(), 0.0f);
		boidEntityIndices.push_back(i);
	}

	const uint32_t boidCount = static_cast<uint32_t>(positions.size());
	if (boidCount == 0)
		return;

	glm::vec3 minPos = glm::vec3(positions[0]);
	glm::vec3 maxPos = glm::vec3(positions[0]);
	for (const glm::vec4& p : positions)
	{
		minPos = glm::min(minPos, glm::vec3(p));
		maxPos = glm::max(maxPos, glm::vec3(p));
	}

	const float cellSize = std::max(m_neighborRadius, 0.001f);
	const glm::uvec3 dims = ComputeGridDims(minPos, maxPos, cellSize, m_maxGridDim);
	const uint32_t totalCells = dims.x * dims.y * dims.z;

	std::vector<uint32_t> cellIds(boidCount, 0u);
	std::vector<uint32_t> cellCount(totalCells, 0u);
	std::vector<uint32_t> cellStart(totalCells, INVALID_INDEX);
	std::vector<uint32_t> sortedIndices(boidCount, 0u);

	for (uint32_t i = 0; i < boidCount; ++i)
	{
		const glm::uvec3 cell = PositionToCell(glm::vec3(positions[i]), minPos, cellSize, dims);
		const uint32_t flat = FlattenCell(cell.x, cell.y, cell.z, dims.x, dims.y);
		cellIds[i] = flat;
		cellCount[flat]++;
	}

	uint32_t runningOffset = 0u;
	for (uint32_t c = 0; c < totalCells; ++c)
	{
		if (cellCount[c] == 0u)
			continue;

		cellStart[c] = runningOffset;
		runningOffset += cellCount[c];
	}

	std::vector<uint32_t> writeOffsets = cellStart;
	for (uint32_t i = 0; i < boidCount; ++i)
	{
		const uint32_t cell = cellIds[i];
		const uint32_t dst = writeOffsets[cell]++;
		sortedIndices[dst] = i;
	}

	FlockingPushConstants params{};
	params.entityCount = boidCount;
	params.gridDimX = dims.x;
	params.gridDimY = dims.y;
	params.gridDimZ = dims.z;
	params.gridMin[0] = minPos.x;
	params.gridMin[1] = minPos.y;
	params.gridMin[2] = minPos.z;
	params.cellSize = cellSize;
	params.neighborRadius = m_neighborRadius;
	params.separationRadius = m_separationRadius;
	params.maxForce = m_maxForce;
	params.cohesionWeight = m_cohesionWeight;
	params.separationWeight = m_separationWeight;
	params.useSpatialHash = m_useSpatialHash;

	EnsureComputeReady(params, boidCount, totalCells);

	m_compute->PushConstants(&params, static_cast<uint32_t>(sizeof(FlockingPushConstants)));
	m_compute->Upload(0, positions.data(), static_cast<VkDeviceSize>(positions.size() * sizeof(glm::vec4)));

	std::vector<glm::vec4> forces(boidCount, glm::vec4(0.0f));
	m_compute->Upload(1, forces.data(), static_cast<VkDeviceSize>(forces.size() * sizeof(glm::vec4)));
	m_compute->Upload(2, sortedIndices.data(), static_cast<VkDeviceSize>(sortedIndices.size() * sizeof(uint32_t)));
	m_compute->Upload(3, cellStart.data(), static_cast<VkDeviceSize>(cellStart.size() * sizeof(uint32_t)));
	m_compute->Upload(4, cellCount.data(), static_cast<VkDeviceSize>(cellCount.size() * sizeof(uint32_t)));

	m_compute->Dispatch(boidCount, 256);
	m_compute->Readback(1, forces.data(), static_cast<VkDeviceSize>(forces.size() * sizeof(glm::vec4)));

	const PeerID localPeerId = m_localPeerId ? m_localPeerId->load(std::memory_order_relaxed) : 0;

	for (uint32_t boid = 0; boid < boidCount; ++boid)
	{
		Entity& entity = entities[boidEntityIndices[boid]];

		auto* netComp = entity.GetComponent<ComponentNetwork>(EComponentType::Component_Network);
		if (netComp && netComp->IsSimulated() && !netComp->IsOwnedByMe(localPeerId))
			continue;

		auto* physics = entity.GetComponent<ComponentPhysics>(EComponentType::Component_Physics);
		if (!physics)
			continue;

		const glm::vec3 force = glm::vec3(forces[boid]) * m_forceScale;
		physics->ApplyForce(force);
	}
}