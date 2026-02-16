#include "pch.h"
#include <glm/glm.hpp>

#include "../PhysicsEngine/Maths/Integration.h"

namespace Calculus
{
    using namespace Physics;

    static void ExpectVecNear(const glm::vec3& a, const glm::vec3& b, float eps = 1e-5f)
    {
        EXPECT_NEAR(a.x, b.x, eps);
        EXPECT_NEAR(a.y, b.y, eps);
        EXPECT_NEAR(a.z, b.z, eps);
    }

    TEST(IntegrationTest, Euler_SingleStep_NoAcceleration)
    {
        glm::vec3 pos(1.0f, 2.0f, 3.0f);
        glm::vec3 vel(0.5f, -1.0f, 2.0f);
        float dt = 0.25f;

        glm::vec3 result = Integration<glm::vec3>::Euler(pos, vel, dt);
        glm::vec3 expected = pos + vel * dt;

        ExpectVecNear(result, expected);
    }

    TEST(IntegrationTest, Euler_MultipleSteps_NoAcceleration)
    {
        glm::vec3 pos(0.0f, 0.0f, 0.0f);
        glm::vec3 vel(0.1f, 0.2f, -0.3f);
        float dt = 0.01f;
        int steps = 1000; // total time = 10.0s

        glm::vec3 cur = pos;
        for (int i = 0; i < steps; ++i)
        {
            cur = Integration<glm::vec3>::Euler(cur, vel, dt);
        }

        glm::vec3 expected = pos + vel * (dt * steps);
        ExpectVecNear(cur, expected, 1e-4f); // accumulate a little tolerance for repeated ops
    }

    TEST(IntegrationTest, SemiImplicitEuler_Matches_Euler_ForConstantVelocity)
    {
        glm::vec3 pos(2.0f, -1.0f, 0.5f);
        glm::vec3 vel(-0.25f, 0.75f, 1.0f);
        float dt = 0.125f;

        glm::vec3 euler = Integration<glm::vec3>::Euler(pos, vel, dt);
        glm::vec3 semi = Integration<glm::vec3>::SemiImplicitEuler(pos, vel, dt);

        ExpectVecNear(euler, semi);
    }

    TEST(IntegrationTest, RungeKutta4_NoAcceleration_Equivalence)
    {
        // With no acceleration and constant derivative (velocity),
        // all RK4 k-terms are identical -> result must equal pos + vel * dt
        glm::vec3 pos(4.0f, 4.0f, 4.0f);
        glm::vec3 vel(1.5f, -0.5f, 0.25f);
        float dt = 0.2f;

        glm::vec3 rk4 = Integration<glm::vec3>::RungeKutta4(pos, vel, vel, vel, vel, dt);
        glm::vec3 expected = pos + vel * dt;

        ExpectVecNear(rk4, expected);
    }

    TEST(IntegrationTest, Midpoint_NoAcceleration_Equivalence)
    {
        // Midpoint uses k2 for the step; if k2 == velocity then step equals pos + vel * dt
        glm::vec3 pos(-1.0f, 2.5f, -0.75f);
        glm::vec3 vel(0.33f, -0.66f, 0.99f);
        float dt = 0.5f;

        glm::vec3 mid = Integration<glm::vec3>::Midpoint(pos, vel, vel, dt);
        glm::vec3 expected = pos + vel * dt;

        ExpectVecNear(mid, expected);
    }
}