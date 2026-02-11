
#include "pch.h"
#include <glm/glm.hpp>
#include "../PhysicsEngine/Shapes/Plane.h"

namespace ShapeTests
{
    using namespace Physics;

    // Helper to construct a Plane from a point+normal by choosing two orthogonal in-plane vectors.
    static Plane BuildPlaneFromPointNormal(const glm::vec3& point, const glm::vec3& normal)
    {
        glm::vec3 n = glm::normalize(normal);
        glm::vec3 arbitrary = (glm::abs(n.x) < 0.9f) ? glm::vec3(1.0f, 0.0f, 0.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
        glm::vec3 u = glm::normalize(glm::cross(arbitrary, n));
        glm::vec3 v = glm::cross(n, u);
        return Plane::FromThreePoints(point, point + u, point + v);
    }

    TEST(PlaneTest, PointAbovePlane)
    {
        Plane p = BuildPlaneFromPointNormal(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        glm::vec3 pt(2.0f, 3.0f, 5.0f);
        float dist = p.getShortestDistance(pt);
        EXPECT_NEAR(dist, 5.0f, 0.01f);
    }

    TEST(PlaneTest, PointBelowPlane)
    {
        Plane p = BuildPlaneFromPointNormal(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        glm::vec3 pt(2.0f, 3.0f, -4.0f);
        float dist = p.getShortestDistance(pt);
        EXPECT_NEAR(dist, 4.0f, 0.01f);
    }

    TEST(PlaneTest, PointOnPlane)
    {
        Plane p = BuildPlaneFromPointNormal(glm::vec3(1.0f, 1.0f, 1.0f), glm::vec3(1.0f, 1.0f, 1.0f));
        glm::vec3 pt(0.0f, 2.0f, 1.0f);
        float dist = p.getShortestDistance(pt);
        EXPECT_NEAR(dist, 0.0f, 1e-5f);
    }

    TEST(PlaneTest, PointCloseToPlane)
    {
        Plane p = BuildPlaneFromPointNormal(glm::vec3(0.0f), glm::vec3(1.0f, 1.0f, 0.0f));
        glm::vec3 pt(1.0f, 1.0f, 1.0f);
        float dist = p.getShortestDistance(pt);
        EXPECT_NEAR(dist, 1.41421356f, 0.01f);
    }

    TEST(PlaneTest, PointWithNegativeCoordinates)
    {
        Plane p = BuildPlaneFromPointNormal(glm::vec3(-2.0f, -2.0f, -2.0f), glm::vec3(1.0f, 1.0f, 1.0f));
        glm::vec3 pt(-1.0f, -1.0f, -1.0f);
        float dist = p.getShortestDistance(pt);
        EXPECT_NEAR(dist, 1.7320508f, 0.01f);
    }

    TEST(PlaneTest, PointAlongNormalDirection)
    {
        Plane p = BuildPlaneFromPointNormal(glm::vec3(0.0f), glm::vec3(1.0f, 1.0f, 0.0f));
        glm::vec3 pt(1.0f, 1.0f, 0.0f);
        float dist = p.getShortestDistance(pt);
        EXPECT_NEAR(dist, 1.41421356f, 0.01f);
    }

    TEST(PlaneTest, PointNearPlaneRandomDirection)
    {
        Plane p = BuildPlaneFromPointNormal(glm::vec3(0.0f), glm::vec3(1.0f, -1.0f, 0.0f));
        glm::vec3 pt(1.0f, 2.0f, 3.0f);
        float dist = p.getShortestDistance(pt);
        EXPECT_NEAR(dist, 0.70710678f, 0.01f);
    }
}