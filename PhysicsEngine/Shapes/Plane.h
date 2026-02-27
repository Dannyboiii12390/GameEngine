#pragma once
#include <glm/glm.hpp>
#include "Collider.h"   

namespace Physics
{
	class Plane : public Collider
    {
    public:
        Plane(const glm::vec3& point, const glm::vec3& u, const glm::vec3& v);

        static Plane FromThreePoints(const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2);

        glm::vec3 pointAt(float s, float t) const;

        // Signed distance from point to plane
        float signedDistance(const glm::vec3& p) const;
        // Project a point onto the plane
        glm::vec3 projectPoint(const glm::vec3& p) const;

        // Check if point lies on plane
        bool containsPoint(const glm::vec3& p, float epsilon = 1e-5f) const;
        float getShortestDistance(const glm::vec3& p) const;

        bool isColliding(const Sphere& other) const override;
		bool isColliding(const LineInf& other) const override;
        bool isColliding(const Capsule& other) const override;
        bool isColliding(const Cylinder& other) const override;
		bool isColliding(const Plane& other) const override;

		bool isColliding(const Collider& other) const override { return other.isColliding(*this); }

    private:
        glm::vec3 m_point;

        glm::vec3 m_u;
        glm::vec3 m_v;

        glm::vec3 m_normal;
        glm::vec3 m_unitNormal;
    };
};