#include "pch.h"
#include <glm/glm.hpp>
#include "../PhysicsEngine/Shapes/Sphere.h"

namespace ShapeTests
{
    using namespace Physics;

    TEST(SphereTest, NoIntersection_CentreAtOrigin)
    {
        Sphere a(glm::vec3(0.0f, 0.0f, 0.0f), 1.0f);
        Sphere b(glm::vec3(5.0f, 0.0f, 0.0f), 1.0f);

        EXPECT_FALSE(a.Intersects(b));
        EXPECT_FALSE(b.Intersects(a));
    }

    TEST(SphereTest, NoIntersection_OffsetCentre)
    {
        Sphere a(glm::vec3(3.0f, 3.0f, 3.0f), 2.0f);
        Sphere b(glm::vec3(10.0f, 10.0f, 10.0f), 2.0f);

        EXPECT_FALSE(a.Intersects(b));
        EXPECT_FALSE(b.Intersects(a));
    }

    TEST(SphereTest, Overlapping_CentreAtOrigin)
    {
        Sphere a(glm::vec3(0.0f, 0.0f, 0.0f), 2.0f);
        Sphere b(glm::vec3(2.0f, 0.0f, 0.0f), 2.0f);

        EXPECT_TRUE(a.Intersects(b));
        EXPECT_TRUE(b.Intersects(a));
    }

    TEST(SphereTest, Overlapping_OffsetCentre)
    {
        Sphere a(glm::vec3(5.0f, 5.0f, 5.0f), 3.0f);
        Sphere b(glm::vec3(8.0f, 5.0f, 5.0f), 3.0f);

        EXPECT_TRUE(a.Intersects(b));
        EXPECT_TRUE(b.Intersects(a));
    }

    TEST(SphereTest, FullyContained_CentreAtOrigin)
    {
        Sphere outer(glm::vec3(0.0f, 0.0f, 0.0f), 3.0f);
        Sphere inner(glm::vec3(1.0f, 0.0f, 0.0f), 1.0f);

        EXPECT_TRUE(outer.Intersects(inner));
        EXPECT_TRUE(inner.Intersects(outer));
    }

    TEST(SphereTest, FullyContained_OffsetCentre)
    {
        Sphere outer(glm::vec3(6.0f, 6.0f, 6.0f), 5.0f);
        Sphere inner(glm::vec3(7.0f, 6.0f, 6.0f), 2.0f);

        EXPECT_TRUE(outer.Intersects(inner));
        EXPECT_TRUE(inner.Intersects(outer));
    }
}