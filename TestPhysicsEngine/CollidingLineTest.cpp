#include "pch.h"
#include <glm/glm.hpp>

#include "../PhysicsEngine/Shapes/LineInf.h"
#include "../PhysicsEngine/Shapes/Sphere.h"
#include "../PhysicsEngine/Shapes/Capsule.h"
#include "../PhysicsEngine/Shapes/Cylinder.h"
#include "../PhysicsEngine/Shapes/Plane.h"

namespace Collisions
{
    using namespace Physics;

    // --- LineInf <-> Sphere ---
    TEST(CollidingLineTest, Line_Sphere_Intersecting)
    {
        // Line through origin along X
        LineInf line(glm::vec3(-5.0f, 0.0f, 0.0f), glm::vec3(5.0f, 0.0f, 0.0f));
        Sphere s(glm::vec3(0.0f, 0.0f, 0.0f), 1.0f);

        EXPECT_TRUE(line.isColliding(s));
        EXPECT_TRUE(s.isColliding(line));
    }

    TEST(CollidingLineTest, Line_Sphere_NotIntersecting)
    {
        LineInf line(glm::vec3(-5.0f, 5.0f, 0.0f), glm::vec3(5.0f, 5.0f, 0.0f)); // offset in Y
        Sphere s(glm::vec3(0.0f, 0.0f, 0.0f), 1.0f);

        EXPECT_FALSE(line.isColliding(s));
        EXPECT_FALSE(s.isColliding(line));
    }

    TEST(CollidingLineTest, Line_Sphere_LineStartsInside)
    {
        LineInf line(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f)); // passes from center outward
        Sphere s(glm::vec3(0.0f, 0.0f, 0.0f), 0.5f);

        EXPECT_TRUE(line.isColliding(s));
        EXPECT_TRUE(s.isColliding(line));
    }

    // --- LineInf <-> LineInf ---
    TEST(CollidingLineTest, Line_Line_Intersecting)
    {
        // X axis and Z axis crossing at origin (coplanar intersection)
        LineInf a(glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        LineInf b(glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 0.0f, 1.0f));

        EXPECT_TRUE(a.isColliding(b));
        EXPECT_TRUE(b.isColliding(a));
    }

    TEST(CollidingLineTest, Line_Line_ParallelNoIntersection)
    {
        // Parallel lines separated in Y
        LineInf a(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        LineInf b(glm::vec3(0.0f, 2.0f, 0.0f), glm::vec3(1.0f, 2.0f, 0.0f));

        EXPECT_FALSE(a.isColliding(b));
        EXPECT_FALSE(b.isColliding(a));
    }

    TEST(CollidingLineTest, Line_Line_Coincident)
    {
        LineInf a(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(2.0f, 0.0f, 0.0f));
        LineInf b(glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(3.0f, 0.0f, 0.0f)); // collinear with a

        EXPECT_TRUE(a.isColliding(b));
        EXPECT_TRUE(b.isColliding(a));
    }

    // --- LineInf <-> Capsule ---
    TEST(CollidingLineTest, Line_Capsule_Intersecting)
    {
        // Capsule along X from 0..10, radius 1; line at x=5 passes through
        Capsule cap(glm::vec3(0.0f), glm::vec3(10.0f, 0.0f, 0.0f), 1.0f);
        LineInf line(glm::vec3(5.0f, -5.0f, 0.0f), glm::vec3(5.0f, 5.0f, 0.0f));

        EXPECT_TRUE(line.isColliding(cap));
        EXPECT_TRUE(cap.isColliding(line));
    }

    TEST(CollidingLineTest, Line_Capsule_NotIntersecting)
    {
        Capsule cap(glm::vec3(0.0f), glm::vec3(10.0f, 0.0f, 0.0f), 0.5f);
        LineInf line(glm::vec3(5.0f, 2.0f, 0.0f), glm::vec3(6.0f, 2.0f, 0.0f)); // offset greater than radius

        EXPECT_FALSE(line.isColliding(cap));
        EXPECT_FALSE(cap.isColliding(line));
    }

    // --- LineInf <-> Cylinder ---
    TEST(CollidingLineTest, Line_Cylinder_Intersecting)
    {
        Cylinder cyl(glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 0.0f, 1.0f), 1.0f); // axis along Z
        LineInf line(glm::vec3(-5.0f, 0.0f, 0.0f), glm::vec3(5.0f, 0.0f, 0.0f)); // passes through axis

        EXPECT_TRUE(line.isColliding(cyl));
        EXPECT_TRUE(cyl.isColliding(line));
    }

    TEST(CollidingLineTest, Line_Cylinder_NotIntersecting)
    {
        Cylinder cyl(glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 0.0f, 1.0f), 0.5f);
        LineInf line(glm::vec3(0.0f, 2.0f, 0.0f), glm::vec3(1.0f, 2.0f, 0.0f)); // offset in Y

        EXPECT_FALSE(line.isColliding(cyl));
        EXPECT_FALSE(cyl.isColliding(line));
    }

    // --- LineInf <-> Plane ---
    TEST(CollidingLineTest, Line_Plane_Intersecting)
    {
        Plane plane(glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f)); // z=0
        LineInf line(glm::vec3(0.0f, 0.0f, -5.0f), glm::vec3(0.0f, 0.0f, 5.0f)); // crosses plane

        EXPECT_TRUE(line.isColliding(plane));
        EXPECT_TRUE(plane.isColliding(line));
    }

    TEST(CollidingLineTest, Line_Plane_ParallelNoIntersection)
    {
        Plane plane(glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f)); // z=0
        LineInf line(glm::vec3(0.0f, 0.0f, 2.0f), glm::vec3(5.0f, 0.0f, 2.0f)); // parallel, z=2

        EXPECT_FALSE(line.isColliding(plane));
        EXPECT_FALSE(plane.isColliding(line));
    }

    TEST(CollidingLineTest, Line_Plane_LineInPlane)
    {
        Plane plane(glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f)); // z=0
        LineInf line(glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f)); // lies in plane

        EXPECT_TRUE(line.isColliding(plane));
        EXPECT_TRUE(plane.isColliding(line));
    }
}
