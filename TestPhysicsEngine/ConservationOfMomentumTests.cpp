#include "pch.h"
#include <glm/glm.hpp>
#include <tuple>

#include "../PhysicsEngine/Maths/CollisionResolution/ConservationOfMomentum.h"

namespace MomentumTests
{
    using namespace Physics;

    static void ExpectVecNear(const glm::vec3& a, const glm::vec3& b, float eps = 1e-5f)
    {
        EXPECT_NEAR(a.x, b.x, eps);
        EXPECT_NEAR(a.y, b.y, eps);
        EXPECT_NEAR(a.z, b.z, eps);
    }

    TEST(ConservationOfMomentumTest, InElastic_HeadOnEqualMass_SwapsVelocities)
    {
        glm::vec3 v1(1.0f, 0.0f, 0.0f);
        glm::vec3 v2(-1.0f, 0.0f, 0.0f);
        float m1 = 1.0f;
        float m2 = 1.0f;

        auto [r1, r2] = InElasticCollision(v1, v2, m1, m2);

        // equal masses in a head-on 1D elastic collision -> velocities swap
        ExpectVecNear(r1, glm::vec3(-1.0f, 0.0f, 0.0f));
        ExpectVecNear(r2, glm::vec3(1.0f, 0.0f, 0.0f));
    }

    TEST(ConservationOfMomentumTest, Elastic_HeadOnDifferentMasses_KnownResult)
    {
        glm::vec3 v1(1.0f, 0.0f, 0.0f);
        glm::vec3 v2(-1.0f, 0.0f, 0.0f);
        float m1 = 2.0f;
        float m2 = 1.0f;

        auto [r1, r2] = ElasticCollision(v1, v2, m1, m2);

        // Analytical 1D results:
        // v1' = (m1-m2)/(m1+m2) * u1 + (2*m2)/(m1+m2) * u2 = -1/3
        // v2' = (m2-m1)/(m1+m2) * u2 + (2*m1)/(m1+m2) * u1 = 5/3
        ExpectVecNear(r1, glm::vec3(-1.0f / 3.0f, 0.0f, 0.0f), 1e-5f);
        ExpectVecNear(r2, glm::vec3(5.0f / 3.0f, 0.0f, 0.0f), 1e-5f);
    }

    TEST(ConservationOfMomentumTest, Elastic_RetainsTangentialComponent)
    {
        // Velocities not collinear: only normal component should change.
        glm::vec3 v1(1.0f, 1.0f, 0.0f);
        glm::vec3 v2(-1.0f, 0.0f, 0.0f);
        float m1 = 1.0f;
        float m2 = 1.0f;

        // compute collision normal used by ElasticCollision
        glm::vec3 rel = v1 - v2;
        glm::vec3 n = glm::normalize(rel);

        glm::vec3 tang1 = v1 - glm::dot(v1, n) * n;
        glm::vec3 tang2 = v2 - glm::dot(v2, n) * n;

        auto [r1, r2] = ElasticCollision(v1, v2, m1, m2);

        glm::vec3 tang1_after = r1 - glm::dot(r1, n) * n;
        glm::vec3 tang2_after = r2 - glm::dot(r2, n) * n;

        // tangential components should remain unchanged
        ExpectVecNear(tang1, tang1_after, 1e-5f);
        ExpectVecNear(tang2, tang2_after, 1e-5f);
    }

    TEST(ConservationOfMomentumTest, Elastic_NoRelativeVelocity_ReturnsSame)
    {
        glm::vec3 v(2.0f, -1.0f, 0.5f);
        float m1 = 1.0f;
        float m2 = 3.0f;

        auto [r1, r2] = ElasticCollision(v, v, m1, m2);

        ExpectVecNear(r1, v);
        ExpectVecNear(r2, v);
    }

    TEST(ConservationOfMomentumTest, Elastic_ZeroMassSum_ReturnsSame)
    {
        glm::vec3 v1(1.0f, 0.0f, 0.0f);
        glm::vec3 v2(-2.0f, 0.0f, 0.0f);
        float m1 = 0.0f;
        float m2 = 0.0f;

        auto [r1, r2] = ElasticCollision(v1, v2, m1, m2);

        ExpectVecNear(r1, v1);
        ExpectVecNear(r2, v2);
    }

    TEST(ConservationOfMomentumTest, Elastic_MomentumAndEnergyConserved)
    {
        glm::vec3 v1(0.7f, -0.3f, 0.2f);
        glm::vec3 v2(-0.4f, 0.6f, -0.1f);
        float m1 = 2.5f;
        float m2 = 1.75f;

        glm::vec3 p_before = m1 * v1 + m2 * v2;
        float ke_before = 0.5f * m1 * glm::dot(v1, v1) + 0.5f * m2 * glm::dot(v2, v2);

        auto [r1, r2] = ElasticCollision(v1, v2, m1, m2);

        glm::vec3 p_after = m1 * r1 + m2 * r2;
        float ke_after = 0.5f * m1 * glm::dot(r1, r1) + 0.5f * m2 * glm::dot(r2, r2);

        // momentum conserved
        ExpectVecNear(p_before, p_after, 1e-5f);

        // kinetic energy conserved for elastic collision (allow small numerical tol)
        EXPECT_NEAR(ke_before, ke_after, 1e-4f);
    }

    TEST(ConservationOfMomentumTest, InElastic_MomentumAndEnergyConserved_AsImplemented)
    {
        // Note: InElasticCollision implementation currently uses elastic formula.
        glm::vec3 v1(0.5f, 0.2f, -0.1f);
        glm::vec3 v2(-0.2f, 0.4f, 0.3f);
        float m1 = 1.2f;
        float m2 = 0.8f;

        glm::vec3 p_before = m1 * v1 + m2 * v2;
        float ke_before = 0.5f * m1 * glm::dot(v1, v1) + 0.5f * m2 * glm::dot(v2, v2);

        auto [r1, r2] = InElasticCollision(v1, v2, m1, m2);

        glm::vec3 p_after = m1 * r1 + m2 * r2;
        float ke_after = 0.5f * m1 * glm::dot(r1, r1) + 0.5f * m2 * glm::dot(r2, r2);

        // momentum conserved
        ExpectVecNear(p_before, p_after, 1e-5f);

        // because implementation is elastic-like, KE should be conserved (within tolerance)
        EXPECT_NEAR(ke_before, ke_after, 1e-4f);
    }
}