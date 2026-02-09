#pragma once
#include <glm/glm.hpp>

// Forward declare Sphere to avoid circular include in headers.
class Sphere;

class LineInf
{
public:
    LineInf(const glm::vec3& a, const glm::vec3& b);

    glm::vec3 getA() const;
    glm::vec3 getB() const;

    // Returns the closest point on the infinite line to `point`.
    glm::vec3 getShortestPathToPoint(const glm::vec3& point) const;

    // Intersection with sphere (declaration only; implementation in .cpp).
    bool isCollidingWith(const Sphere& sphere) const;

private:
    glm::vec3 m_a;
    glm::vec3 m_b;
};