#include "SystemPhysics.h"
#include "../Components/ComponentTransform.h"
#include "../Components/ComponentNetwork.h"
#include "../Components/ComponentPhysics.h"
#include "../Components/ComponentVelocity.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <omp.h>


void SystemPhysics::OnUpdate(std::span<Entity> entities, float deltaTime)
{
    EComponentType requiredComponents = EComponentType::Component_Physics | EComponentType::Component_Velocity;

    const int count = static_cast<int>(entities.size());

    // Each entity's physics state is independent — safe to parallelise.
    #pragma omp parallel for
    for (int i = 0; i < count; ++i)
    {
        auto& entity = entities[i];

		auto* netComp = entity.GetComponent<ComponentNetwork>(EComponentType::Component_Network);

        // If it's a simulated object AND we don't own it, SKIP simulation
        if (netComp && netComp->IsSimulated() && !netComp->IsOwnedByMe(m_localPeerId)) {
            continue; // Another peer is simulating this; we will just receive the transform updates
        }

        if (!entity.HasComponent(requiredComponents))
            continue;

        ComponentPhysics* physics   = entity.GetComponent<ComponentPhysics>(EComponentType::Component_Physics);
        ComponentVelocity* velocity = entity.GetComponent<ComponentVelocity>(EComponentType::Component_Velocity);

        if (physics->IsAffectedByGravity())
            physics->ApplyForce(glm::vec3(0.0f, -g_GRAVITY_CONSTANT, 0.0f));

        glm::vec3 acceleration = physics->getTotalForce() * physics->GetInverseMass();
        glm::vec3 new_vel = Physics::Integration<glm::vec3>::Integrate(
            physics->GetIntegrationMethod(), velocity->GetPositionVelocity(), acceleration, deltaTime);
        velocity->SetPositionalVelocity(new_vel);

        ComponentTransform* transform = entity.GetComponent<ComponentTransform>(EComponentType::Component_Transform);
        glm::mat3 R = glm::mat3(1.0f);
        if (transform)
        {
            glm::mat4 rotX = glm::rotate(glm::mat4(1.0f), glm::radians(transform->Rotation().x), glm::vec3(1.0f, 0.0f, 0.0f));
            glm::mat4 rotY = glm::rotate(glm::mat4(1.0f), glm::radians(transform->Rotation().y), glm::vec3(0.0f, 1.0f, 0.0f));
            glm::mat4 rotZ = glm::rotate(glm::mat4(1.0f), glm::radians(transform->Rotation().z), glm::vec3(0.0f, 0.0f, 1.0f));
            glm::mat4 rot  = rotZ * rotY * rotX;
            R = glm::mat3(rot);
        }

        glm::vec3 tau_world     = physics->getTotalTorque();
        glm::vec3 tau_obj       = glm::transpose(R) * tau_world;
        glm::vec3 omega_world   = velocity->GetRotationalVelocity();
        glm::vec3 omega_obj     = glm::transpose(R) * omega_world;
        glm::vec3 alpha_obj     = physics->GetInverseInertiaTensor() * tau_obj;
        glm::vec3 omega_obj_new = Physics::Integration<glm::vec3>::Integrate(
            physics->GetIntegrationMethod(), omega_obj, alpha_obj, deltaTime);
        glm::vec3 omega_world_new = R * omega_obj_new;
        velocity->SetRotationalVelocity(omega_world_new);

        // Velocity swapping is owned by SystemVelocity. No swap here.

        physics->ClearForces();
        physics->ClearTorques();
    }
}