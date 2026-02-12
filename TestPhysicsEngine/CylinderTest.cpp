
#include "pch.h"
#include <glm/glm.hpp>
#include "../PhysicsEngine/Shapes/Cylinder.h"

namespace ShapeTests
{
    using namespace Physics;

    TEST(CylinderPointTest, PointInsideCylinder)
    {
        // Cylinder along X axis from x=0..10 radius 1
        glm::vec3 start(0.0f, 0.0f, 0.0f);
        glm::vec3 end(10.0f, 0.0f, 0.0f);
        Cylinder cyl(start, end, 1.0f);

        // point at middle axis -> inside
        glm::vec3 pInside(5.0f, 0.0f, 0.0f);
        EXPECT_TRUE(cyl.ContainsPoint(pInside));
    }

    TEST(CylinderPointTest, PointOutsideCylinder)
    {
        glm::vec3 start(0.0f, 0.0f, 0.0f);
        glm::vec3 end(10.0f, 0.0f, 0.0f);
        Cylinder cyl(start, end, 1.0f);

        // point outside radially (2 units away from axis)
        glm::vec3 pOutside(5.0f, 2.0f, 0.0f);
        EXPECT_FALSE(cyl.ContainsPoint(pOutside));
    }

    TEST(CylinderPointTest, PointOutside_CloseToStart)
    {
        // short cylinder; radius 1.0, start at x=1
        glm::vec3 start(1.0f, 0.0f, 0.0f);
        glm::vec3 end(5.0f, 0.0f, 0.0f);
        Cylinder cyl(start, end, 1.0f);

        // point slightly beyond radius from start (distance = 1.1) -> outside near start
        glm::vec3 pNearStart(1.0f - 1.1f, 0.0f, 0.0f);
        EXPECT_FALSE(cyl.ContainsPoint(pNearStart));
    }

    TEST(CylinderPointTest, PointOutside_CloseToEnd)
    {
        glm::vec3 start(-10.0f, 0.0f, 0.0f);
        glm::vec3 end(-5.0f, 0.0f, 0.0f);
        Cylinder cyl(start, end, 1.0f);

        // point slightly beyond radius from end (distance = 1.2) -> outside near end
        glm::vec3 pNearEnd(-5.0f + 1.2f, 0.0f, 0.0f);
        EXPECT_FALSE(cyl.ContainsPoint(pNearEnd));
    }
}