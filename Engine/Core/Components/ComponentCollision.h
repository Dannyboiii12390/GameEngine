#pragma once 

#include "IComponent.h"
#include <glm/glm.hpp>
#include <memory>
#include <functional>
#include <cstdint>
#include <string>

#include "../../PhysicsEngine/Shapes/Sphere.h"
#include "../../PhysicsEngine/Shapes/LineInf.h"
#include "../../PhysicsEngine/Shapes/Capsule.h"
#include "../../PhysicsEngine/Shapes/Cylinder.h"
#include "../../PhysicsEngine/Shapes/Plane.h"
#include <iostream>

class Entity;

void CollisionResponse(Entity& self, Entity& other);

enum class CollisionRole : uint8_t
{
    Solid = 0,
    Container = 1
};

struct MaterialInteractionCoefficients
{
    float restitution = 0.85f;
    float staticFriction = 0.5f;
    float dynamicFriction = 0.3f;
};

void ClearMaterialInteractions();
void RegisterMaterialInteraction(
    const std::string& materialA,
    const std::string& materialB,
    const MaterialInteractionCoefficients& coeffs);
MaterialInteractionCoefficients GetMaterialInteraction(
    const std::string& materialA,
    const std::string& materialB);

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
            return false;

        return m_collider->isColliding(other);
    }

    void SetCollider(std::unique_ptr<Physics::Collider> collider)
    {
        m_collider = std::move(collider);
    }

    void SetCollisionRole(CollisionRole role)
    {
        m_collisionRole = role;
    }

    CollisionRole GetCollisionRole() const
    {
        return m_collisionRole;
    }

private:

    std::unique_ptr<Physics::Collider> m_collider;
    CollisionCallback m_onCollision = CollisionResponse;
    CollisionRole m_collisionRole = CollisionRole::Solid;
};


