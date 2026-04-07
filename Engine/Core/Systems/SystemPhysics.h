
#pragma once

#include "ISystem.h"
#include <span>
#include "../Entity.h"

#include "../../PhysicsEngine/Maths/Integration.h"

#include <glm/glm.hpp>
#include "../NetworkTypes.h"

class Entity;

class SystemPhysics : public ISystem
{
public:
	SystemPhysics(PeerID localPeerID) : ISystem(), m_localPeerId(localPeerID)
	{
		m_SystemType = ESystemType::System_Physics;
	}
	SystemPhysics() : SystemPhysics(0) {} // Default to local peer ID 0 if not provided

	// Update physics: apply gravity (and any per-object forces handled elsewhere)
	// This system updates velocities (semi-implicit Euler): v += a * dt.
	// Position integration is left to SystemVelocity (keep responsibilities separate).
	void OnUpdate(std::span<Entity> entities, float deltaTime);

	void SetLocalPeerId(PeerID localPeerID) { m_localPeerId = localPeerID; }

private:
	PeerID m_localPeerId; // This should be set to the actual local peer ID in a real implementation

};