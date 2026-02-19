#include "pch.h"
#include <glm/glm.hpp>

#include "../PhysicsEngine/Shapes/Plane.h"
#include "../PhysicsEngine/Shapes/Sphere.h"
#include "../PhysicsEngine/Shapes/Cylinder.h"
#include "../PhysicsEngine/Maths/CollisionResolution/CollisionResolution.h"


namespace CollisionImpulseTests
{

    static void ExpectVecNear(const glm::vec3& a, const glm::vec3& b, float eps = 1e-5f)
    {
        EXPECT_NEAR(a.x, b.x, eps);
        EXPECT_NEAR(a.y, b.y, eps);
        EXPECT_NEAR(a.z, b.z, eps);
    }

    TEST(CollisionImpulseTests, ReflectAgainstPlane_HeadOn)
    {
        glm::vec3 v(0.0f, -2.0f, 0.0f); // moving downward into upward-facing plane
        glm::vec3 normal(0.0f, 1.0f, 0.0f); // plane normal pointing up
        float restitution = 1.0f; // perfectly elastic

        glm::vec3 out = Physics::ResolveVelocityAgainstFixedObject(v, restitution, normal);

        // Head-on: should invert normal component (v' = v - 2*(v·n) n ) -> (0, 2, 0)
        ExpectVecNear(out, glm::vec3(0.0f, 2.0f, 0.0f));
    }

    TEST(CollisionImpulseTests, ReflectAgainstPlane_Angled)
    {
        glm::vec3 v(1.0f, -1.0f, 0.0f);
        glm::vec3 normal = glm::normalize(glm::vec3(0.0f, 1.0f, 0.0f));
        float restitution = 1.0f;

        glm::vec3 out = Physics::ResolveVelocityAgainstFixedObject(v, restitution, normal);

        // Normal component flips sign; tangential (x) component unchanged => (1,1,0)
        ExpectVecNear(out, glm::vec3(1.0f, 1.0f, 0.0f));
    }

    TEST(CollisionImpulseTests, ReflectAgainstCylinder_Radial)
    {
        // Cylinder axis along X from (0,0,0) to (10,0,0), radius 1.
        Physics::Cylinder cyl(glm::vec3(0.0f), glm::vec3(10.0f, 0.0f, 0.0f), 1.0f);

        // Sphere center above axis at (5, 1, 0) moving downward (into cylinder radial inward)
        glm::vec3 spherePos(5.0f, 1.0f, 0.0f);
        glm::vec3 v(0.0f, -2.0f, 0.0f);

        // Compute normal: radial from closest point on axis to sphere center -> (0, 1, 0) normalized -> (0,1,0)
        glm::vec3 A = cyl.getA();
        glm::vec3 B = cyl.getB();
        glm::vec3 u = glm::normalize(B - A);
        float t_raw = glm::dot(spherePos - A, u);
        float L = glm::length(B - A);
        float t = std::clamp(t_raw, 0.0f, L);
        glm::vec3 closest = A + u * t;
        glm::vec3 normal = glm::normalize(spherePos - closest);

        glm::vec3 out = Physics::ResolveVelocityAgainstFixedObject(v, 1.0f, normal);

        // Should reflect Y component to positive
        ExpectVecNear(out, glm::vec3(0.0f, 2.0f, 0.0f));
    }
}