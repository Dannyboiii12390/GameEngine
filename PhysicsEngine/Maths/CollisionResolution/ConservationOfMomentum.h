#pragma once 
#include <glm/glm.hpp>
#include <tuple>
#include <cmath>

namespace Physics
{
	inline std::tuple<glm::vec3, glm::vec3> InElasticCollision(const glm::vec3& vel1, const glm::vec3& vel2, float m1, float m2)
	{
		glm::vec3 v1 = (m1 - m2) / (m1 + m2) * vel1 + (2 * m2) / (m1 + m2) * vel2;
		glm::vec3 v2 = (m2 - m1) / (m1 + m2) * vel2 + (2 * m1) / (m1 + m2) * vel1;

		return std::make_tuple(v1, v2);
	}
	inline std::tuple<glm::vec3, glm::vec3> ElasticCollision(const glm::vec3& vel1, const glm::vec3& vel2, float m1, float m2)
	{
		const float eps = 1e-8f;
		float massSum = m1 + m2;
		if (std::abs(massSum) < eps)
		{
			// Degenerate masses: cannot compute; return original velocities.
			return std::make_tuple(vel1, vel2);
		}

		glm::vec3 relVel = vel1 - vel2;
		float relSpeed = glm::length(relVel);
		if (relSpeed < eps)
		{
			// No relative motion along any direction: nothing changes.
			return std::make_tuple(vel1, vel2);
		}

		// Use the direction of relative velocity as the collision normal.
		glm::vec3 n = relVel / relSpeed; // normalized

		// Scalar velocities along normal
		float u1 = glm::dot(vel1, n);
		float u2 = glm::dot(vel2, n);

		// 1D elastic collision solution for normal components
		float v1n = (u1 * (m1 - m2) + 2.0f * m2 * u2) / massSum;
		float v2n = (u2 * (m2 - m1) + 2.0f * m1 * u1) / massSum;

		// Replace only the normal component; tangential component remains unchanged
		glm::vec3 v1 = vel1 + (v1n - u1) * n;
		glm::vec3 v2 = vel2 + (v2n - u2) * n;

		return std::make_tuple(v1, v2);
	}
}