#include "Capsule.h"
#include "Sphere.h"
#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>

using namespace Physics;

Capsule::Capsule(const glm::vec3& a, const glm::vec3& b, float radius)
    : m_a(a), m_b(b), m_radius(radius)
{
}

const glm::vec3& Capsule::getA() const { return m_a; }
const glm::vec3& Capsule::getB() const { return m_b; }
float Capsule::getRadius() const { return m_radius; }

static float DistancePointToSegment(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b)
{
    glm::vec3 ab = b - a;
    float denom = glm::dot(ab, ab);
    if (denom == 0.0f)
        return glm::length(p - a);

    float t = glm::dot(p - a, ab) / denom;
    t = std::clamp(t, 0.0f, 1.0f);
    glm::vec3 closest = a + t * ab;
    return glm::length(p - closest);
}

bool Capsule::ContainsPoint(const glm::vec3& point) const
{
    float dist = DistancePointToSegment(point, m_a, m_b);
    return dist <= m_radius;
}

bool Capsule::Intersects(const Sphere& s) const
{
    // Compute minimal distance from sphere center to the capsule's central segment.
    float dist = DistancePointToSegment(s.getPos(), m_a, m_b);
    // Intersection occurs when center-to-segment distance <= (capsuleRadius + sphereRadius)
    return dist <= (m_radius + s.getRadius());
}