#pragma once
#include <glm/glm.hpp>
#include "Collider.h"   
#include <string>
#include <sstream>

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

        void setPosition(const glm::vec3& newPos) override;
        void setRotation(const glm::vec3& newRot) override;

        std::string toString()
        {
            std::stringstream ss;
            ss << "Point: " << vectoString(m_point) << "\n";
            ss << "m_u: " << vectoString(m_u) << "\n";
            ss << "m_v: " << vectoString(m_v) << "\n";
            ss << "m_normal: " << vectoString(m_normal) << "\n";
            ss << "m_unitNormal: " << vectoString(m_unitNormal) << "\n";
            ss << "m_basePoint: " << vectoString(m_basePoint) << "\n";
            ss << "m_baseU: " << vectoString(m_baseU) << "\n";
            ss << "m_baseV: " << vectoString(m_baseV);
            return ss.str();
        }
    private:
        glm::vec3 m_point;

        glm::vec3 m_u;
        glm::vec3 m_v;

        glm::vec3 m_normal;
        glm::vec3 m_unitNormal;

		glm::vec3 m_basePoint;
		glm::vec3 m_baseU;
		glm::vec3 m_baseV;

        std::string vectoString(const glm::vec3& v) const
        {
            ::std::stringstream ss;
            ss << "(" << v.x << ", " << v.y << ", " << v.z << ")";
            return ss.str();
        }
    };
};