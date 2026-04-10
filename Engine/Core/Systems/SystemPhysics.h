#pragma once

#include "ISystem.h"
#include <span>
#include <atomic>
#include "../Entity.h"
#include "../../PhysicsEngine/Maths/Integration.h"
#include <glm/glm.hpp>
#include "../NetworkTypes.h"

class Entity;

class SystemPhysics : public ISystem
{
public:
	// Keep default ctor for scenes that do: make_unique<SystemPhysics>()
	SystemPhysics()
		: ISystem(), m_localPeerId(nullptr)
	{
		m_SystemType = ESystemType::System_Physics;
	}

	// Network-aware ctor (FlatBufferScene)
	SystemPhysics(std::atomic<PeerID>* localPeerId)
		: ISystem(), m_localPeerId(localPeerId)
	{
		m_SystemType = ESystemType::System_Physics;
	}

	void OnUpdate(std::span<Entity> entities, float deltaTime);

private:
	std::atomic<PeerID>* m_localPeerId = nullptr;
};