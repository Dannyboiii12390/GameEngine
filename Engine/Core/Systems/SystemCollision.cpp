#include "SystemCollision.h"
#include "../Components/ComponentCollision.h"
#include "../Components/ComponentTransform.h"
#include "../Components/ComponentVelocity.h"
#include "../Components/ComponentNetwork.h"
#include "../Entity.h"
#include <omp.h>
#include "../../DebugUtils.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace
{
	struct CellCoord
	{
		int x;
		int y;
		int z;

		bool operator==(const CellCoord& other) const
		{
			return x == other.x && y == other.y && z == other.z;
		}
	};

	struct CellCoordHasher
	{
		size_t operator()(const CellCoord& coord) const noexcept
		{
			const size_t hx = static_cast<size_t>(coord.x * 73856093);
			const size_t hy = static_cast<size_t>(coord.y * 19349663);
			const size_t hz = static_cast<size_t>(coord.z * 83492791);
			return hx ^ hy ^ hz;
		}
	};

	struct BroadPhaseBody
	{
		int entityIndex;
		ComponentCollision* collision;
		glm::vec3 min;
		glm::vec3 max;
	};

	uint64_t MakePairKey(int a, int b)
	{
		const uint32_t low = static_cast<uint32_t>(std::min(a, b));
		const uint32_t high = static_cast<uint32_t>(std::max(a, b));
		return (static_cast<uint64_t>(low) << 32u) | high;
	}

	bool BuildAabb(const Physics::Collider& collider, glm::vec3& outMin, glm::vec3& outMax, bool& outInfinite)
	{
		outInfinite = false;

		switch (collider.getType())
		{
		case Physics::EColliderType::SPHERE:
		{
			const auto& sphere = static_cast<const Physics::Sphere&>(collider);
			const glm::vec3 center = sphere.getPos();
			const float radius = sphere.getRadius();
			const glm::vec3 radiusVec(radius);
			outMin = center - radiusVec;
			outMax = center + radiusVec;
			return true;
		}
		case Physics::EColliderType::CAPSULE:
		{
			const auto& capsule = static_cast<const Physics::Capsule&>(collider);
			const glm::vec3 a = capsule.getA();
			const glm::vec3 b = capsule.getB();
			const float radius = capsule.getRadius();
			const glm::vec3 r(radius);
			outMin = glm::min(a, b) - r;
			outMax = glm::max(a, b) + r;
			return true;
		}
		case Physics::EColliderType::CYLINDER:
		{
			const auto& cylinder = static_cast<const Physics::Cylinder&>(collider);
			const glm::vec3 a = cylinder.getA();
			const glm::vec3 b = cylinder.getB();
			const float radius = cylinder.getRadius();
			const glm::vec3 r(radius);
			outMin = glm::min(a, b) - r;
			outMax = glm::max(a, b) + r;
			return true;
		}
		case Physics::EColliderType::LINEINF:
		case Physics::EColliderType::PLANE:
			outInfinite = true;
			return false;
		default:
			return false;
		}
	}
}

void SystemCollision::OnUpdate(std::span<Entity> entities, float deltaTime)
{
    // First, update all collider positions from their transforms
    for (Entity& entity : entities)
    {
        if (!entity.HasComponent(EComponentType::Component_Collision) || !entity.HasComponent(EComponentType::Component_Transform))
            continue;

        auto* colComp = entity.GetComponent<ComponentCollision>(EComponentType::Component_Collision);
        auto* transComp = entity.GetComponent<ComponentTransform>(EComponentType::Component_Transform);

        if (colComp && transComp)
        {
            Physics::Collider* collider = colComp->GetCollider();
            if (collider)
            {
                collider->setPosition(transComp->Position());
                collider->setRotation(transComp->Rotation());
            }
        }
    }

    // Now, proceed with collision detection
    const size_t numEntities = entities.size();
    if (numEntities == 0) return;

    // Simple N-squared collision detection for now
    for (size_t i = 0; i < numEntities; ++i)
    {
        Entity& entityA = entities[i];
        if (!entityA.HasComponent(EComponentType::Component_Collision)) continue;

        auto* colCompA = entityA.GetComponent<ComponentCollision>(EComponentType::Component_Collision);
        if (!colCompA || colCompA->GetCollisionRole() == CollisionRole::Container) continue;

        for (size_t j = i + 1; j < numEntities; ++j)
        {
            Entity& entityB = entities[j];
            if (!entityB.HasComponent(EComponentType::Component_Collision)) continue;

            auto* colCompB = entityB.GetComponent<ComponentCollision>(EComponentType::Component_Collision);
            if (!colCompB) continue;

            Physics::Collider* colliderA = colCompA->GetCollider();
            Physics::Collider* colliderB = colCompB->GetCollider();

            if (colliderA && colliderB && colliderA->isColliding(*colliderB))
            {
                colCompA->InvokeCollision(entityA, entityB);

                // If B is not a container, it can also handle the collision
                if (colCompB->GetCollisionRole() != CollisionRole::Container)
                {
                    colCompB->InvokeCollision(entityB, entityA);
                }
            }
        }
    }
}


