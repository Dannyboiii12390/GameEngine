#pragma once
#include <glm/glm.hpp>
#include "Collider.h"

namespace Physics
{
    class Plane;
    class Sphere;

	class LineInf : public Collider
    {
    public:
        LineInf(const glm::vec3& a, const glm::vec3& b);

        glm::vec3 getA() const;
        glm::vec3 getB() const;

        // Returns the closest point on the infinite line to `point`.
        glm::vec3 getShortestPathToPoint(const glm::vec3& point) const;

        bool isColliding(const Sphere& s, float length) const;
		bool isColliding(const Sphere& other) const override;
		bool isColliding(const LineInf& other) const override;
		bool isColliding(const Plane& other) const override;
		bool isColliding(const Capsule& other) const override;
		bool isColliding(const Cylinder& other) const override;

		bool isColliding(const Collider& other) const override { return other.isColliding(*this); }

    private:
        glm::vec3 m_a;
        glm::vec3 m_b;
    };
};