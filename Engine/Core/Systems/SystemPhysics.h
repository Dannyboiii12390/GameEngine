
#pragma once

#include "ISystem.h"
#include <span>
#include "../Entity.h"

#include "../../PhysicsEngine/Maths/Integration.h"

#include <glm/glm.hpp>

class Entity;

class SystemPhysics : public ISystem
{
public:
	SystemPhysics() : ISystem()
	{
		m_SystemType = ESystemType::System_Physics;
	}

	SystemPhysics(std::span<Entity> ents) : ISystem()
	{
		m_SystemType = ESystemType::System_Physics;
		m_Entities = ents;
	}

	// Update physics: apply gravity (and any per-object forces handled elsewhere)
	// This system updates velocities (semi-implicit Euler): v += a * dt.
	// Position integration is left to SystemVelocity (keep responsibilities separate).
	void OnUpdate(float deltaTime);

private:
	std::span<Entity> m_Entities;
};