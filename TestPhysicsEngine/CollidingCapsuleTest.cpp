#include "pch.h"
#include <glm/glm.hpp>

#include "../PhysicsEngine/Shapes/Capsule.h"
#include "../PhysicsEngine/Shapes/Sphere.h"
#include "../PhysicsEngine/Shapes/LineInf.h"
#include "../PhysicsEngine/Shapes/Cylinder.h"
#include "../PhysicsEngine/Shapes/Plane.h"

namespace Collisions
{
    using namespace Physics;

    // --- Capsule <-> Sphere ---
    TEST(CollidingCapsuleTest, Capsule_Sphere_Intersecting)
    {
        Capsule cap(glm::vec3(0.0f), glm::vec3(10.0f, 0.0f, 0.0f), 2.0f);
        Sphere s(glm::vec3(5.0f, 1.0f, 0.0f), 0.5f);

        EXPECT_TRUE(cap.isColliding(s));
        EXPECT_TRUE(s.isColliding(cap));
    }

    TEST(CollidingCapsuleTest, Capsule_Sphere_NotIntersecting)
    {
        Capsule cap(glm::vec3(0.0f), glm::vec3(10.0f, 0.0f, 0.0f), 1.0f);
        Sphere s(glm::vec3(5.0f, 3.5f, 0.0f), 0.5f);

        EXPECT_FALSE(cap.isColliding(s));
        EXPECT_FALSE(s.isColliding(cap));
    }

    // --- Capsule <-> LineInf ---
    TEST(CollidingCapsuleTest, Capsule_Line_Intersecting)
    {
        Capsule cap(glm::vec3(0.0f), glm::vec3(10.0f, 0.0f, 0.0f), 2.0f);
        // Infinite line passing through x=5 (crosses capsule axis)
        LineInf line(glm::vec3(5.0f, 5.0f, 0.0f), glm::vec3(5.0f, -5.0f, 0.0f));

        EXPECT_TRUE(cap.isColliding(line));
        EXPECT_TRUE(line.isColliding(cap));
    }

    TEST(CollidingCapsuleTest, Capsule_Line_NotIntersecting)
    {
        Capsule cap(glm::vec3(0.0f), glm::vec3(10.0f, 0.0f, 0.0f), 1.0f);
        // Line well to the side at x=20
        LineInf line(glm::vec3(20.0f, 0.0f, 0.0f), glm::vec3(20.0f, 1.0f, 0.0f));

        EXPECT_FALSE(cap.isColliding(line));
        EXPECT_FALSE(line.isColliding(cap));
    }

    // --- Capsule <-> Capsule ---
    TEST(CollidingCapsuleTest, Capsule_Capsule_Intersecting)
    {
        Capsule a(glm::vec3(0.0f), glm::vec3(10.0f, 0.0f, 0.0f), 1.0f);
        Capsule b(glm::vec3(5.0f, 0.0f, 0.0f), glm::vec3(15.0f, 0.0f, 0.0f), 1.0f);

        EXPECT_TRUE(a.isColliding(b));
        EXPECT_TRUE(b.isColliding(a));
    }

    TEST(CollidingCapsuleTest, Capsule_Capsule_NotIntersecting)
    {
        Capsule a(glm::vec3(0.0f), glm::vec3(10.0f, 0.0f, 0.0f), 0.5f);
        // Parallel but offset in Y so radii don't reach
        Capsule b(glm::vec3(5.0f, 3.0f, 0.0f), glm::vec3(15.0f, 3.0f, 0.0f), 0.5f);

        EXPECT_FALSE(a.isColliding(b));
        EXPECT_FALSE(b.isColliding(a));
    }

    // --- Capsule <-> Cylinder ---
    TEST(CollidingCapsuleTest, Capsule_Cylinder_Intersecting)
    {
        Capsule cap(glm::vec3(0.0f), glm::vec3(10.0f, 0.0f, 0.0f), 1.0f);
        Cylinder cyl(glm::vec3(5.0f, 0.0f, 0.0f), glm::vec3(15.0f, 0.0f, 0.0f), 1.0f);

        EXPECT_TRUE(cap.isColliding(cyl));
        EXPECT_TRUE(cyl.isColliding(cap));
    }

    TEST(CollidingCapsuleTest, Capsule_Cylinder_NotIntersecting)
    {
        Capsule cap(glm::vec3(0.0f), glm::vec3(10.0f, 0.0f, 0.0f), 0.5f);
        Cylinder cyl(glm::vec3(5.0f, 3.0f, 0.0f), glm::vec3(15.0f, 3.0f, 0.0f), 0.5f);

        EXPECT_FALSE(cap.isColliding(cyl));
        EXPECT_FALSE(cyl.isColliding(cap));
    }

    // --- Capsule <-> Plane ---
    TEST(CollidingCapsuleTest, Capsule_Plane_Intersecting)
    {
        // XY plane (normal +Z)
        Plane plane(glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(1.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f));

        // Capsule axis crosses the plane (z from -1 to +1)
        Capsule cap(glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 0.0f, 1.0f), 0.5f);

        EXPECT_TRUE(cap.isColliding(plane));
        EXPECT_TRUE(plane.isColliding(cap));
    }

    TEST(CollidingCapsuleTest, Capsule_Plane_NotIntersecting)
    {
        Plane plane(glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(1.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f));

        // Capsule entirely above plane and farther than radius
        Capsule cap(glm::vec3(0.0f, 0.0f, 2.0f), glm::vec3(0.0f, 0.0f, 3.0f), 0.5f);

        EXPECT_FALSE(cap.isColliding(plane));
        EXPECT_FALSE(plane.isColliding(cap));
    }
}