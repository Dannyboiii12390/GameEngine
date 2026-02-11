
#include "pch.h"
#include "../PhysicsEngine/Shapes/LineInf.h"
#include <glm/glm.hpp>

namespace ShapeTests
{
    using namespace Physics;

    TEST(LineTest, ClosestPointOnLine)
    {
        glm::vec3 a(0.0f, 0.0f, 0.0f);
        glm::vec3 dir(1.0f, 1.0f, 1.0f);
        LineInf line(a, a + dir);

        glm::vec3 p(2.0f, 3.0f, 4.0f);
        glm::vec3 closest = line.getShortestPathToPoint(p);
        float dist = glm::length(p - closest);

        EXPECT_NEAR(dist, 1.41f, 0.01f);
    }

    TEST(LineTest, GeneralPointIsOnLine)
    {
        glm::vec3 a(0.0f, 0.0f, 0.0f);
        glm::vec3 dir(1.0f, 2.0f, 3.0f);
        LineInf line(a, a + dir);

        glm::vec3 p(3.0f, 6.0f, 9.0f); // 3 * dir -> lies on the line
        glm::vec3 closest = line.getShortestPathToPoint(p);
        float dist = glm::length(p - closest);

        EXPECT_NEAR(dist, 0.0f, 1e-5f);
    }

    TEST(LineTest, VerticalLineCase)
    {
        glm::vec3 a(2.0f, 2.0f, 0.0f);
        glm::vec3 dir(0.0f, 0.0f, 1.0f);
        LineInf line(a, a + dir);

        glm::vec3 p(4.0f, 5.0f, 3.0f);
        glm::vec3 closest = line.getShortestPathToPoint(p);
        float dist = glm::length(p - closest);

        EXPECT_NEAR(dist, 3.61f, 0.01f);
    }

    TEST(LineTest, HorizontalLineCase)
    {
        glm::vec3 a(0.0f, 0.0f, 0.0f);
        glm::vec3 dir(1.0f, 0.0f, 0.0f);
        LineInf line(a, a + dir);

        glm::vec3 p(3.0f, 4.0f, 5.0f);
        glm::vec3 closest = line.getShortestPathToPoint(p);
        float dist = glm::length(p - closest);

        EXPECT_NEAR(dist, 6.40f, 0.01f);
    }

    TEST(LineTest, DiagonalLineCase)
    {
        glm::vec3 a(1.0f, 1.0f, 1.0f);
        glm::vec3 dir(1.0f, -1.0f, 1.0f); // direction vector
        LineInf line(a, a + dir);

        glm::vec3 p(2.0f, 5.0f, 3.0f);
        glm::vec3 closest = line.getShortestPathToPoint(p);
        float dist = glm::length(p - closest);

        EXPECT_NEAR(dist, 4.55f, 0.01f);
    }
}