// PSEUDOCODE / PLAN (detailed):
// 1) Define a small POD struct that matches the GPU-side per-entity storage layout.
//    - Must be std-layout and aligned for GPU (use glm::vec4 fields for 16-byte alignment).
//    - Contains position, velocity, and collider metadata fields (packed into vec4s).
// 2) In the SystemCollision constructor:
//    - Store the requested numEntities in a member variable for later use by OnUpdate.
//    - Load the compute shader (existing call left intact).
//    - Prepare an optional push-constant that informs the compute pipeline of the active entity count.
//      - Call ComputeShader::PushConstants(&count, sizeof(count)) BEFORE CreateBuffers() so pipeline layout may include push constant size.
//    - Calculate buffer sizes required for:
//        a) read buffer (current frame data for all entities) -> numEntities * sizeof(ShaderEntityGPU)
//        b) write buffer (output positions/velocities for all entities) -> numEntities * sizeof(ShaderEntityGPU)
//      - Use a std::vector<VkDeviceSize> and pass it to ComputeShader::CreateBuffers(sizes).
//    - Save per-entity size and counts as private members so OnUpdate (implemented elsewhere) can use them.
//
// NOTE: This header only creates and records resources required by OnUpdate.
//       Actual data uploads, dispatches, and readbacks are expected in the OnUpdate implementation elsewhere.

#pragma once

#include "ISystem.h"
#include "../../Renderer/ComputeShader.h"
#include <glm/ext/vector_float4.hpp>
#include <vector>
#include <vulkan/vulkan.h>
#include <cstdint>

 // Forward declaration
class VulkanRHI;


class SystemCollision : public ISystem
{
public:
	// GPU-side layout for a single entity used by the compute shader.
	// Matches what the collision.compute shader will read/write.
	struct ShaderEntityGPU
	{
		glm::vec4 position; // xyz = position, w = padding
		glm::vec4 velocity; // xyz = velocity, w = padding
		glm::vec4 collider; // x = colliderType (as float/int bitcast), y = radius, z = halfExtentX, w = halfExtentY (or extra)
	};

	SystemCollision(uint32_t numEntities, VulkanRHI* rhi = nullptr) : m_RHI(rhi), m_ComputeShader(rhi), m_NumEntities(numEntities)
	{
		m_ComputeShader.LoadShader("SHADERS/collision.comp.spv");
		// compute shader should detect collisions and work out new positions/velocities based on current positions/velocities and collider data. 
		// see CollisionResponse() in ComponentCollision.cpp for CPU-based collision response logic that the shader should be based on.
		// should be a matrix for each entity containing position, velocity, and collider info (type, radius/half-extents, etc). 
		// The shader will read the current frame's data from this matrix, perform collision detection and response, and write the new positions/velocities into a separate output matrix that the CPU will read back and apply to the entities' components.
		
		// Prepare push-constant with entity count (optional; pipeline layout may use it)
		uint32_t entityCount = m_NumEntities;
		// PushConstants() is optional; if present it informs the pipeline layout about push-constant size prior to CreateBuffers()
		// This call is intentionally placed before CreateBuffers() per ComputeShader usage pattern.
		// If ComputeShader doesn't implement PushConstants(), this call will be a no-op at compile-time only if overloaded/guarded there.
		// If your ComputeShader requires a different mechanism, adjust accordingly in that implementation.
		// 
		// Prepare push-constant with entity count + dt (pipeline layout needs size up-front)
		struct PushConsts { uint32_t entityCount; float deltaTime; };
		PushConsts pc{ m_NumEntities, 0.0f };
		m_ComputeShader.PushConstants(&pc, static_cast<uint32_t>(sizeof(pc)));

		// Create per-entity GPU buffers: read (input) and write (output)
		const VkDeviceSize entitySize = static_cast<VkDeviceSize>(sizeof(ShaderEntityGPU));
		const VkDeviceSize totalSize = entitySize * static_cast<VkDeviceSize>(m_NumEntities);

		// Two buffers: [0] read/input, [1] write/output
		std::vector<VkDeviceSize> bufferSizes;
		bufferSizes.reserve(2);
		bufferSizes.push_back(totalSize); // input buffer size
		bufferSizes.push_back(totalSize); // output buffer size

		m_EntitySize = entitySize;
		m_ComputeShader.CreateBuffers(bufferSizes);
	}
	SystemCollision() = default;
	~SystemCollision()
	{
		m_ComputeShader.Destroy();
	}

	// Inherited via ISystem
	void OnUpdate(std::span<Entity> entities, float deltaTime) override;

private:
	VulkanRHI* m_RHI = nullptr;
	ComputeShader m_ComputeShader;

	// Stored for use by OnUpdate (implemented elsewhere)
	uint32_t m_NumEntities = 0;
	VkDeviceSize m_EntitySize = 0;
};
