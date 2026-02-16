#include "SystemPhysics.h"


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

        //velocity->SetPositionalVelocity(velocity->GetPositionVelocity() + acceleration * deltaTime);
        velocity->SetPositionalVelocity(new_vel);

        // Clear forces for next frame
        physics->ClearForces();
    }
}