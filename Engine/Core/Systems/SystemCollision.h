#pragma once

#include "ISystem.h"

// Forward declaration
class VulkanRHI;


class SystemCollision : public ISystem
{
public:
	SystemCollision(VulkanRHI* rhi) : m_RHI(rhi) {}
	SystemCollision() = default;

	// Inherited via ISystem
	void OnUpdate(std::span<Entity> entities, float deltaTime) override;

private:
	VulkanRHI* m_RHI = nullptr;
};
