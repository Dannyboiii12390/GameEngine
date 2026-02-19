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

    void InvokeCollision(Entity& ent1, Entity& ent2);
	Physics::Collider* GetCollider() const { return m_collider.get(); }

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
