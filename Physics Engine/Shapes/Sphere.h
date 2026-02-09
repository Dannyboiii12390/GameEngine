#pragma once
#include <glm/glm.hpp>

// Forward-declare LineInf to avoid circular includes in headers.
class LineInf;
class Plane;

class Sphere
{
public:
    Sphere(const glm::vec3& pos, float radius);

    const glm::vec3& getPos() const;
    float getRadius() const;

    bool isCollidingWith(const Sphere& other) const;
    bool isCollidingWith(const glm::vec3& point) const;
    bool isCollidingWith(const LineInf& line) const;
	bool isCollidingWith(const Plane& plane) const;

private:
    glm::vec3 m_pos;
    float m_radius;
};