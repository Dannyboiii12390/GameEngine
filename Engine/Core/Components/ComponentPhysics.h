#pragma once

#include "glm/glm.hpp"
#include "IComponent.h"
#include "../../PhysicsEngine/Maths/Integration.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>

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
	void SetAffectedByGravity(bool affected) { m_IsAffectedByGravity = affected; }

	// --- Torque / rotation support ---------------------------------------
	// Apply a torque (accumulates over the frame). Torque is a 3D vector whose
	// direction is the axis (right-hand rule) and magnitude is the torque scalar.
	void ApplyTorque(const glm::vec3& torque)
	{
		m_totalTorque += torque;
	}

	// Clear accumulated torques explicitly
	void ClearTorques() { m_totalTorque = glm::vec3(0.0f); }

	// Read-only access to accumulated torque
	const glm::vec3& getTotalTorque() const { return m_totalTorque; }

	// Legacy: set diagonal moment-of-inertia (principal axes aligned with object).
	// This convenience sets the inertia tensor to a diagonal matrix with these components.
	void SetMomentOfInertia(const glm::vec3& moi)
	{
		m_MomentOfInertia = moi;
		m_InertiaTensor = glm::mat3(0.0f);
		m_InertiaTensor[0][0] = moi.x;
		m_InertiaTensor[1][1] = moi.y;
		m_InertiaTensor[2][2] = moi.z;
		m_InverseInertiaTensor = glm::inverse(m_InertiaTensor);
	}

	// Set full inertia tensor (object-space). Inverse is computed and cached.
	void SetInertiaTensor(const glm::mat3& inertia)
	{
		m_InertiaTensor = inertia;
		// Use glm inverse; caller should ensure tensor is invertible (non-zero principal elements for diagonal case).
		m_InverseInertiaTensor = glm::inverse(m_InertiaTensor);
	}

	const glm::mat3& GetInertiaTensor() const { return m_InertiaTensor; }
	const glm::mat3& GetInverseInertiaTensor() const { return m_InverseInertiaTensor; }

	// Convenience: compute and set inertia tensor for a solid cylinder aligned with object Z axis.
	// radius: cylinder radius, height: cylinder height, mass: use mass parameter (or fall back to component mass).
	// Formulas (solid cylinder about central axis Z):
	//   I_xx = I_yy = (1/12) * m * (3*r^2 + h^2)
	//   I_zz = (1/2) * m * r^2
	void SetCylinderInertia(float mass, float radius, float height)
	{
		float I_xx = (1.0f / 12.0f) * mass * (3.0f * radius * radius + height * height);
		float I_yy = I_xx;
		float I_zz = 0.5f * mass * radius * radius;
		glm::mat3 I = glm::mat3(0.0f);
		I[0][0] = I_xx;
		I[1][1] = I_yy;
		I[2][2] = I_zz;
		SetInertiaTensor(I);
	}

private:
	
	// Mass
	float m_Mass = 1.0f;
	float m_InverseMass = 1.0f; // maintained by SetMass

	// Accumulated force for the current frame (vector)
	glm::vec3 m_totalForce = glm::vec3(0.0f);

	// Gravity toggle and gravity constant
	bool m_IsAffectedByGravity = true;

	// Legacy per-axis scalar moment-of-inertia storage for convenience.
	glm::vec3 m_MomentOfInertia = glm::vec3(1.0f, 1.0f, 1.0f);

	// Full inertia tensor (object-space) and cached inverse.
	glm::mat3 m_InertiaTensor = glm::mat3(1.0f);
	glm::mat3 m_InverseInertiaTensor = glm::mat3(1.0f);

	// Accumulated torque for the current frame (vector).
	glm::vec3 m_totalTorque = glm::vec3(0.0f);

	Physics::EIntegrationMethod m_IntegrationMethod = Physics::EIntegrationMethod::SemiImplicitEuler;
};
