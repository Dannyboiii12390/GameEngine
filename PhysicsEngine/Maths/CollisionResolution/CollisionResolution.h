#pragma once

#include <glm/glm.hpp>

namespace Physics
{
    glm::vec3 ResolveVelocityAgainstFixedObject(const glm::vec3& vel, float restitution, const glm::vec3& normal);





}