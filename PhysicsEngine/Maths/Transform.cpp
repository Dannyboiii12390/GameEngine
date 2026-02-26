#include "Transform.h"
#include <cmath>

namespace Physics
{
	static float NormalizeAngle360(float deg)
	{
		// fmod can produce negative results; ensure final in [0,360)
		float r = std::fmod(deg, 360.0f);
		if (r < 0.0f) r += 360.0f;
		// If result is very close to 360, wrap to 0 to avoid 360.0 vs 0.0 ambiguity
		if (std::abs(r - 360.0f) < 1e-6f) r = 0.0f;
		return r;
	}

	void Transform::Normalize()
	{
		m_rotationDegrees.x = NormalizeAngle360(m_rotationDegrees.x);
		m_rotationDegrees.y = NormalizeAngle360(m_rotationDegrees.y);
		m_rotationDegrees.z = NormalizeAngle360(m_rotationDegrees.z);
	}

	void Transform::ApplyAngularDisplacement(const glm::vec3& deltaDegrees)
	{
		m_rotationDegrees += deltaDegrees;
		Normalize();
	}
}