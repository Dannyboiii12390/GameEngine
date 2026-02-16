#pragma once
#include <glm/glm.hpp>
#include "Collider.h"


namespace Physics
{
    // Forward-declare LineInf to avoid circular includes in headers.

    class LineInf;
    class Plane;
	class Cylinder;
    class Capsule;

	class Sphere : public Collider
    {
    public:
        Sphere(const glm::vec3& pos, float radius);

        const glm::vec3& getPos() const;
        float getRadius() const;
        
		bool isColliding(const Sphere& other) const override; 
        bool isColliding(const LineInf& other) const override; 
        bool isColliding(const Capsule& other) const override;
        bool isColliding(const Cylinder& other) const override;
        bool isColliding(const Plane& other) const override;
		void setPosition(const glm::vec3& newPos) override { m_pos = newPos; }

    private:
        glm::vec3 m_pos;
        float m_radius;
    };
};