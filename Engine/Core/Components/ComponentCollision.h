#pragma once 

#include "IComponent.h"
#include <glm/glm.hpp>
#include <memory>
#include <functional>

#include "../../PhysicsEngine/Shapes/Sphere.h"
#include "../../PhysicsEngine/Shapes/LineInf.h"
#include "../../PhysicsEngine/Shapes/Capsule.h"
#include "../../PhysicsEngine/Shapes/Cylinder.h"
#include "../../PhysicsEngine/Shapes/Plane.h"
#include <iostream>

class Entity;

class ComponentCollision : public IComponent
{
public:

    using CollisionCallback = std::function<void(Entity&, Entity&)>;

    ComponentCollision() : IComponent(EComponentType::Component_Collision) {}

    // Store the function to call when this collider has collided.
    void SetOnCollision(CollisionCallback cb)
    {
        m_onCollision = std::move(cb);
    }

    // Called by the physics system when a collision occurs.
    // Passes this component and the other collider to the stored callback if present.
    //
    // Pseudocode / Plan:
    // 1. Invoke the stored collision callback if one exists so game logic can respond.
    // 2. To prevent "sticking" due to overlapping colliders, attempt a best-effort positional fixup:
    //    - If `Entity` exposes `GetPosition()` and `SetPosition(const glm::vec3&)`, compute the separation
    //      vector from ent2 to ent1, normalize it, and nudge ent1 slightly along that direction.
    //    - If separation is near zero, apply a small upward nudge to avoid degenerate direction.
    //    - Use a small nudge value (e.g. 0.01f) so we don't visibly teleport the entity.
    // 3. This is a conservative, minimal change to reduce sticking. More robust resolution should be
    //    handled by the physics system (positional correction / penetration resolution).
    void InvokeCollision(Entity& ent1, Entity& ent2);
	Physics::Collider* GetCollider() const { return m_collider.get(); }

    // Replace only the Collided method implementation:
    bool Collided(const Physics::Collider& other) const
    {
        if (!m_collider)
            return false; // No collider means we can't collide with anything.

        switch (other.getType())
        {
            case Physics::EColliderType::SPHERE:
            {
                const Physics::Sphere* sphereOther = dynamic_cast<const Physics::Sphere*>(&other);
                if (sphereOther)
                {
                    return m_collider->isColliding(*sphereOther);
                }   
                return false;
            }
            case Physics::EColliderType::LINEINF:
            {
                const Physics::LineInf* lineOther = dynamic_cast<const Physics::LineInf*>(&other);
                if (lineOther)
                    return m_collider->isColliding(*lineOther);
                return false;
            }
            case Physics::EColliderType::CAPSULE:
            {
                const Physics::Capsule* capOther = dynamic_cast<const Physics::Capsule*>(&other);
                if (capOther)
                    return m_collider->isColliding(*capOther);
                return false;
            }
            case Physics::EColliderType::CYLINDER:
            {
                const Physics::Cylinder* cylOther = dynamic_cast<const Physics::Cylinder*>(&other);
                if (cylOther)
                    return m_collider->isColliding(*cylOther);
                return false;
            }
            case Physics::EColliderType::PLANE:
            {
                const Physics::Plane* planeOther = dynamic_cast<const Physics::Plane*>(&other);
                if (planeOther)
                    return m_collider->isColliding(*planeOther);
                return false;
            }
            default:
                return false; // Unknown collider type, assume no collision.
        }
    }
    void SetCollider(std::unique_ptr<Physics::Collider> collider)
    {
        m_collider = std::move(collider);
	}
private:

    std::unique_ptr<Physics::Collider> m_collider;
    CollisionCallback m_onCollision;
};
