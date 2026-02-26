#pragma once

#include <glm/glm.hpp>

namespace Physics
{
	// Small helper representing an object's orientation (degrees).
	// Provides a simple ApplyAngularDisplacement(deltaDegrees) API
	// which accumulates rotation and normalizes angles into [0,360).
	class Transform
	{
	public:
		Transform() = default;
		explicit Transform(const glm::vec3& rotationDegrees) : m_rotationDegrees(rotationDegrees) {}

		// Apply an angular displacement expressed in degrees.
		// The rotation is accumulated; angles are normalized to [0,360).
		void ApplyAngularDisplacement(const glm::vec3& deltaDegrees);

		// Accessors
		const glm::vec3& GetRotationDegrees() const { return m_rotationDegrees; }
		void SetRotationDegrees(const glm::vec3& rot) { m_rotationDegrees = rot; Normalize(); }

	private:
		void Normalize();

	private:
		glm::vec3 m_rotationDegrees{ 0.0f, 0.0f, 0.0f };
	};
}