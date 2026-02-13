#include "LineInf.h"

#include "Sphere.h"
#include "Plane.h"
#include "Capsule.h"
#include "Cylinder.h"

#include <glm/glm.hpp>
#include <algorithm>

using namespace Physics;

LineInf::LineInf(const glm::vec3& a, const glm::vec3& b)
	: m_a(a), m_b(b), Collider(EColliderType::LINEINF)
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

bool LineInf::isColliding(const Sphere& s, float length) const 
{
    // Use the original segment endpoints (don't normalize the absolute positions).
    // `length` in tests is provided as glm::length(m_b - m_a); we reconstruct `end`
    // to ensure we use the provided length but keep the true start position.
    glm::vec3 start = m_a;
    glm::vec3 dir = m_b - m_a;
    float dirLenSq = glm::dot(dir, dir);
    if (dirLenSq == 0.0f) // degenerate segment -> treat as point at m_a
    {
        glm::vec3 diff = s.getPos() - start;
        return glm::dot(diff, diff) <= s.getRadius() * s.getRadius();
    }

    // If tests pass a length that matches |m_b - m_a| then this gives the original end.
    // Keep defensive behaviour: if length is non-positive fallback to using m_b directly.
    glm::vec3 end;
    if (length > 0.0f)
        end = start + glm::normalize(dir) * length;
    else
        end = m_b;

    glm::vec3 ab = end - start;
    float denom = glm::dot(ab, ab);
    const float EPS = 1e-8f;
    if (denom <= EPS) // degenerate after numerical operations
    {
        glm::vec3 diff = s.getPos() - start;
        return glm::dot(diff, diff) <= s.getRadius() * s.getRadius();
    }

    float t = glm::dot(s.getPos() - start, ab) / denom;
    t = std::clamp(t, 0.0f, 1.0f); // clamp to segment
    glm::vec3 closest = start + t * ab;
    glm::vec3 diff = s.getPos() - closest;
    return glm::dot(diff, diff) <= s.getRadius() * s.getRadius();
}
bool LineInf::isColliding(const Sphere& other) const 
{
    glm::vec3 closest = getShortestPathToPoint(other.getPos());
    glm::vec3 diff = other.getPos() - closest;
    float distSq = glm::dot(diff, diff);
    float r = other.getRadius();
    return distSq <= r * r;
}
bool LineInf::isColliding(const LineInf& other) const 
{
    // Two infinite lines collide if they intersect (point) or are coincident (collinear).
    const float EPS = 1e-6f;

    glm::vec3 P1 = getA();
    glm::vec3 Q1 = getB();
    glm::vec3 P2 = other.getA();
    glm::vec3 Q2 = other.getB();

    glm::vec3 d1 = Q1 - P1;
    glm::vec3 d2 = Q2 - P2;

    float len1 = glm::length(d1);
    float len2 = glm::length(d2);

    // Degenerate line(s) -> treat degenerate as point(s)
    if (len1 <= EPS && len2 <= EPS)
    {
        // Both points: collide if same location
        return glm::dot(P1 - P2, P1 - P2) <= EPS * EPS;
    }
    if (len1 <= EPS)
    {
        // First is a point -> distance from point to other infinite line
        glm::vec3 v = d2 / (len2 > EPS ? len2 : 1.0f);
        glm::vec3 w = P1 - P2;
        float dist = glm::length(glm::cross(w, v)); // v is unit when len2>EPS
        return dist <= EPS;
    }
    if (len2 <= EPS)
    {
        glm::vec3 v = d1 / (len1 > EPS ? len1 : 1.0f);
        glm::vec3 w = P2 - P1;
        float dist = glm::length(glm::cross(w, v));
        return dist <= EPS;
    }

    glm::vec3 u1 = d1 / len1;
    glm::vec3 u2 = d2 / len2;
    glm::vec3 cross = glm::cross(u1, u2);
    float crossLen2 = glm::dot(cross, cross);

    if (crossLen2 > EPS * EPS)
    {
        // Not parallel: distance between lines = |dot((P2-P1), cross)| / |cross|
        float numer = std::abs(glm::dot(P2 - P1, cross));
        float denom = std::sqrt(crossLen2);
        float dist = numer / denom;
        return dist <= EPS;
    }
    else
    {
        // Parallel: distance between lines = |cross(P2-P1, u1)| length (u1 is unit)
        glm::vec3 w = P2 - P1;
        float dist = glm::length(glm::cross(w, u1)); // u1 unit
        return dist <= EPS; // if zero -> coincident
    }
}
bool LineInf::isColliding(const Plane& other) const 
{
    // Infinite line intersects plane unless it's parallel and not in plane.
    const float EPS = 1e-6f;
    glm::vec3 A = getA();
    glm::vec3 B = getB();

    float dA = other.signedDistance(A);
    float dB = other.signedDistance(B);

    // If either endpoint lies (numerically) on plane -> intersects
    if (std::abs(dA) <= EPS || std::abs(dB) <= EPS)
        return true;

    // denom = dot(dir, planeNormal) = signedDistance(B) - signedDistance(A)
    float denom = dB - dA;
    if (std::abs(denom) <= EPS)
    {
        // Parallel: if distances are near zero then lies in plane, otherwise no intersection.
        return std::abs(dA) <= EPS;
    }

    // Non-parallel infinite line always intersects the plane.
    return true;
}
bool LineInf::isColliding(const Capsule& other) const 
{
    // Check min distance between infinite line (this) and capsule segment; collide if <= radius.
    const float EPS = 1e-8f;
    glm::vec3 P0 = getA();
    glm::vec3 P1 = getB();
    glm::vec3 u = P1 - P0;
    float a = glm::dot(u, u);
    // direction for line, treat u may be zero but getShortestPathToPoint handles degenerate lines above.
    glm::vec3 Q0 = other.getA();
    glm::vec3 Q1 = other.getB();
    glm::vec3 v = Q1 - Q0;
    float c = glm::dot(v, v);
    float r = other.getRadius();

    // Degenerate capsule segment -> capsule is a sphere at Q0 radius r
    if (c <= EPS)
    {
        // distance from point Q0 to infinite line
        glm::vec3 closest = getShortestPathToPoint(Q0);
        float distSq = glm::dot(Q0 - closest, Q0 - closest);
        return distSq <= r * r;
    }

    // If this line is degenerate (P0==P1), treat as point vs capsule (point to segment distance)
    if (a <= EPS)
    {
        // Distance from P0 to segment Q0-Q1
        float t = glm::dot(P0 - Q0, v) / c;
        t = std::clamp(t, 0.0f, 1.0f);
        glm::vec3 closest = Q0 + v * t;
        float distSq = glm::dot(P0 - closest, P0 - closest);
        return distSq <= r * r;
    }

    // Solve for parameters t (line) and s (segment) minimizing |(P0 + u*t) - (Q0 + v*s)|^2
    float b = glm::dot(u, v);
    glm::vec3 w0 = P0 - Q0;
    float d = glm::dot(u, w0);
    float e = glm::dot(v, w0);

    float denom = a * c - b * b;
    float s = 0.0f;
    float t = 0.0f;

    if (std::abs(denom) > EPS)
    {
        // Correct formula for s (parameter on segment v):
        // from Real-Time Collision Detection: s = (a*e - b*d) / denom
        s = (a * e - b * d) / denom;
    }
    else
    {
        // Parallel: choose s by projecting w0 onto v
        s = std::clamp(e / c, 0.0f, 1.0f);
    }

    // Clamp s to [0,1] for segment
    s = std::clamp(s, 0.0f, 1.0f);

    // Given s, compute best t (unconstrained) for infinite line
    t = (b * s - d) / a;

    glm::vec3 c1 = P0 + u * t;
    glm::vec3 c2 = Q0 + v * s;
    float distSq = glm::dot(c1 - c2, c1 - c2);
    return distSq <= r * r;
}
bool LineInf::isColliding(const Cylinder& other) const 
{
    // Mirror of Cylinder::isColliding(LineInf) logic:
    // Check if infinite line intersects finite cylinder volume (axis segment + radius).
    const glm::vec3 A = other.getA();
    const glm::vec3 B = other.getB();
    const float cylR = other.getRadius();

    const glm::vec3 P0 = getA();
    const glm::vec3 P1 = getB();

    const float EPS = 1e-6f;

    // Degenerate line -> treat as point
    glm::vec3 vRaw = P1 - P0;
    float vLen = glm::length(vRaw);
    if (vLen <= EPS)
    {
        // Distance from point A (cylinder axis start) to the point
        // Use projection onto axis segment and radial test.
        // We'll compute distance from point P0 to cylinder volume similarly to Cylinder::ContainsPoint.
        glm::vec3 ab = B - A;
        float abLenSq = glm::dot(ab, ab);
        if (abLenSq == 0.0f)
        {
            glm::vec3 diff = P0 - A;
            float distSq = glm::dot(diff, diff);
            float radiusSum = cylR; // comparing point (radius 0) to cylinder radius
            return distSq <= radiusSum * radiusSum;
        }

        float t = glm::dot(P0 - A, ab) / abLenSq;
        t = std::clamp(t, 0.0f, 1.0f);
        glm::vec3 closest = A + t * ab;
        glm::vec3 diff = P0 - closest;
        return glm::dot(diff, diff) <= cylR * cylR;
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
        float t = (e - b * d) / denom;

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