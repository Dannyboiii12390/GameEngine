#include "pch.h"
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include "../PhysicsEngine/Shapes/Sphere.h"
#include "../PhysicsEngine/Shapes/LineInf.h"

namespace ShapeTests
{
    using namespace Physics;
    TEST(LineSegmentSphereTest, Intersects_AtMiddleOfSegment)
    {
        glm::vec3 start(0.0f, 0.0f, 0.0f);
        glm::vec3 end(10.0f, 0.0f, 0.0f);
        Sphere s(glm::vec3(5.0f, 0.0f, 0.0f), 1.0f); // sphere centered at x=5 intersects middle

        LineInf line(start, end);
        float length = glm::length(end - start);
        EXPECT_TRUE(line.SegmentIntersectsSphere(s, length));
    }

    TEST(LineSegmentSphereTest, Intersects_AtStartOfSegment)
    {
        glm::vec3 start(1.0f, 0.0f, 0.0f); // lies on sphere surface
        glm::vec3 end(5.0f, 2.0f, 0.0f);
        Sphere s(glm::vec3(0.0f, 0.0f, 0.0f), 1.0f);

        LineInf line(start, end);
        float length = glm::length(end - start);
        EXPECT_TRUE(line.SegmentIntersectsSphere(s, length));
    }

    TEST(LineSegmentSphereTest, Intersects_AtEndOfSegment)
    {
        glm::vec3 start(0.0f, 1.0f, 0.0f);
        glm::vec3 end(2.0f, 0.0f, 0.0f); // end at distance 1 from sphere center at (3,0,0)
        Sphere s(glm::vec3(3.0f, 0.0f, 0.0f), 1.0f);

        LineInf line(start, end);
        float length = glm::length(end - start);
        EXPECT_TRUE(line.SegmentIntersectsSphere(s, length));
    }

    TEST(LineSegmentSphereTest, NoIntersection_SphereTooFar)
    {
        glm::vec3 start(0.0f, 0.0f, 0.0f);
        glm::vec3 end(1.0f, 0.0f, 0.0f);
        Sphere s(glm::vec3(5.0f, 0.0f, 0.0f), 1.0f);

        LineInf line(start, end);
        float length = glm::length(end - start);
        EXPECT_FALSE(line.SegmentIntersectsSphere(s, length));
    }

    TEST(LineSegmentSphereTest, NoIntersection_LineIntersectsBeforeStart)
    {
        // Infinite line would intersect the sphere, but the intersection points lie
        // before the segment start -> segment misses the sphere.
        glm::vec3 start(5.0f, 0.0f, 0.0f);
        glm::vec3 end(10.0f, 0.0f, 0.0f);
        Sphere s(glm::vec3(-5.0f, 0.0f, 0.0f), 3.0f); // intersection region on line is around x in [-8, -2]

        LineInf line(start, end);
        float length = glm::length(end - start);
        EXPECT_FALSE(line.SegmentIntersectsSphere(s, length));
    }

    TEST(LineSegmentSphereTest, NoIntersection_LineIntersectsAfterEnd)
    {
        // Infinite line would intersect the sphere, but the intersection points lie
        // after the segment end -> segment misses the sphere.
        glm::vec3 start(-10.0f, 0.0f, 0.0f);
        glm::vec3 end(-5.0f, 0.0f, 0.0f);
        Sphere s(glm::vec3(5.0f, 0.0f, 0.0f), 3.0f); // intersection region on line is around x in [2,8]

        LineInf line(start, end);
        float length = glm::length(end - start);
        EXPECT_FALSE(line.SegmentIntersectsSphere(s, length));
    }
}