#pragma once

#include <glm/glm.hpp>
#include "Sphere.h"


namespace Physics
{
	class Sphere;
    // Finite cylinder defined by a line segment (a->b) as the central axis and a radius.
    // ContainsPoint returns true if the point is within the cylinder volume (including end caps).
    class Cylinder
    {
    public:
        Cylinder(const glm::vec3& a, const glm::vec3& b, float radius);

        const glm::vec3& getA() const;
        const glm::vec3& getB() const;
        float getRadius() const;

        // Returns true when point is inside the finite cylinder (inclusive of surface).
        bool ContainsPoint(const glm::vec3& point) const;
		bool Intersects(const Sphere& sphere) const;

    private:
        glm::vec3 m_a;
        glm::vec3 m_b;
        float m_radius;
    };
}