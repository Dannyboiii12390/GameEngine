#include "Sphere.h"
#include "LineInf.h"
#include "Plane.h"
#include "Cylinder.h"
#include "Capsule.h"

#include <glm/glm.hpp>
#include <glm/gtc/epsilon.hpp>
#include <algorithm>
#include <cmath>

using namespace Physics;

Sphere::Sphere(const glm::vec3& pos, float radius)
    : m_pos(pos), m_radius(radius)
{
}

const glm::vec3& Sphere::getPos() const { return m_pos; }
float Sphere::getRadius() const { return m_radius; }

bool Sphere::Intersects(const Sphere& other) const
{
    glm::vec3 diff = m_pos - other.m_pos;
    float distanceSquared = glm::dot(diff, diff);
    float radiusSum = m_radius + other.m_radius;
    return distanceSquared <= radiusSum * radiusSum;
}

bool Sphere::Intersects(const glm::vec3& point) const
{
    glm::vec3 diff = m_pos - point;
    float distanceSquared = glm::dot(diff, diff);
    return distanceSquared <= m_radius * m_radius;
}

bool Sphere::Intersects(const LineInf& line) const
{
    // Closest point on line to sphere center
    glm::vec3 closest = line.getShortestPathToPoint(m_pos);
    glm::vec3 diff = m_pos - closest;
    float distSq = glm::dot(diff, diff);
    return distSq <= m_radius * m_radius;
}
bool Sphere::Intersects(const Plane& plane) const
{
    float dist = plane.getShortestDistance(m_pos);
    return dist <= m_radius;
}
bool Sphere::Intersects(const Cylinder& cyl) const
{
    const glm::vec3 A = cyl.getA();
    const glm::vec3 B = cyl.getB();
    const float R = cyl.getRadius();
    const glm::vec3 P = m_pos;

    glm::vec3 AB = B - A;
    float L = glm::length(AB);

    // Degenerate axis -> treat as sphere (cap) at A with radius R
    const float EPS = 1e-6f;
    if (L <= EPS)
    {
        glm::vec3 diff = P - A;
        float distSq = glm::dot(diff, diff);
        float radiusSum = m_radius + R;
        return distSq <= radiusSum * radiusSum;
    }

    glm::vec3 u = AB / L; // unit axis
    glm::vec3 AP = P - A;
    float t_raw = glm::dot(AP, u);

    float shortestDistanceToCylinderVolume = 0.0f;

    if (t_raw >= 0.0f && t_raw <= L)
    {
        // Projects inside segment - distance to lateral surface
        glm::vec3 radialVec = AP - t_raw * u;
        float radialDist = glm::length(radialVec);
        shortestDistanceToCylinderVolume = std::max(0.0f, radialDist - R);
    }
    else if (t_raw < 0.0f)
    {
        float axialDist = -t_raw;
        // radial distance relative to axis projection
        glm::vec3 radialVec = AP - t_raw * u; // AP - (dot(AP,u))*u gives perpendicular component
        float radialDist = glm::length(radialVec);
        if (radialDist <= R)
        {
            shortestDistanceToCylinderVolume = axialDist; // nearest point on cap disk
        }
        else
        {
            float rimDelta = radialDist - R;
            shortestDistanceToCylinderVolume = std::sqrt(rimDelta * rimDelta + axialDist * axialDist);
        }
    }
    else // t_raw > L
    {
        float axialDist = t_raw - L;
        glm::vec3 radialVec = AP - L * u; // relative to B
        float radialDist = glm::length(radialVec);
        if (radialDist <= R)
        {
            shortestDistanceToCylinderVolume = axialDist;
        }
        else
        {
            float rimDelta = radialDist - R;
            shortestDistanceToCylinderVolume = std::sqrt(rimDelta * rimDelta + axialDist * axialDist);
        }
    }

    return shortestDistanceToCylinderVolume <= m_radius;
}

bool Sphere::Intersects(const Capsule& cap) const
{
	return cap.Intersects(*this);
}
/*
bool Sphere::isColliding(const Sphere& other) 
{ 
    glm::vec3 diff = m_pos - other.m_pos;
    float distanceSquared = glm::dot(diff, diff);
    float radiusSum = m_radius + other.m_radius;
    return distanceSquared <= radiusSum * radiusSum;
} 
bool Sphere::isColliding(const LineInf& other) 
{ 
    // Closest point on line to sphere center
    glm::vec3 closest = other.getShortestPathToPoint(m_pos);
    glm::vec3 diff = m_pos - closest;
    float distSq = glm::dot(diff, diff);
    return distSq <= m_radius * m_radius;
} 
bool Sphere::isColliding(const Capsule& other) 
{ 
    return other.isColliding(*this);
}
bool Sphere::isColliding(const Cylinder& other) { return Intersects(other); } 
bool Sphere::isColliding(const Plane& other) { return Intersects(other); }*/