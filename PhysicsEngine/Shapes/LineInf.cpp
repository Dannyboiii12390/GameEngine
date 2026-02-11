
#include "LineInf.h"
#include "Sphere.h"
#include <glm/glm.hpp>

using namespace Physics;

LineInf::LineInf(const glm::vec3& a, const glm::vec3& b)
    : m_a(a), m_b(b)
{
}

glm::vec3 LineInf::getA() const { return m_a; }
glm::vec3 LineInf::getB() const { return m_b; }

glm::vec3 LineInf::getShortestPathToPoint(const glm::vec3& point) const
{
    glm::vec3 ab = m_b - m_a;
    float denom = glm::dot(ab, ab);
    if (denom == 0.0f) // degenerate line (a == b)
        return m_a;
    float t = glm::dot(point - m_a, ab) / denom;
    return m_a + t * ab;
}

bool LineInf::isCollidingWith(const Sphere& sphere) const
{
    glm::vec3 closest = getShortestPathToPoint(sphere.getPos());
    glm::vec3 diff = sphere.getPos() - closest;
    float distSq = glm::dot(diff, diff);
    float r = sphere.getRadius();
    return distSq <= r * r;
}