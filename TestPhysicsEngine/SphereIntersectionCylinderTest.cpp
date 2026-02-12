#include "pch.h"
#include <glm/glm.hpp>
#include <algorithm>
#include "../PhysicsEngine/Shapes/Sphere.h"
#include "../PhysicsEngine/Shapes/Cylinder.h"

namespace ShapeTests
{
    using namespace Physics;

    TEST(SphereCylinderIntersectionTest, SphereInside_MiddleOfSegment)
    {
        glm::vec3 a(0.0f, 0.0f, 0.0f);
        glm::vec3 b(10.0f, 0.0f, 0.0f);
        float cylR = 2.0f;

		Cylinder cyl(a, b, cylR);
        Sphere s(glm::vec3(5.0f, 1.0f, 0.0f), 0.5f); // center projects to middle, perpDist=1 <= cylR -> inside
        EXPECT_TRUE(s.isColliding(cyl));
        EXPECT_TRUE(cyl.isColliding(s));
    }

    TEST(SphereCylinderIntersectionTest, SphereOutsideAtMiddleOfSegment)
    {
        glm::vec3 a(0.0f, 0.0f, 0.0f);
        glm::vec3 b(10.0f, 0.0f, 0.0f);
        float cylR = 2.0f;

		Cylinder cyl(a, b, cylR);
        Sphere s(glm::vec3(5.0f, 3.5f, 0.0f), 0.5f); // perpDist = 3.5 -> 3.5 - 2.0 = 1.5 > sphere radius -> no intersection
        EXPECT_FALSE(s.isColliding(cyl));
        EXPECT_FALSE(cyl.isColliding(s));
    }

    TEST(SphereCylinderIntersectionTest, SphereInside_IntersectingAtStart)
    {
        glm::vec3 a(0.0f, 0.0f, 0.0f);
        glm::vec3 b(10.0f, 0.0f, 0.0f);
        float cylR = 1.0f;

        // center inside cylinder volume near the start cap
		Cylinder cyl(a, b, cylR);
        Sphere s(glm::vec3(0.5f, 0.0f, 0.0f), 0.6f);
        EXPECT_TRUE(s.isColliding(cyl));
        EXPECT_TRUE(cyl.isColliding(s));
    }

    TEST(SphereCylinderIntersectionTest, SphereOutside_ButIntersectingAtStart)
    {
        glm::vec3 a(0.0f, 0.0f, 0.0f);
        glm::vec3 b(10.0f, 0.0f, 0.0f);
        float cylR = 1.0f;

        // center just before start but sphere radius reaches into the cap -> intersects
		Cylinder cyl(a, b, cylR);
        Sphere s(glm::vec3(-0.5f, 0.0f, 0.0f), 0.6f);
        EXPECT_TRUE(s.isColliding(cyl));
        EXPECT_TRUE(cyl.isColliding(s));
    }

    TEST(SphereCylinderIntersectionTest, SphereInside_IntersectingAtEnd)
    {
        glm::vec3 a(0.0f, 0.0f, 0.0f);
        glm::vec3 b(10.0f, 0.0f, 0.0f);
        float cylR = 1.0f;

        // center inside near the end cap
		Cylinder cyl(a, b, cylR);
        Sphere s(glm::vec3(10.0f, 0.5f, 0.0f), 0.4f);
        EXPECT_TRUE(s.isColliding(cyl));
        EXPECT_TRUE(cyl.isColliding(s));
    }

    TEST(SphereCylinderIntersectionTest, SphereOutside_ButIntersectingAtEnd)
    {
        glm::vec3 a(0.0f, 0.0f, 0.0f);
        glm::vec3 b(10.0f, 0.0f, 0.0f);
        float cylR = 1.0f;

        // center beyond end but sphere radius reaches into the end cap -> intersects
		Cylinder cyl(a, b, cylR);
        Sphere s(glm::vec3(10.8f, 0.0f, 0.0f), 1.0f);
        EXPECT_TRUE(s.isColliding(cyl));
        EXPECT_TRUE(cyl.isColliding(s));
    }
}