#include "pch.h"
#include "../PhysicsEngine/Shapes/Sphere.h"
#include "../PhysicsEngine/Shapes/LineInf.h"

namespace ShapeTests
{
	using namespace Physics;
    TEST(LineSphereIntersectionTest, NoIntersection_CentreAtOrigin)
    {
        glm::vec3 linePoint(5.0f, 5.0f, 5.0f);
        glm::vec3 lineDir(1.0f, 0.0f, 0.0f);
        LineInf line(linePoint, linePoint + lineDir);

        Sphere sphere(glm::vec3(0.0f, 0.0f, 0.0f), 3.0f);

        EXPECT_FALSE(line.isCollidingWith(sphere));
        EXPECT_FALSE(sphere.isCollidingWith(line));
    }

    TEST(LineSphereIntersectionTest, PassesThroughSphere_CentreAtOrigin)
    {
        glm::vec3 linePoint(10.0f, 0.0f, 0.0f);
        glm::vec3 lineDir(-1.0f, 0.0f, 0.0f);
        LineInf line(linePoint, linePoint + lineDir);

        Sphere sphere(glm::vec3(10.0f, 0.0f, 0.0f), 5.0f);

        EXPECT_TRUE(line.isCollidingWith(sphere));
        EXPECT_TRUE(sphere.isCollidingWith(line));
    }

    TEST(LineSphereIntersectionTest, LineStartsInsideSphere)
    {
        glm::vec3 linePoint(3.0f, 2.0f, 2.0f);
        glm::vec3 lineDir(1.0f, 0.0f, 0.0f);
        LineInf line(linePoint, linePoint + lineDir);

        Sphere sphere(glm::vec3(2.0f, 2.0f, 2.0f), 5.0f);

        EXPECT_TRUE(line.isCollidingWith(sphere));
        EXPECT_TRUE(sphere.isCollidingWith(line));
    }

    TEST(LineSphereIntersectionTest, LinePassesThroughSphereCenter)
    {
        glm::vec3 linePoint(-5.0f, 0.0f, 0.0f);
        glm::vec3 lineDir(1.0f, 0.0f, 0.0f);
        LineInf line(linePoint, linePoint + lineDir);

        Sphere sphere(glm::vec3(0.0f, 0.0f, 0.0f), 3.0f);

        EXPECT_TRUE(line.isCollidingWith(sphere));
        EXPECT_TRUE(sphere.isCollidingWith(line));
    }
}
