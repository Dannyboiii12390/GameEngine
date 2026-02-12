#pragma once

#include "ISystem.h"
#include <vulkan/vulkan.h>
#include <vector>

class VulkanRHI;
class Entity;

/// Simple rendering system that drives ComponentGeometry instances.
/// - Does not allocate command buffers or manage frame lifecycle (VulkanRHI does).
/// - Render() expects a valid VkCommandBuffer already begun by the caller.
class SystemRenderer : public ISystem
{
public:
	SystemRenderer() = default;
	~SystemRenderer();

	// Non-copyable
	SystemRenderer(const SystemRenderer&) = delete;
	SystemRenderer& operator=(const SystemRenderer&) = delete;

	// Movable
	SystemRenderer(SystemRenderer&&) noexcept = default;
	SystemRenderer& operator=(SystemRenderer&&) noexcept = default;

	// Initialize with the RHI (kept for future use)
	void Initialize(VulkanRHI* rhi);
	void Shutdown();

	// Render a list of entities using the provided command buffer.
	// Entities containing a ComponentGeometry will be drawn (if valid).
	void Render(VkCommandBuffer cmd, std::vector<Entity>& entities);

private:
	VulkanRHI* m_RHI = nullptr;
};