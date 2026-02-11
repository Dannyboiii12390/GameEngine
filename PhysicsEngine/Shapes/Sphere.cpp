
#include "Sphere.h"
#include "LineInf.h"
#include "Plane.h"
#include <glm/glm.hpp>

using namespace Physics;

Sphere::Sphere(const glm::vec3& pos, float radius)
    : m_pos(pos), m_radius(radius)
{
}

const glm::vec3& Sphere::getPos() const { return m_pos; }
float Sphere::getRadius() const { return m_radius; }

bool Sphere::isCollidingWith(const Sphere& other) const
{
    glm::vec3 diff = m_pos - other.m_pos;
    float distanceSquared = glm::dot(diff, diff);
    float radiusSum = m_radius + other.m_radius;
    return distanceSquared <= radiusSum * radiusSum;
}

bool Sphere::isCollidingWith(const glm::vec3& point) const
{
    glm::vec3 diff = m_pos - point;
    float distanceSquared = glm::dot(diff, diff);
    return distanceSquared <= m_radius * m_radius;
}

bool Sphere::isCollidingWith(const LineInf& line) const
{
    // Closest point on line to sphere center
    glm::vec3 closest = line.getShortestPathToPoint(m_pos);
    glm::vec3 diff = m_pos - closest;
    float distSq = glm::dot(diff, diff);
    return distSq <= m_radius * m_radius;
}
bool Sphere::isCollidingWith(const Plane& plane) const
{
    float dist = plane.getShortestDistance(m_pos);
    return dist <= m_radius;
}