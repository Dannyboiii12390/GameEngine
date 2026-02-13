#include "pch.h"
#include <glm/glm.hpp>

#include "../PhysicsEngine/Shapes/Cylinder.h"
#include "../PhysicsEngine/Shapes/Sphere.h"
#include "../PhysicsEngine/Shapes/LineInf.h"
#include "../PhysicsEngine/Shapes/Capsule.h"
#include "../PhysicsEngine/Shapes/Plane.h"

namespace Collisions
{
    using namespace Physics;

    // --- Cylinder <-> Sphere ---
    TEST(CollidingCylinderTest, Cylinder_Sphere_Intersecting)
    {
        Cylinder cyl(glm::vec3(0.0f), glm::vec3(10.0f, 0.0f, 0.0f), 2.0f);
        Sphere s(glm::vec3(5.0f, 1.0f, 0.0f), 0.5f); // projects to middle, inside radial distance

        EXPECT_TRUE(cyl.isColliding(s));
        EXPECT_TRUE(s.isColliding(cyl));
    }

    TEST(CollidingCylinderTest, Cylinder_Sphere_NotIntersecting)
    {
        Cylinder cyl(glm::vec3(0.0f), glm::vec3(10.0f, 0.0f, 0.0f), 1.0f);
        Sphere s(glm::vec3(5.0f, 3.5f, 0.0f), 0.5f); // too far radially

        EXPECT_FALSE(cyl.isColliding(s));
        EXPECT_FALSE(s.isColliding(cyl));
    }

    // --- Cylinder <-> LineInf ---
    TEST(CollidingCylinderTest, Cylinder_Line_Intersecting)
    {
        Cylinder cyl(glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 0.0f, 1.0f), 1.0f);
        LineInf line(glm::vec3(-5.0f, 0.0f, 0.0f), glm::vec3(5.0f, 0.0f, 0.0f)); // passes through axis

        EXPECT_TRUE(cyl.isColliding(line));
        EXPECT_TRUE(line.isColliding(cyl));
    }

    TEST(CollidingCylinderTest, Cylinder_Line_NotIntersecting)
    {
        Cylinder cyl(glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 0.0f, 1.0f), 0.5f);
        LineInf line(glm::vec3(5.0f, 5.0f, 0.0f), glm::vec3(6.0f, 5.0f, 0.0f)); // far away, parallel to X

        EXPECT_FALSE(cyl.isColliding(line));
        EXPECT_FALSE(line.isColliding(cyl));
    }

    // --- Cylinder <-> Capsule ---
    TEST(CollidingCylinderTest, Cylinder_Capsule_Intersecting)
    {
        Cylinder cyl(glm::vec3(0.0f), glm::vec3(10.0f, 0.0f, 0.0f), 1.0f);
        Capsule cap(glm::vec3(5.0f, 0.5f, 0.0f), glm::vec3(15.0f, 0.5f, 0.0f), 0.6f); // overlapping region near x=5..10

        EXPECT_TRUE(cyl.isColliding(cap));
        EXPECT_TRUE(cap.isColliding(cyl));
    }

    TEST(CollidingCylinderTest, Cylinder_Capsule_NotIntersecting)
    {
        Cylinder cyl(glm::vec3(0.0f), glm::vec3(10.0f, 0.0f, 0.0f), 0.5f);
        Capsule cap(glm::vec3(5.0f, 3.0f, 0.0f), glm::vec3(15.0f, 3.0f, 0.0f), 0.5f); // offset in Y

        EXPECT_FALSE(cyl.isColliding(cap));
        EXPECT_FALSE(cap.isColliding(cyl));
    }

    // --- Cylinder <-> Cylinder ---
    TEST(CollidingCylinderTest, Cylinder_Cylinder_Intersecting)
    {
        Cylinder a(glm::vec3(0.0f), glm::vec3(10.0f, 0.0f, 0.0f), 1.0f);
        Cylinder b(glm::vec3(5.0f, 0.5f, 0.0f), glm::vec3(15.0f, 0.5f, 0.0f), 1.0f);

        EXPECT_TRUE(a.isColliding(b));
        EXPECT_TRUE(b.isColliding(a));
    }

    TEST(CollidingCylinderTest, Cylinder_Cylinder_NotIntersecting)
    {
        Cylinder a(glm::vec3(0.0f), glm::vec3(10.0f, 0.0f, 0.0f), 0.5f);
        Cylinder b(glm::vec3(5.0f, 3.0f, 0.0f), glm::vec3(15.0f, 3.0f, 0.0f), 0.5f);

        EXPECT_FALSE(a.isColliding(b));
        EXPECT_FALSE(b.isColliding(a));
    }

    // --- Cylinder <-> Plane ---
    TEST(CollidingCylinderTest, Cylinder_Plane_Intersecting)
    {
        Plane plane(glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f)); // z=0
        Cylinder cyl(glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 0.0f, 1.0f), 0.5f); // crosses plane

        EXPECT_TRUE(cyl.isColliding(plane));
        EXPECT_TRUE(plane.isColliding(cyl));
    }

    TEST(CollidingCylinderTest, Cylinder_Plane_NoIntersection)
    {
        Plane plane(glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f)); // z=0
        Cylinder cyl(glm::vec3(0.0f, 0.0f, 2.0f), glm::vec3(0.0f, 0.0f, 3.0f), 0.4f); // entirely above and farther than radius

        EXPECT_FALSE(cyl.isColliding(plane));
        EXPECT_FALSE(plane.isColliding(cyl));
    }
}