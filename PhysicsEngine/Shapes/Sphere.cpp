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
	: m_pos(pos), m_radius(radius), Collider(EColliderType::SPHERE)
{
}

const glm::vec3& Sphere::getPos() const { return m_pos; }
float Sphere::getRadius() const { return m_radius; }

bool Sphere::isColliding(const Sphere& other) const 
{ 
    glm::vec3 diff = m_pos - other.m_pos;
    float distanceSquared = glm::dot(diff, diff);
    float radiusSum = m_radius + other.m_radius;
    return distanceSquared <= radiusSum * radiusSum;
} 
bool Sphere::isColliding(const LineInf& other) const 
{ 
    // Closest point on line to sphere center
    glm::vec3 closest = other.getShortestPathToPoint(m_pos);
    glm::vec3 diff = m_pos - closest;
    float distSq = glm::dot(diff, diff);
    return distSq <= m_radius * m_radius;
} 
bool Sphere::isColliding(const Capsule& other) const 
{ 
    const glm::vec3 A = other.getA();
    const glm::vec3 B = other.getB();
    const glm::vec3 P = m_pos;
    const float rCaps = other.getRadius();

    glm::vec3 AB = B - A;
    float ab2 = glm::dot(AB, AB);

    const float EPS = 1e-8f;
    glm::vec3 closest;
    if (ab2 <= EPS)
    {
        // Degenerate segment -> treat as sphere at A
        closest = A;
    }
    else
    {
        float t = glm::dot(P - A, AB) / ab2;
        t = std::clamp(t, 0.0f, 1.0f);
        closest = A + t * AB;
    }

    glm::vec3 diff = P - closest;
    float distSq = glm::dot(diff, diff);
    float radiusSum = m_radius + rCaps;
    return distSq <= radiusSum * radiusSum;
}
bool Sphere::isColliding(const Cylinder& other) const 
{ 
    const glm::vec3 A = other.getA();
    const glm::vec3 B = other.getB();
    const float R = other.getRadius();
    const glm::vec3 P = m_pos;

    glm::vec3 AB = B - A;
    float L = glm::length(AB);

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
        glm::vec3 radialVec = AP - t_raw * u;
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
    else // t_raw > L
    {
        float axialDist = t_raw - L;
        glm::vec3 radialVec = AP - L * u;
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
bool Sphere::isColliding(const Plane& other) const 
{ 
    float signedDist = other.signedDistance(m_pos);
    float absDist = std::abs(signedDist);
    return absDist <= m_radius;
}

void Sphere::setScale(const glm::vec3& newScale) 
{
    // Uniformly scale the radius by the largest component of the scale vector.
    float maxScale = std::max(newScale.x, std::max(newScale.y, newScale.z));
    m_radius *= maxScale;
}