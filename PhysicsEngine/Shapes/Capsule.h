#pragma once
#include <glm/glm.hpp>

#include "Collider.h"

namespace Physics
{
    class Sphere;

    // Capsule: finite line segment [a,b] with radius r. The capsule volume is all points
    // whose distance to the segment is <= r (this naturally includes spherical end-caps).
	class Capsule : public Collider
    {
    public:
        Capsule(const glm::vec3& a, const glm::vec3& b, float radius);

        const glm::vec3& getA() const;
        const glm::vec3& getB() const;
        float getRadius() const;

        // True when point lies inside capsule (inclusive of surface).
        bool ContainsPoint(const glm::vec3& point) const;

		bool isColliding(const Sphere& other) const override; 
        bool isColliding(const LineInf& other) const override;
        bool isColliding(const Capsule& other) const override;
        bool isColliding(const Cylinder& other) const override;
        bool isColliding(const Plane& other) const override;

		bool isColliding(const Collider& other) const override { return other.isColliding(*this); }


    private:
        glm::vec3 m_a;
        glm::vec3 m_b;
        float m_radius;
    };
}
