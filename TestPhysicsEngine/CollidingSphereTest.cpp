#include "pch.h"
#include <glm/glm.hpp>

#include "../PhysicsEngine/Shapes/Sphere.h"
#include "../PhysicsEngine/Shapes/LineInf.h"
#include "../PhysicsEngine/Shapes/Capsule.h"
#include "../PhysicsEngine/Shapes/Cylinder.h"
#include "../PhysicsEngine/Shapes/Plane.h"

namespace Collisions
{
    using namespace Physics;

    // --- Sphere <-> Sphere ---
    TEST(CollidingSphereTest, Sphere_Sphere_Intersecting)
    {
        Sphere a(glm::vec3(0.0f), 2.0f);
        Sphere b(glm::vec3(3.0f, 0.0f, 0.0f), 2.0f); // distance = 3, sum radii = 4

        EXPECT_TRUE(a.isColliding(b));
        EXPECT_TRUE(b.isColliding(a));
    }

    TEST(CollidingSphereTest, Sphere_Sphere_NotIntersecting)
    {
        Sphere a(glm::vec3(0.0f), 1.0f);
        Sphere b(glm::vec3(5.0f, 0.0f, 0.0f), 1.0f); // distance = 5, sum radii = 2

        EXPECT_FALSE(a.isColliding(b));
        EXPECT_FALSE(b.isColliding(a));
    }

    TEST(CollidingSphereTest, Sphere_Sphere_Tangent)
    {
        Sphere a(glm::vec3(0.0f), 1.5f);
        Sphere b(glm::vec3(3.0f, 0.0f, 0.0f), 1.5f); // distance = 3, sum radii = 3 -> tangent

        EXPECT_TRUE(a.isColliding(b));
        EXPECT_TRUE(b.isColliding(a));
    }

    // --- Sphere <-> LineInf ---
    TEST(CollidingSphereTest, Sphere_Line_PassesThrough)
    {
        Sphere s(glm::vec3(0.0f), 1.0f);
        LineInf line(glm::vec3(-5.0f, 0.0f, 0.0f), glm::vec3(5.0f, 0.0f, 0.0f)); // passes through center

        EXPECT_TRUE(s.isColliding(line));
        EXPECT_TRUE(line.isColliding(s));
    }

    TEST(CollidingSphereTest, Sphere_Line_NoIntersection)
    {
        Sphere s(glm::vec3(0.0f), 1.0f);
        LineInf line(glm::vec3(-5.0f, 5.0f, 0.0f), glm::vec3(5.0f, 5.0f, 0.0f)); // offset by 5 in Y

        EXPECT_FALSE(s.isColliding(line));
        EXPECT_FALSE(line.isColliding(s));
    }

    TEST(CollidingSphereTest, Sphere_Line_Tangent)
    {
        Sphere s(glm::vec3(0.0f), 1.0f);
        // line at y = 1 (tangent to unit sphere at y=1)
        LineInf line(glm::vec3(-5.0f, 1.0f, 0.0f), glm::vec3(5.0f, 1.0f, 0.0f));

        EXPECT_TRUE(s.isColliding(line));
        EXPECT_TRUE(line.isColliding(s));
    }

    // --- Sphere <-> Capsule ---
    TEST(CollidingSphereTest, Sphere_Capsule_Intersecting)
    {
        Capsule cap(glm::vec3(0.0f), glm::vec3(10.0f, 0.0f, 0.0f), 1.0f);
        Sphere s(glm::vec3(5.0f, 0.5f, 0.0f), 0.6f); // projects to middle within radius+sphere

        EXPECT_TRUE(s.isColliding(cap));
        EXPECT_TRUE(cap.isColliding(s));
    }

    TEST(CollidingSphereTest, Sphere_Capsule_NotIntersecting)
    {
        Capsule cap(glm::vec3(0.0f), glm::vec3(10.0f, 0.0f, 0.0f), 0.5f);
        Sphere s(glm::vec3(5.0f, 3.0f, 0.0f), 0.4f); // too far radially

        EXPECT_FALSE(s.isColliding(cap));
        EXPECT_FALSE(cap.isColliding(s));
    }

    // --- Sphere <-> Cylinder ---
    TEST(CollidingSphereTest, Sphere_Cylinder_Intersecting)
    {
        Cylinder cyl(glm::vec3(0.0f), glm::vec3(10.0f, 0.0f, 0.0f), 2.0f);
        Sphere s(glm::vec3(5.0f, 1.0f, 0.0f), 0.5f); // projects to middle, inside radial distance

        EXPECT_TRUE(s.isColliding(cyl));
        EXPECT_TRUE(cyl.isColliding(s));
    }

    TEST(CollidingSphereTest, Sphere_Cylinder_NotIntersecting)
    {
        Cylinder cyl(glm::vec3(0.0f), glm::vec3(10.0f, 0.0f, 0.0f), 1.0f);
        Sphere s(glm::vec3(5.0f, 3.5f, 0.0f), 0.5f); // too far radially

        EXPECT_FALSE(s.isColliding(cyl));
        EXPECT_FALSE(cyl.isColliding(s));
    }

    // --- Sphere <-> Plane ---
    TEST(CollidingSphereTest, Sphere_Plane_NoIntersection)
    {
        Plane plane(glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f)); // z=0
        Sphere s(glm::vec3(0.0f, 0.0f, 5.0f), 1.0f);

        EXPECT_FALSE(s.isColliding(plane));
        EXPECT_FALSE(plane.isColliding(s));
    }

    TEST(CollidingSphereTest, Sphere_Plane_Tangent)
    {
        Plane plane(glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        Sphere s(glm::vec3(0.0f, 0.0f, 1.0f), 1.0f); // tangent at z=0

        EXPECT_TRUE(s.isColliding(plane));
        EXPECT_TRUE(plane.isColliding(s));
    }

    TEST(CollidingSphereTest, Sphere_Plane_Intersecting)
    {
        Plane plane(glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        Sphere s(glm::vec3(0.0f, 0.0f, 0.5f), 1.0f); // center within radius distance

        EXPECT_TRUE(s.isColliding(plane));
        EXPECT_TRUE(plane.isColliding(s));
    }
}