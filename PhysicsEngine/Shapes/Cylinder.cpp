#include "Cylinder.h"
#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>

#include "Sphere.h" 
#include "LineInf.h" 
#include "Plane.h" 
#include "Capsule.h"

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
    const glm::vec3 A = getA();
    const glm::vec3 B = getB();
    const float cylR = getRadius();
    const float sphereR = sphere.getRadius();
    const glm::vec3 P = sphere.getPos();

    glm::vec3 AB = B - A;
    float L = glm::length(AB);

    // Degenerate axis -> treat as sphere (cap) at A with radius cylR
    const float EPS = 1e-6f;
    if (L <= EPS)
    {
        glm::vec3 diff = P - A;
        float distSq = glm::dot(diff, diff);
        float radiusSum = cylR + sphereR;
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
        shortestDistanceToCylinderVolume = std::max(0.0f, radialDist - cylR);
    }
    else if (t_raw < 0.0f)
    {
        float axialDist = -t_raw;
        // radial distance relative to axis projection
        glm::vec3 radialVec = AP - t_raw * u; // AP - (dot(AP,u))*u gives perpendicular component
        float radialDist = glm::length(radialVec);
        if (radialDist <= cylR)
        {
            shortestDistanceToCylinderVolume = axialDist; // nearest point on cap disk
        }
        else
        {
            float rimDelta = radialDist - cylR;
            shortestDistanceToCylinderVolume = std::sqrt(rimDelta * rimDelta + axialDist * axialDist);
        }
    }
    else // t_raw > L
    {
        float axialDist = t_raw - L;
        glm::vec3 radialVec = AP - L * u; // relative to B
        float radialDist = glm::length(radialVec);
        if (radialDist <= cylR)
        {
            shortestDistanceToCylinderVolume = axialDist;
        }
        else
        {
            float rimDelta = radialDist - cylR;
            shortestDistanceToCylinderVolume = std::sqrt(rimDelta * rimDelta + axialDist * axialDist);
        }
    }

    // Intersection occurs if distance from sphere center to cylinder volume <= sphere radius
    return shortestDistanceToCylinderVolume <= sphereR;
}

bool Cylinder::isColliding(const Sphere& other) const
{
    // Mirror of Intersects: compare against the sphere's radius
    const glm::vec3 A = getA();
    const glm::vec3 B = getB();
    const float cylR = getRadius();
    const float sphereR = other.getRadius();
    const glm::vec3 P = other.getPos();

    glm::vec3 AB = B - A;
    float L = glm::length(AB);

    // Degenerate axis -> treat as sphere (cap) at A with radius cylR
    const float EPS = 1e-6f;
    if (L <= EPS)
    {
        glm::vec3 diff = P - A;
        float distSq = glm::dot(diff, diff);
        float radiusSum = cylR + sphereR;
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
        shortestDistanceToCylinderVolume = std::max(0.0f, radialDist - cylR);
    }
    else if (t_raw < 0.0f)
    {
        float axialDist = -t_raw;
        glm::vec3 radialVec = AP - t_raw * u; // AP - (dot(AP,u))*u gives perpendicular component
        float radialDist = glm::length(radialVec);
        if (radialDist <= cylR)
        {
            shortestDistanceToCylinderVolume = axialDist; // nearest point on cap disk
        }
        else
        {
            float rimDelta = radialDist - cylR;
            shortestDistanceToCylinderVolume = std::sqrt(rimDelta * rimDelta + axialDist * axialDist);
        }
    }
    else // t_raw > L
    {
        float axialDist = t_raw - L;
        glm::vec3 radialVec = AP - L * u; // relative to B
        float radialDist = glm::length(radialVec);
        if (radialDist <= cylR)
        {
            shortestDistanceToCylinderVolume = axialDist;
        }
        else
        {
            float rimDelta = radialDist - cylR;
            shortestDistanceToCylinderVolume = std::sqrt(rimDelta * rimDelta + axialDist * axialDist);
        }
    }

    return shortestDistanceToCylinderVolume <= sphereR;
}
bool Cylinder::isColliding(const LineInf& other) const
{
    const glm::vec3 A = getA();
    const glm::vec3 B = getB();
    const float cylR = getRadius();

    const glm::vec3 P0 = other.getA();
    const glm::vec3 P1 = other.getB();

    const float EPS = 1e-6f;

    // Degenerate line -> treat as point
    glm::vec3 vRaw = P1 - P0;
    float vLen = glm::length(vRaw);
    if (vLen <= EPS)
    {
        // Use existing point-in-cylinder test
        return ContainsPoint(P0);
    }
    glm::vec3 v = vRaw / vLen; // unit direction of infinite line

    // Cylinder axis
    glm::vec3 AB = B - A;
    float L = glm::length(AB);
    if (L <= EPS)
    {
        // Cylinder degenerates to a disk/sphere centered at A with radius cylR.
        // Distance from point A to the infinite line:
        glm::vec3 w = A - P0;
        float t = glm::dot(w, v); // parameter on line for closest point
        glm::vec3 closest = P0 + v * t;
        float distSq = glm::dot(A - closest, A - closest);
        return distSq <= cylR * cylR;
    }

    glm::vec3 u = AB / L; // unit axis

    // Compute closest points between two infinite lines (axis: A + u*s, line: P0 + v*t)
    glm::vec3 w0 = A - P0;
    float b = glm::dot(u, v);
    float d = glm::dot(u, w0);
    float e = glm::dot(v, w0);
    float denom = 1.0f - b * b; // since dot(u,u)=dot(v,v)=1

    if (std::abs(denom) > EPS)
    {
        // Non-parallel lines: compute s,t for closest approach
        float s = (b * e - d) / denom;
        float t = (e - b * d) / denom; // rearranged from formula to avoid recomputing a,c

        // If the axis-parameter s lies within [0, L], then the closest approach touches the finite cylinder's lateral surface
        if (s >= 0.0f && s <= L)
        {
            glm::vec3 pointOnAxis = A + u * s;
            glm::vec3 pointOnLine = P0 + v * t;
            float distSq = glm::dot(pointOnAxis - pointOnLine, pointOnAxis - pointOnLine);
            if (distSq <= cylR * cylR)
                return true;
        }
    }
    else
    {
        // Parallel case: distance between lines is constant. Compute perpendicular distance from line to axis.
        // perpendicular vector from P0 to axis: (A - P0) - projection onto u
        glm::vec3 diff = (A - P0) - glm::dot(A - P0, u) * u;
        float distSq = glm::dot(diff, diff);
        if (distSq <= cylR * cylR)
        {
            // Because the line is infinite, some point along the line will have its projection onto the axis
            // within any finite interval; therefore a lateral intersection exists with the finite cylinder
            // if the projection interval overlaps [0,L]. The infinite line spans all projection values,
            // so there will be some projection s ∈ [0,L] -> collision.
            return true;
        }
    }

    // Check intersection with end caps (disks) at A and B
    const glm::vec3 capCenters[2] = { A, B };
    for (int i = 0; i < 2; ++i)
    {
        const glm::vec3& C = capCenters[i];
        float denomPlane = glm::dot(u, v);

        if (std::abs(denomPlane) < EPS)
        {
            // Line parallel to cap plane.
            // If line lies in the plane (signed distance ~ 0), check 2D distance from center to line.
            float v_signed = glm::dot(u, P0 - C);
            if (std::abs(v_signed) < EPS)
            {
                // Closest point on the infinite line to C
                float tClosest = glm::dot(v, C - P0);
                glm::vec3 closest = P0 + v * tClosest;
                float distSq = glm::dot(C - closest, C - closest);
                if (distSq <= cylR * cylR)
                    return true;
            }
            // otherwise no intersection with this cap (line parallel but offset)
        }
        else
        {
            // Solve for t where line intersects the cap's plane: dot(u, (P0 + v*t) - C) = 0
            float tPlane = -glm::dot(u, P0 - C) / denomPlane;
            glm::vec3 X = P0 + v * tPlane;
            float distSq = glm::dot(X - C, X - C);
            if (distSq <= cylR * cylR)
                return true;
        }
    }

    // No intersection found
    return false;
}
bool Cylinder::isColliding(const Capsule& other) const
{
    // Collision if the minimal distance between the two center segments
    // is <= sum of radii. (Capsule is segment with radius r; cylinder volume
    // is all points within cylR of some point on its axis segment.)
    const glm::vec3 P0 = getA();
    const glm::vec3 P1 = getB();
    const float r0 = getRadius();

    const glm::vec3 Q0 = other.getA();
    const glm::vec3 Q1 = other.getB();
    const float r1 = other.getRadius();

    // Helper: squared distance between two segments (from Real-Time Collision Detection)
    auto segSegDistSq = [](const glm::vec3& p1, const glm::vec3& q1, const glm::vec3& p2, const glm::vec3& q2) -> float
        {
            const float EPS = 1e-8f;
            glm::vec3 d1 = q1 - p1; // direction vector of segment S1
            glm::vec3 d2 = q2 - p2; // direction vector of segment S2
            glm::vec3 r = p1 - p2;
            float a = glm::dot(d1, d1); // squared length of segment S1
            float e = glm::dot(d2, d2); // squared length of segment S2
            float f = glm::dot(d2, r);

            // If both segments degenerate to points
            if (a <= EPS && e <= EPS)
            {
                glm::vec3 diff = p1 - p2;
                return glm::dot(diff, diff);
            }
            // If first segment degenerates to a point
            if (a <= EPS)
            {
                float t = std::clamp(f / e, 0.0f, 1.0f);
                glm::vec3 c2 = p2 + d2 * t;
                glm::vec3 diff = p1 - c2;
                return glm::dot(diff, diff);
            }
            // If second segment degenerates to a point
            if (e <= EPS)
            {
                float s = std::clamp(-glm::dot(d1, r) / a, 0.0f, 1.0f);
                glm::vec3 c1 = p1 + d1 * s;
                glm::vec3 diff = c1 - p2;
                return glm::dot(diff, diff);
            }

            float b = glm::dot(d1, d2);
            float c = glm::dot(d1, r);
            float denom = a * e - b * b;

            float s = 0.0f;
            float t = 0.0f;

            if (denom != 0.0f)
            {
                s = std::clamp((b * f - c * e) / denom, 0.0f, 1.0f);
            }
            else
            {
                s = 0.0f; // parallel; force s = 0 and compute t below
            }

            // Compute t to minimize distance given s
            t = (b * s + f) / e;
            if (t < 0.0f)
            {
                t = 0.0f;
                s = std::clamp(-c / a, 0.0f, 1.0f);
            }
            else if (t > 1.0f)
            {
                t = 1.0f;
                s = std::clamp((b - c) / a, 0.0f, 1.0f);
            }

            glm::vec3 c1 = p1 + d1 * s;
            glm::vec3 c2 = p2 + d2 * t;
            glm::vec3 diff = c1 - c2;
            return glm::dot(diff, diff);
        };

    float distSq = segSegDistSq(P0, P1, Q0, Q1);
    float radiusSum = r0 + r1;
    return distSq <= radiusSum * radiusSum;
}
bool Cylinder::isColliding(const Cylinder& other) const
{
    // Two finite cylinders collide if the minimal distance between their axis segments
    // is <= sum of radii (the cylinder volume is all points within radius of axis segment).
    const glm::vec3 P0 = getA();
    const glm::vec3 P1 = getB();
    const float r0 = getRadius();

    const glm::vec3 Q0 = other.getA();
    const glm::vec3 Q1 = other.getB();
    const float r1 = other.getRadius();

    // Reuse same segment-segment squared distance routine
    auto segSegDistSq = [](const glm::vec3& p1, const glm::vec3& q1, const glm::vec3& p2, const glm::vec3& q2) -> float
        {
            const float EPS = 1e-8f;
            glm::vec3 d1 = q1 - p1; // direction vector of segment S1
            glm::vec3 d2 = q2 - p2; // direction vector of segment S2
            glm::vec3 r = p1 - p2;
            float a = glm::dot(d1, d1); // squared length of segment S1
            float e = glm::dot(d2, d2); // squared length of segment S2
            float f = glm::dot(d2, r);
            // If both segments degenerate to points
            if (a <= EPS && e <= EPS)
            {
                glm::vec3 diff = p1 - p2;
                return glm::dot(diff, diff);
            }
            // If first segment degenerates to a point
            if (a <= EPS)
            {
                float t = std::clamp(f / e, 0.0f, 1.0f);
                glm::vec3 c2 = p2 + d2 * t;
                glm::vec3 diff = p1 - c2;
                return glm::dot(diff, diff);
            }
            // If second segment degenerates to a point
            if (e <= EPS)
            {
                float s = std::clamp(-glm::dot(d1, r) / a, 0.0f, 1.0f);
                glm::vec3 c1 = p1 + d1 * s;
                glm::vec3 diff = c1 - p2;
                return glm::dot(diff, diff);
            }

            float b = glm::dot(d1, d2);
            float c = glm::dot(d1, r);
            float denom = a * e - b * b;

            float s = 0.0f;
            float t = 0.0f;

            if (denom != 0.0f)
            {
                s = std::clamp((b * f - c * e) / denom, 0.0f, 1.0f);
            }
            else
            {
                s = 0.0f; // parallel; force s = 0 and compute t below
            }

            // Compute t to minimize distance given s
            t = (b * s + f) / e;

            if (t < 0.0f)
            {
                t = 0.0f;
                s = std::clamp(-c / a, 0.0f, 1.0f);
            }
            else if (t > 1.0f)
            {
                t = 1.0f;
                s = std::clamp((b - c) / a, 0.0f, 1.0f);
            }

            glm::vec3 c1 = p1 + d1 * s;
            glm::vec3 c2 = p2 + d2 * t;
            glm::vec3 diff = c1 - c2;
            return glm::dot(diff, diff);
        };

    float distSq = segSegDistSq(P0, P1, Q0, Q1);
    float radiusSum = r0 + r1;
    return distSq <= radiusSum * radiusSum;
}
bool Cylinder::isColliding(const Plane& other) const
{
    // Cylinder intersects plane if any part of the finite cylinder volume crosses the plane.
    // Compute signed distances of the axis endpoints to the plane.
    const glm::vec3 A = getA();
    const glm::vec3 B = getB();
    const float cylR = getRadius();

    const float dA = other.signedDistance(A);
    const float dB = other.signedDistance(B);

    const float absA = std::abs(dA);
    const float absB = std::abs(dB);

    // Degenerate axis -> sphere-case
    const float EPS = 1e-6f;
    if (glm::length(B - A) <= EPS)
    {
        return absA <= cylR;
    }

    // If either cap center is within radius distance of the plane, there is intersection
    if (absA <= cylR || absB <= cylR)
        return true;

    // If signed distances have opposite signs, the plane crosses the axis segment.
    // At crossing point plane distance is zero which is <= cylR so there is intersection.
    if (dA * dB < 0.0f)
        return true;

    // Otherwise axis is entirely on one side and both endpoints are farther than cylR -> no intersection
    return false;
}
