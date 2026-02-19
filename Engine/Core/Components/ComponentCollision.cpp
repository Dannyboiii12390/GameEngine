
#include "ComponentCollision.h"
#include "../Entity.h"


void ComponentCollision::InvokeCollision(Entity& ent1, Entity& ent2)
{
    // Call user callback first so game logic can run
    if (m_onCollision)
        m_onCollision(ent1, ent2);

    // Best-effort positional fixup & velocity/force cleanup to avoid "sticking" or continued motion.
    // Only do a minimal correction for Sphere <-> Plane collisions here.
    //if (!m_collider)
    //    return;

    //Physics::Collider* otherCollider = nullptr;
    //if (ent2.HasComponent(EComponentType::Component_Collision))
    //{
    //    ComponentCollision* otherColComp = ent2.GetComponent<ComponentCollision>(EComponentType::Component_Collision);
    //    if (otherColComp)
    //        otherCollider = otherColComp->GetCollider();
    //}

    //if (!otherCollider)
    //    return;

    //// We only handle the common case: a sphere (this) vs a plane (other).
    //if (m_collider->getType() == Physics::EColliderType::SPHERE && otherCollider->getType() == Physics::EColliderType::PLANE)
    //{
    //    auto* sphere = dynamic_cast<Physics::Sphere*>(m_collider.get());
    //    auto* plane = dynamic_cast<Physics::Plane*>(otherCollider);
    //    if (!sphere || !plane)
    //        return;

    //    // Obtain translation component if present (colliders have been positioned by SystemCollision before this call).
    //    ComponentTranslation* trans = ent1.GetComponent<ComponentTranslation>(EComponentType::Component_Translation);
    //    if (!trans)
    //        return;

    //    glm::vec3 pos = trans->Position();
    //    glm::vec3 proj = plane->projectPoint(pos);
    //    glm::vec3 dir = pos - proj;
    //    float dist = glm::length(dir);
    //    const float EPS = 1e-6f;
    //    glm::vec3 normal;
    //    if (dist > EPS)
    //        normal = dir / dist; // points from plane toward sphere center
    //    else
    //    {
    //        // Degenerate: choose plane normal direction using signed distance sign
    //        float signedD = plane->signedDistance(pos);
    //        // if signedD is zero use a safe up vector
    //        normal = (std::abs(signedD) > EPS) ? glm::normalize(pos - proj) : glm::vec3(0.0f, 1.0f, 0.0f);
    //    }

    //    float penetration = sphere->getRadius() - dist;
    //    if (penetration > 0.0f)
    //    {
    //        // Push sphere out so it's exactly touching the plane
    //        glm::vec3 newPos = pos + normal * penetration;
    //        trans->SetPosition(newPos);
    //        sphere->setPosition(newPos); // keep collider consistent

    //        // Zero velocity if present
    //        if (ent1.HasComponent(EComponentType::Component_Velocity))
    //        {
    //            ComponentVelocity* vel = ent1.GetComponent<ComponentVelocity>(EComponentType::Component_Velocity);
    //            if (vel)
    //                vel->SetPositionalVelocity(glm::vec3(0.0f));
    //        }

    //        // Clear accumulated physics forces if present
    //        if (ent1.HasComponent(EComponentType::Component_Physics))
    //        {
    //            ComponentPhysics* phys = ent1.GetComponent<ComponentPhysics>(EComponentType::Component_Physics);
    //            if (phys)
    //                phys->ClearForces();
    //        }
    //    }
    //}

    // Note: you can extend this to handle the swapped case (plane on ent1 and sphere on ent2)
    // and more complex collision resolution (impulses, restitution).
}