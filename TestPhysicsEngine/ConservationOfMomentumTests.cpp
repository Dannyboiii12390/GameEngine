#include "pch.h"
#include <glm/glm.hpp>

namespace MomentumTests
{
    // Component-wise near for vec3
    static void ExpectVecNear(const glm::vec3& a, const glm::vec3& b, float eps = 1e-5f)
    {
        EXPECT_NEAR(a.x, b.x, eps);
        EXPECT_NEAR(a.y, b.y, eps);
        EXPECT_NEAR(a.z, b.z, eps);
    }

    // 1D elastic collision analytic result using glm::vec3 for masses and velocities.
    // Masses are provided as glm::vec3 where all components should be equal (mass stored per-component).
    static void ElasticCollision1D(const glm::vec3& m1, const glm::vec3& m2,
                                   const glm::vec3& u1, const glm::vec3& u2,
                                   glm::vec3& out_v1, glm::vec3& out_v2)
    {
        // Performs the standard closed-form 1D elastic collision component-wise.
        // This lets tests use glm::vec3 everywhere (no plain floats for masses/velocities).
        out_v1 = (u1 * (m1 - m2) + glm::vec3(2.0f) * m2 * u2) / (m1 + m2);
        out_v2 = (u2 * (m2 - m1) + glm::vec3(2.0f) * m1 * u1) / (m1 + m2);
    }

    // 3D vector elastic collision for two spheres (assumes instantaneous elastic collision along line of centres)
    // p1, p2 = positions; v1, v2 = incoming velocities; m1, m2 = masses stored as glm::vec3 (all components equal).
    // Produces v1', v2' in out_v1/out_v2.
    static void ElasticCollisionVec3(const glm::vec3& p1, const glm::vec3& p2,
                                     const glm::vec3& v1, const glm::vec3& v2,
                                     const glm::vec3& m1, const glm::vec3& m2,
                                     glm::vec3& out_v1, glm::vec3& out_v2)
    {
        // Extract scalar masses from the vector mass representation (assume components equal)
        float ms1 = m1.x;
        float ms2 = m2.x;

        glm::vec3 n = p1 - p2;
        float len = glm::length(n);
        if (len <= 1e-8f)
        {
            // Degenerate: treat as no change
            out_v1 = v1;
            out_v2 = v2;
            return;
        }
        n = n / len; // unit normal from 2 -> 1

        float k1 = (2.0f * ms2) / (ms1 + ms2);
        float k2 = (2.0f * ms1) / (ms1 + ms2);

        float a1 = glm::dot(v1 - v2, n);
        float a2 = glm::dot(v2 - v1, n);

        out_v1 = v1 - k1 * a1 * n;
        out_v2 = v2 - k2 * a2 * n;
    }

    TEST(ConservationOfMomentum, HeadOn_DifferentMasses_1D)
    {
        glm::vec3 m1(2.0f, 2.0f, 2.0f); // use vec3 mass (same value per component)
        glm::vec3 m2(3.0f, 3.0f, 3.0f);
        glm::vec3 u1(5.0f, 0.0f, 0.0f);
        glm::vec3 u2(-2.0f, 0.0f, 0.0f);

        glm::vec3 beforeMomentum = m1 * u1 + m2 * u2;

        glm::vec3 v1(0.0f);
        glm::vec3 v2(0.0f);
        ElasticCollision1D(m1, m2, u1, u2, v1, v2);

        glm::vec3 afterMomentum = m1 * v1 + m2 * v2;

        ExpectVecNear(beforeMomentum, afterMomentum);
    }

    TEST(ConservationOfMomentum, EqualMasses_SwapVelocities_1D)
    {
        // Equal masses -> elastic head-on swap velocities (vector form)
        glm::vec3 m(1.0f, 1.0f, 1.0f);
        glm::vec3 u1(3.0f, 0.0f, 0.0f);
        glm::vec3 u2(0.0f, 0.0f, 0.0f);

        glm::vec3 beforeMomentum = m * u1 + m * u2;

        glm::vec3 v1(0.0f), v2(0.0f);
        ElasticCollision1D(m, m, u1, u2, v1, v2);

        // For equal masses and 1D elastic collision expect velocities swapped
        ExpectVecNear(v1, u2);
        ExpectVecNear(v2, u1);

        glm::vec3 afterMomentum = m * v1 + m * v2;
        ExpectVecNear(beforeMomentum, afterMomentum);
    }

    TEST(ConservationOfMomentum, GlancingCollision_2D_VectorCase)
    {
        // Two spheres colliding off-axis. Positions set so line of centres is X axis -> reduces to 1D along normal
        glm::vec3 p1(0.0f, 0.0f, 0.0f);
        glm::vec3 p2(1.0f, 0.0f, 0.0f); // centres separated along +X

        glm::vec3 m1(2.0f, 2.0f, 2.0f);
        glm::vec3 m2(1.0f, 1.0f, 1.0f);

        // v1 has both x (normal) and y (tangential) components; tangential should be preserved
        glm::vec3 v1(1.0f, 0.5f, 0.0f);
        glm::vec3 v2(-1.0f, 0.0f, 0.0f);

        glm::vec3 beforeMomentum = m1 * v1 + m2 * v2;

        glm::vec3 v1p, v2p;
        ElasticCollisionVec3(p1, p2, v1, v2, m1, m2, v1p, v2p);

        glm::vec3 afterMomentum = m1 * v1p + m2 * v2p;

        ExpectVecNear(beforeMomentum, afterMomentum);
    }

    TEST(ConservationOfMomentum, StationaryTarget_1D)
    {
        // Stationary target: m1 moving, m2 at rest.
        glm::vec3 m1(1.5f, 1.5f, 1.5f);
        glm::vec3 m2(2.5f, 2.5f, 2.5f);
        glm::vec3 u1(4.0f, 0.0f, 0.0f);
        glm::vec3 u2(0.0f, 0.0f, 0.0f);

        glm::vec3 beforeMomentum = m1 * u1 + m2 * u2;

        glm::vec3 v1(0.0f), v2(0.0f);
        ElasticCollision1D(m1, m2, u1, u2, v1, v2);

        glm::vec3 afterMomentum = m1 * v1 + m2 * v2;

        ExpectVecNear(beforeMomentum, afterMomentum);
    }
}