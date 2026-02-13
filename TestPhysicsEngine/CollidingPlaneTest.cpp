#include "pch.h"
#include <glm/glm.hpp>

#include "../PhysicsEngine/Shapes/Plane.h"
#include "../PhysicsEngine/Shapes/Sphere.h"
#include "../PhysicsEngine/Shapes/LineInf.h"
#include "../PhysicsEngine/Shapes/Capsule.h"
#include "../PhysicsEngine/Shapes/Cylinder.h"

namespace Collisions
{
    using namespace Physics;

    // Sphere <-> Plane (symmetric checks)
    TEST(CollidingPlaneTest, Sphere_Plane_NoIntersection)
    {
        Plane plane(glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        Sphere s(glm::vec3(0.0f, 0.0f, 5.0f), 1.0f);

        EXPECT_FALSE(plane.isColliding(s));
        EXPECT_FALSE(s.isColliding(plane));
    }

    TEST(CollidingPlaneTest, Sphere_Plane_Tangent)
    {
        Plane plane(glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        Sphere s(glm::vec3(0.0f, 0.0f, 1.0f), 1.0f);

        EXPECT_TRUE(plane.isColliding(s));
        EXPECT_TRUE(s.isColliding(plane));
    }

    // LineInf <-> Plane
    TEST(CollidingPlaneTest, Line_Plane_Intersecting)
    {
        Plane plane(glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        // Line crossing Z axis through plane
        LineInf line(glm::vec3(0.0f, 0.0f, -5.0f), glm::vec3(0.0f, 0.0f, 5.0f));

        EXPECT_TRUE(plane.isColliding(line));
        EXPECT_TRUE(line.isColliding(plane));
    }

    TEST(CollidingPlaneTest, Line_Plane_ParallelNoIntersection)
    {
        Plane plane(glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        // Line parallel to plane (along X) but offset in Z
        LineInf line(glm::vec3(0.0f, 0.0f, 2.0f), glm::vec3(5.0f, 0.0f, 2.0f));

        EXPECT_FALSE(plane.isColliding(line));
        EXPECT_FALSE(line.isColliding(plane));
    }

    TEST(CollidingPlaneTest, Line_Plane_LineInPlane)
    {
        Plane plane(glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        // Line lies exactly in the plane (Z = 0)
        LineInf line(glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f));

        EXPECT_TRUE(plane.isColliding(line));
        EXPECT_TRUE(line.isColliding(plane));
    }

    // Capsule <-> Plane
    TEST(CollidingPlaneTest, Capsule_Plane_Intersecting)
    {
        Plane plane(glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        Capsule cap(glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 0.0f, 1.0f), 0.5f);

        EXPECT_TRUE(plane.isColliding(cap));
        EXPECT_TRUE(cap.isColliding(plane));
    }

    TEST(CollidingPlaneTest, Capsule_Plane_NoIntersection)
    {
        Plane plane(glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        Capsule cap(glm::vec3(0.0f, 0.0f, 2.0f), glm::vec3(0.0f, 0.0f, 3.0f), 0.5f);

        EXPECT_FALSE(plane.isColliding(cap));
        EXPECT_FALSE(cap.isColliding(plane));
    }

    // Cylinder <-> Plane
    TEST(CollidingPlaneTest, Cylinder_Plane_Intersecting)
    {
        Plane plane(glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        Cylinder cyl(glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 0.0f, 1.0f), 0.5f);

        EXPECT_TRUE(plane.isColliding(cyl));
        EXPECT_TRUE(cyl.isColliding(plane));
    }

    TEST(CollidingPlaneTest, Cylinder_Plane_NoIntersection)
    {
        Plane plane(glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        Cylinder cyl(glm::vec3(0.0f, 0.0f, 2.0f), glm::vec3(0.0f, 0.0f, 3.0f), 0.4f);

        EXPECT_FALSE(plane.isColliding(cyl));
        EXPECT_FALSE(cyl.isColliding(plane));
    }

    // Plane <-> Plane
    TEST(CollidingPlaneTest, Plane_Plane_Intersecting)
    {
        // Two planes with different normals -> intersect along a line
        Plane p1(glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f)); // z=0
        Plane p2(glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)); // y=0 (normals not parallel)

        EXPECT_TRUE(p1.isColliding(p2));
        EXPECT_TRUE(p2.isColliding(p1));
    }

    TEST(CollidingPlaneTest, Plane_Plane_ParallelDistinct)
    {
        Plane p1(glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f)); // z=0
        // parallel normal but offset by z=2 (distinct)
        Plane p2(glm::vec3(0.0f, 0.0f, 2.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

        EXPECT_FALSE(p1.isColliding(p2));
        EXPECT_FALSE(p2.isColliding(p1));
    }

    TEST(CollidingPlaneTest, Plane_Plane_Coincident)
    {
        Plane p1(glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        Plane p2 = Plane::FromThreePoints(glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

        EXPECT_TRUE(p1.isColliding(p2));
        EXPECT_TRUE(p2.isColliding(p1));
    }
}