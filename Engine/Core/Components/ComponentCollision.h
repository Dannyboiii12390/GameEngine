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

void CollisionResponse(Entity& self, Entity& other);

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

		return m_collider->isColliding(other);
    }
    void SetCollider(std::unique_ptr<Physics::Collider> collider)
    {
        m_collider = std::move(collider);
	}
private:

    std::unique_ptr<Physics::Collider> m_collider;
    CollisionCallback m_onCollision = CollisionResponse;
};


