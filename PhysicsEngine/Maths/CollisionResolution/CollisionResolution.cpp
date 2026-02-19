#include "CollisionResolution.h"
#include <glm/gtc/epsilon.hpp>
#include <glm/gtc/constants.hpp>

namespace Physics
{
    glm::vec3 ResolveVelocityAgainstFixedObject(const glm::vec3& vel, float restitution, const glm::vec3& normal)
    {
        const float EPS = 1e-8f;
        glm::vec3 n = normal;
        float nlen = glm::length(n);
        if (nlen <= EPS)
            return vel; // degenerate normal -> no change

        n = n / nlen; // ensure normalized

        float v_n = glm::dot(vel, n);
        // Only reflect when there is a component into the surface (v_n < 0)
        // If v_n >= 0 body is moving away from contact; keep velocity.
        if (v_n >= 0.0f)
            return vel;

        // v' = v - (1 + e) * (v · n) * n
        return vel - (1.0f + restitution) * v_n * n;
    }
}