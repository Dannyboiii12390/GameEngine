#include "pch.h"
#include <glm/glm.hpp>
#include "../PhysicsEngine/Shapes/Capsule.h"
#include "../PhysicsEngine/Shapes/Sphere.h"

namespace ShapeTests
{
    using namespace Physics;

    TEST(CapsuleTest, PointInsideCapsule_Middle)
    {
        Capsule cap(glm::vec3(0.0f), glm::vec3(10.0f, 0.0f, 0.0f), 1.0f);
        glm::vec3 p(5.0f, 0.5f, 0.0f); // inside radial distance
        EXPECT_TRUE(cap.ContainsPoint(p));
    }

    TEST(CapsuleTest, PointOutsideCapsule_Middle)
    {
        Capsule cap(glm::vec3(0.0f), glm::vec3(10.0f, 0.0f, 0.0f), 1.0f);
        glm::vec3 p(5.0f, 2.0f, 0.0f); // outside radial distance
        EXPECT_FALSE(cap.ContainsPoint(p));
    }

    TEST(CapsuleTest, PointOutsideCapsule_CloseToStart)
    {
        Capsule cap(glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(5.0f, 0.0f, 0.0f), 1.0f);
        glm::vec3 p(1.0f - 1.1f, 0.0f, 0.0f); // slightly beyond start cap
        EXPECT_FALSE(cap.ContainsPoint(p));
    }

    TEST(CapsuleTest, PointOutsideCapsule_CloseToEnd)
    {
        Capsule cap(glm::vec3(-10.0f, 0.0f, 0.0f), glm::vec3(-5.0f, 0.0f, 0.0f), 1.0f);
        glm::vec3 p(-5.0f + 1.2f, 0.0f, 0.0f); // slightly beyond end cap
        EXPECT_FALSE(cap.ContainsPoint(p));
    }

    // Sphere vs Capsule intersection tests (covers cases at middle, start and end).
    TEST(CapsuleTest, SphereIntersectsAtMiddle)
    {
        Capsule cap(glm::vec3(0.0f), glm::vec3(10.0f, 0.0f, 0.0f), 2.0f);
        Sphere s(glm::vec3(5.0f, 1.0f, 0.0f), 0.5f); // projects to middle, inside
        EXPECT_TRUE(s.isColliding(cap));
        EXPECT_TRUE(cap.Intersects(s));
    }

    TEST(CapsuleTest, SphereDoesNotIntersectAtMiddle)
    {
        Capsule cap(glm::vec3(0.0f), glm::vec3(10.0f, 0.0f, 0.0f), 2.0f);
        Sphere s(glm::vec3(5.0f, 3.5f, 0.0f), 0.5f); // too far radially
        EXPECT_FALSE(s.isColliding(cap));
        EXPECT_FALSE(cap.Intersects(s));
    }

    TEST(CapsuleTest, SphereIntersectsAtStart_Inside)
    {
        Capsule cap(glm::vec3(0.0f), glm::vec3(10.0f, 0.0f, 0.0f), 1.0f);
        Sphere s(glm::vec3(0.5f, 0.0f, 0.0f), 0.6f); // inside near start
        EXPECT_TRUE(s.isColliding(cap));
        EXPECT_TRUE(cap.Intersects(s));
    }

    TEST(CapsuleTest, SphereIntersectsAtStart_OutsideButTouching)
    {
        Capsule cap(glm::vec3(0.0f), glm::vec3(10.0f, 0.0f, 0.0f), 1.0f);
        Sphere s(glm::vec3(-0.5f, 0.0f, 0.0f), 0.6f); // center before start but radius reaches cap -> intersects
        EXPECT_TRUE(s.isColliding(cap));
        EXPECT_TRUE(cap.Intersects(s));
    }

    TEST(CapsuleTest, SphereIntersectsAtEnd_Inside)
    {
        Capsule cap(glm::vec3(0.0f), glm::vec3(10.0f, 0.0f, 0.0f), 1.0f);
        Sphere s(glm::vec3(10.0f, 0.5f, 0.0f), 0.4f); // inside near end
        EXPECT_TRUE(s.isColliding(cap));
        EXPECT_TRUE(cap.Intersects(s));
    }

    TEST(CapsuleTest, SphereIntersectsAtEnd_OutsideButTouching)
    {
        Capsule cap(glm::vec3(0.0f), glm::vec3(10.0f, 0.0f, 0.0f), 1.0f);
        Sphere s(glm::vec3(10.8f, 0.0f, 0.0f), 1.0f); // center beyond end but radius reaches cap -> intersects
        EXPECT_TRUE(s.isColliding(cap));
        EXPECT_TRUE(cap.Intersects(s));
    }
}