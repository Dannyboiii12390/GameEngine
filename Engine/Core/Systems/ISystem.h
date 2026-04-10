#pragma once
#include <span>

enum class ESystemType
{
	System_Invalid = 0,
	System_Velocity = 1,
	System_Rendering = 1 << 1,
	System_Physics = 1 << 2,
	System_Collision = 1 << 3,
	System_Network_Sync = 1 << 4
};
class Entity;

class ISystem
{
public:
	virtual ~ISystem() = default; // IMPORTANT: ensures derived destructors run
	virtual void OnUpdate(std::span<Entity> entities, float deltaTime) = 0;

	ESystemType GetSystemType() const { return m_SystemType; }

protected:
	ESystemType m_SystemType = ESystemType::System_Invalid;
};
