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

	SystemCollision(uint32_t numEntities, VulkanRHI* rhi = nullptr) : m_NumEntities(numEntities)
	{
	}
	SystemCollision() = default;
	~SystemCollision()
	{
	}

	// Inherited via ISystem
	void OnUpdate(std::span<Entity> entities, float deltaTime) override;

private:
	// Stored for use by OnUpdate (implemented elsewhere)
	uint32_t m_NumEntities = 0;
	VkDeviceSize m_EntitySize = 0;
};
