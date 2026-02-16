#pragma once

#include "glm/glm.hpp"
#include "IComponent.h"
#include "../../PhysicsEngine/Maths/Integration.h"

static constexpr float g_GRAVITY_CONSTANT = 9.81f;

class ComponentPhysics : public IComponent
{
public:
	ComponentPhysics()
		: IComponent(EComponentType::Component_Physics)
	{
	}

	// Mass handling
	void SetMass(float mass)
	{
		m_Mass = mass;
		m_InverseMass = (mass > 0.0f) ? (1.0f / mass) : 0.0f;
	}
	float GetMass() const { return m_Mass; }
	float GetInverseMass() const { return m_InverseMass; }

	// Apply a force vector (accumulates over the frame)
	void ApplyForce(const glm::vec3& force)
	{
		m_totalForce += force;
	}

	// Clear accumulated forces explicitly
	void ClearForces() { m_totalForce = glm::vec3(0.0f); }

	const glm::vec3& getTotalForce() { return m_totalForce; }
	bool IsAffectedByGravity() const { return m_IsAffectedByGravity; }

	void SetIntegrationMethod(Physics::EIntegrationMethod method) { m_IntegrationMethod = method; }
	Physics::EIntegrationMethod GetIntegrationMethod() const { return m_IntegrationMethod; }


private:
	
	// Mass
	float m_Mass = 1.0f;
	float m_InverseMass = 1.0f; // maintained by SetMass

	// Accumulated force for the current frame (vector)
	glm::vec3 m_totalForce = glm::vec3(0.0f);

	// Gravity toggle and gravity constant
	bool m_IsAffectedByGravity = true;

	Physics::EIntegrationMethod m_IntegrationMethod = Physics::EIntegrationMethod::SemiImplicitEuler;
};
