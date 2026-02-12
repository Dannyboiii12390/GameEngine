
#include "LineInf.h"
#include "Sphere.h"
#include <glm/glm.hpp>
#include <algorithm>

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

bool LineInf::Intersects(const Sphere& sphere) const
{
    glm::vec3 closest = getShortestPathToPoint(sphere.getPos());
    glm::vec3 diff = sphere.getPos() - closest;
    float distSq = glm::dot(diff, diff);
    float r = sphere.getRadius();
    return distSq <= r * r;
}
bool LineInf::SegmentIntersectsSphere(const Sphere& s, float length)
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