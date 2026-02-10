#include "pch.h"
#include <glm/glm.hpp>
#include "../Physics Engine/Shapes/Sphere.h"
#include "../Physics Engine/Shapes/Plane.h"

namespace ShapeTests
{
    using namespace Physics;

    TEST(SpherePlaneTest, NoIntersection_DistanceGreaterThanRadius)
    {
        Plane plane(glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(1.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f)); // XY plane, normal +Z

        Sphere s(glm::vec3(0.0f, 0.0f, 5.0f), 1.0f);

        EXPECT_FALSE(s.isCollidingWith(plane));
    }

    TEST(SpherePlaneTest, Tangent_DistanceEqualsRadius)
    {
        Plane plane(glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(1.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f)); // XY plane, normal +Z

        Sphere s(glm::vec3(0.0f, 0.0f, 1.0f), 1.0f); // center at z=1, radius=1 -> tangent

        EXPECT_TRUE(s.isCollidingWith(plane));
    }

    TEST(SpherePlaneTest, Intersecting_DistanceLessThanRadius)
    {
        Plane plane(glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(1.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f)); // XY plane, normal +Z

        Sphere s(glm::vec3(0.0f, 0.0f, 0.5f), 1.0f); // center at z=0.5, radius=1 -> intersects

        EXPECT_TRUE(s.isCollidingWith(plane));
    }

    TEST(SpherePlaneTest, PlaneWithFlippedNormal_StillDetectsCollision)
    {
        // Swap u and v to flip the normal sign; distance check should use absolute distance.
        Plane plane(glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f),
            glm::vec3(1.0f, 0.0f, 0.0f)); // normal -Z

        Sphere s(glm::vec3(0.0f, 0.0f, -0.5f), 1.0f); // center below plane but within radius

        EXPECT_TRUE(s.isCollidingWith(plane));
    }
}