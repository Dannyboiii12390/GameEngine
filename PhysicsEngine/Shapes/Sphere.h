#pragma once
#include <glm/glm.hpp>

namespace Physics
{
    // Forward-declare LineInf to avoid circular includes in headers.
    class LineInf;
    class Plane;
	class Cylinder;

    class Sphere
    {
    public:
        Sphere(const glm::vec3& pos, float radius);

        const glm::vec3& getPos() const;
        float getRadius() const;

        bool Intersects(const Sphere& other) const;
        bool Intersects(const glm::vec3& point) const;
        bool Intersects(const LineInf& line) const;
        bool Intersects(const Plane& plane) const;
		bool Intersects(const Cylinder& cyl) const;

    private:
        glm::vec3 m_pos;
        float m_radius;
    };
};