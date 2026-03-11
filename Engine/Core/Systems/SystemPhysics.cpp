#include "SystemPhysics.h"
#include "../Components/ComponentPhysics.h"
#include "../Components/ComponentVelocity.h"
#include "../Components/ComponentTransform.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>


void SystemPhysics::OnUpdate(std::span<Entity> entities, float deltaTime)
{
    // We care about entities that have both physics and velocity.
    EComponentType requiredComponents = EComponentType::Component_Physics | EComponentType::Component_Velocity;

    for (auto& entity : entities)
    {
        if (!entity.HasComponent(requiredComponents))
            continue;

        ComponentPhysics* physics = entity.GetComponent<ComponentPhysics>(EComponentType::Component_Physics);
        ComponentVelocity* velocity = entity.GetComponent<ComponentVelocity>(EComponentType::Component_Velocity);

        if (physics->IsAffectedByGravity())
        {
            physics->ApplyForce(glm::vec3(0.0f, -g_GRAVITY_CONSTANT, 0.0f));
        }

        // a = F / m  -> using inverse mass for multiplication
        glm::vec3 acceleration = physics->getTotalForce() * physics->GetInverseMass();

        // v = v + a * dt
        glm::vec3 new_vel = Physics::Integration<glm::vec3>::Integrate(physics->GetIntegrationMethod(), velocity->GetPositionVelocity(), acceleration, deltaTime);

        // Update positional velocity
        velocity->SetPositionalVelocity(new_vel);

        // ---- Angular / rotational update (with inertia tensor) ----
        // We need the object's orientation to convert world torque into object (body) space.
        ComponentTransform* transform = entity.GetComponent<ComponentTransform>(EComponentType::Component_Transform);
        glm::mat3 R = glm::mat3(1.0f);
        if (transform)
        {
            // Build rotation matrix using same order as ComponentTransform::GetTransformMatrix:
            glm::mat4 rotX = glm::rotate(glm::mat4(1.0f), glm::radians(transform->Rotation().x), glm::vec3(1.0f, 0.0f, 0.0f));
            glm::mat4 rotY = glm::rotate(glm::mat4(1.0f), glm::radians(transform->Rotation().y), glm::vec3(0.0f, 1.0f, 0.0f));
            glm::mat4 rotZ = glm::rotate(glm::mat4(1.0f), glm::radians(transform->Rotation().z), glm::vec3(0.0f, 0.0f, 1.0f));
            glm::mat4 rot = rotZ * rotY * rotX;
            R = glm::mat3(rot);
        }

        // World-space torque
        glm::vec3 tau_world = physics->getTotalTorque();

        // Convert torque into object (body) space: tau_obj = R^T * tau_world
        glm::vec3 tau_obj = glm::transpose(R) * tau_world;

        // Convert angular velocity into object space
        glm::vec3 omega_world = velocity->GetRotationalVelocity();
        glm::vec3 omega_obj = glm::transpose(R) * omega_world;

        // Compute angular acceleration in object space using inverse inertia tensor:
        // alpha_obj = I^{-1} * tau_obj
        glm::vec3 alpha_obj = physics->GetInverseInertiaTensor() * tau_obj;

        // Integrate angular velocity in object space
        glm::vec3 omega_obj_new = Physics::Integration<glm::vec3>::Integrate(physics->GetIntegrationMethod(), omega_obj, alpha_obj, deltaTime);

        // Convert back to world space
        glm::vec3 omega_world_new = R * omega_obj_new;

        velocity->SetRotationalVelocity(omega_world_new);

        // Clear forces and torques for next frame
        physics->ClearForces();
        physics->ClearTorques();
    }
}