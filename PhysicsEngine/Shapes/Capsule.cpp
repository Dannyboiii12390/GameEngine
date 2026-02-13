#include "Capsule.h"
#include "Sphere.h"
#include "LineInf.h"
#include "Plane.h" 
#include "Cylinder.h"


#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>

using namespace Physics;

Capsule::Capsule(const glm::vec3& a, const glm::vec3& b, float radius)
	: m_a(a), m_b(b), m_radius(radius), Collider(EColliderType::CAPSULE)
{
}

const glm::vec3& Capsule::getA() const { return m_a; }
const glm::vec3& Capsule::getB() const { return m_b; }
float Capsule::getRadius() const { return m_radius; }

bool Capsule::ContainsPoint(const glm::vec3& point) const
{
    glm::vec3 ab = m_b - m_a;
    float abLenSq = glm::dot(ab, ab);

    const float EPS = 1e-8f;
    if (abLenSq <= EPS)
    {
        glm::vec3 d = point - m_a;
        return glm::dot(d, d) <= m_radius * m_radius;
    }

    float t = glm::dot(point - m_a, ab) / abLenSq;
    t = std::clamp(t, 0.0f, 1.0f);
    glm::vec3 closest = m_a + t * ab;
    glm::vec3 diff = point - closest;
    return glm::dot(diff, diff) <= m_radius * m_radius;
}

// Shortest distance between point p and finite segment [a,b]
static float DistancePointToSegment(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b)
{
    glm::vec3 ab = b - a;
    float ab2 = glm::dot(ab, ab);
    const float EPS = 1e-8f;
    if (ab2 <= EPS)
        return glm::length(p - a);

    float t = glm::dot(p - a, ab) / ab2;
    t = std::clamp(t, 0.0f, 1.0f);
    glm::vec3 closest = a + t * ab;
    return glm::length(p - closest);
}
// Robust shortest distance between two finite segments [p1,q1] and [p2,q2]
static float DistanceSegmentToSegment(const glm::vec3& p1, const glm::vec3& q1, const glm::vec3& p2, const glm::vec3& q2)
{
    const float EPS = 1e-6f;
    glm::vec3   u = q1 - p1;
    glm::vec3   v = q2 - p2;
    glm::vec3   w = p1 - p2;
    float    a = glm::dot(u, u); // squared length of segment S1
    float    b = glm::dot(u, v);
    float    c = glm::dot(v, v); // squared length of segment S2
    float    d = glm::dot(u, w);
    float    e = glm::dot(v, w);
    float    D = a * c - b * b; // always >= 0

    float sc, sN, sD = D; // sc = sN / sD
    float tc, tN, tD = D; // tc = tN / tD

    // compute the line parameters of the two closest points
    if (D < EPS) { // the lines are almost parallel
        sN = 0.0f;         // force using s = 0
        sD = 1.0f;         // to avoid division by 0
        tN = e;
        tD = c;
    }
    else {
        // get the closest points on the infinite lines
        sN = (b * e - c * d);
        tN = (a * e - b * d);
        if (sN < 0.0f) {
            // sc < 0 => the s=0 edge is visible
            sN = 0.0f;
            tN = e;
            tD = c;
        }
        else if (sN > sD) {
            // sc > 1 => the s=1 edge is visible
            sN = sD;
            tN = e + b;
            tD = c;
        }
    }

    // clamp tc to [0,1]
    if (tN < 0.0f) {
        // tc < 0 => the t=0 edge is visible
        tN = 0.0f;
        // recompute sc for this edge
        if (-d < 0.0f)
            sN = 0.0f;
        else if (-d > a)
            sN = sD;
        else {
            sN = -d;
            sD = a;
        }
    }
    else if (tN > tD) {
        // tc > 1 => the t=1 edge is visible
        tN = tD;
        // recompute sc for this edge
        if ((-d + b) < 0.0f)
            sN = 0.0f;
        else if ((-d + b) > a)
            sN = sD;
        else {
            sN = (-d + b);
            sD = a;
        }
    }

    // finally do the division to get sc and tc
    sc = (std::fabs(sN) < EPS ? 0.0f : sN / sD);
    tc = (std::fabs(tN) < EPS ? 0.0f : tN / tD);

    // get the difference of the two closest points
    glm::vec3 dP = w + (sc * u) - (tc * v);  // = S1(sc) - S2(tc)

    return glm::length(dP);
}
// Shortest distance between finite segment [a,b] and infinite line through l0->l1
static float DistanceSegmentToInfiniteLine(const glm::vec3& a, const glm::vec3& b, const glm::vec3& l0, const glm::vec3& l1)
{
    const float EPS = 1e-6f;
    glm::vec3 u = l1 - l0;    // line direction
    glm::vec3 v = b - a;      // segment direction
    glm::vec3 w = l0 - a;
    float a_len2 = glm::dot(u, u); // u·u
    float b_dot = glm::dot(u, v);  // u·v
    float c_len2 = glm::dot(v, v); // v·v
    float d_dot = glm::dot(u, w);  // u·w
    float e_dot = glm::dot(v, w);  // v·w

    float D = a_len2 * c_len2 - b_dot * b_dot;

    // If both directions are near zero (degenerate), fallback to endpoint->line distances
    if (a_len2 < EPS && c_len2 < EPS) {
        // both degenerate to points
        return glm::length(l0 - a);
    }
    // If line direction is near zero (unlikely for LineInf), treat line as point l0
    if (a_len2 < EPS) {
        // distance from best point on segment to point l0
        return DistancePointToSegment(l0, a, b);
    }

    float s = 0.0f; // parameter on line (unbounded)
    float t = 0.0f; // parameter on segment [0,1]

    if (std::fabs(D) > EPS) {
        // general case: compute the closest approach parameters for infinite line and infinite line through segment
        float s_num = (b_dot * e_dot - c_len2 * d_dot);
        float t_num = (a_len2 * e_dot - b_dot * d_dot);
        s = s_num / D;
        t = t_num / D;
    }
    else {
        // almost parallel: choose t by projecting one endpoint
        t = e_dot / c_len2;
        s = 0.0f; // arbitrary
    }

    // clamp t to [0,1]
    t = std::clamp(t, 0.0f, 1.0f);

    // recompute s to be projection of (a + t*v) onto the line: s = dot(u, (a + t*v - l0)) / dot(u,u)
    glm::vec3 ptOnSeg = a + t * v;
    if (a_len2 > EPS)
        s = glm::dot(u, (ptOnSeg - l0)) / a_len2;
    else
        s = 0.0f;

    glm::vec3 closestOnLine = l0 + s * u;
    return glm::length(ptOnSeg - closestOnLine);
}

bool Capsule::isColliding(const Sphere& other) const
{
    // Minimal distance from sphere center to capsule axis segment
    float dist = DistancePointToSegment(other.getPos(), m_a, m_b);
    return dist <= (m_radius + other.getRadius());
}
bool Capsule::isColliding(const LineInf& other) const 
{
    // Use the infinite line defined by other.getA() -> other.getB()
    glm::vec3 l0 = other.getA();
    glm::vec3 l1 = other.getB();
    float dist = DistanceSegmentToInfiniteLine(m_a, m_b, l0, l1);
    return dist <= m_radius;
}
bool Capsule::isColliding(const Capsule& other) const
{
    // Compute squared distance between two segments (from Real-Time Collision Detection)
    auto segSegDistSq = [](const glm::vec3& p1, const glm::vec3& q1, const glm::vec3& p2, const glm::vec3& q2) -> float
        {
            const float EPS = 1e-8f;
            glm::vec3 d1 = q1 - p1;
            glm::vec3 d2 = q2 - p2;
            glm::vec3 r = p1 - p2;
            float a = glm::dot(d1, d1);
            float e = glm::dot(d2, d2);
            float f = glm::dot(d2, r);

            // both degenerate to points
            if (a <= EPS && e <= EPS)
            {
                glm::vec3 diff = p1 - p2;
                return glm::dot(diff, diff);
            }
            // first degenerate -> point-segment
            if (a <= EPS)
            {
                float t = std::clamp(f / e, 0.0f, 1.0f);
                glm::vec3 c2 = p2 + d2 * t;
                glm::vec3 diff = p1 - c2;
                return glm::dot(diff, diff);
            }
            // second degenerate -> segment-point
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
            if (denom != 0.0f)
                s = std::clamp((b * f - c * e) / denom, 0.0f, 1.0f);
            else
                s = 0.0f; // parallel case - pick s = 0 and solve for t

            float t = (b * s + f) / e;

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

    float distSq = segSegDistSq(m_a, m_b, other.m_a, other.m_b);
    float radiusSum = m_radius + other.m_radius;
    return distSq <= radiusSum * radiusSum;
}

bool Capsule::isColliding(const Cylinder& other) const 
{
    // Treat cylinder axis as a finite segment; check axis-to-axis minimal distance against sum of radii.
    // Assumes Cylinder exposes m_a, m_b and getRadius() (as per project signatures).
    float dist = DistanceSegmentToSegment(m_a, m_b, other.getA(), other.getB());
    return dist <= (m_radius + other.getRadius());
}
bool Capsule::isColliding(const Plane& other) const 
{
    // Compute signed distances of capsule segment endpoints to the plane
    float dA = other.signedDistance(m_a);
    float dB = other.signedDistance(m_b);

    // If segment crosses plane, there is intersection (distance 0)
    if (dA * dB < 0.0f)
        return true;

    // Otherwise, check if either endpoint is within radius distance to plane
    if (std::fabs(dA) <= m_radius) return true;
    if (std::fabs(dB) <= m_radius) return true;

    return false;
}