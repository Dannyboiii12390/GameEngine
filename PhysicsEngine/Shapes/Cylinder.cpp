#include "Cylinder.h"
#include <glm/glm.hpp>
#include <algorithm>

using namespace Physics;

Cylinder::Cylinder(const glm::vec3& a, const glm::vec3& b, float radius)
    : m_a(a), m_b(b), m_radius(radius)
{
}

const glm::vec3& Cylinder::getA() const { return m_a; }
const glm::vec3& Cylinder::getB() const { return m_b; }
float Cylinder::getRadius() const { return m_radius; }

bool Cylinder::ContainsPoint(const glm::vec3& point) const
{
    // Project point onto segment ab, clamp to [0,1], compute closest point,
    // then test radial distance against radius.
    glm::vec3 ab = m_b - m_a;
    float abLenSq = glm::dot(ab, ab);
    if (abLenSq == 0.0f)
    {
        // Degenerate: segment is a point -> treat as sphere test at m_a
        glm::vec3 d = point - m_a;
        return glm::dot(d, d) <= m_radius * m_radius;
    }

    float t = glm::dot(point - m_a, ab) / abLenSq;
    t = std::clamp(t, 0.0f, 1.0f);
    glm::vec3 closest = m_a + t * ab;
    glm::vec3 diff = point - closest;
    return glm::dot(diff, diff) <= m_radius * m_radius; // inclusive: on surface counts as inside
}

bool Cylinder::Intersects(const Sphere& sphere) const
{
	return sphere.Intersects(*this);
}