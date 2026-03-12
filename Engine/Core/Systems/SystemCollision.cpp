#include "SystemCollision.h"
#include "../Components/ComponentCollision.h"
#include "../Components/ComponentTransform.h"
#include "../Components/ComponentVelocity.h"
#include "../Entity.h"

void SystemCollision::OnUpdate(std::span<Entity> entities, float deltaTime)
{
	EComponentType requiredComponents = EComponentType::Component_Collision | EComponentType::Component_Transform;

	// Sync collider positions from the committed (read) transform buffer.
	for (Entity& entity : entities)
	{
		if (!entity.HasComponent(requiredComponents))
			continue;

		ComponentCollision* collisionComp = entity.GetComponent<ComponentCollision>(EComponentType::Component_Collision);
		ComponentTransform* thisTransform = entity.GetComponent<ComponentTransform>(EComponentType::Component_Transform);

		collisionComp->GetCollider()->setPosition(thisTransform->Position());
		collisionComp->GetCollider()->setRotation(thisTransform->Rotation());
	}

	// Detect and respond. CollisionResponse writes corrections into the write
	// buffer. SystemVelocity owns all buffer swapping at the start of the
	// next frame, so no swaps are needed here.
	for (Entity& entity : entities)
	{
		if (!entity.HasComponent(requiredComponents))
			continue;

		ComponentCollision* collisionComp = entity.GetComponent<ComponentCollision>(EComponentType::Component_Collision);

		for (Entity& other : entities)
		{
			if (&entity == &other) continue;
			if (!other.HasComponent(EComponentType::Component_Collision))
				continue;

			ComponentCollision* otherCollision = other.GetComponent<ComponentCollision>(EComponentType::Component_Collision);

			if (collisionComp->Collided(*otherCollision->GetCollider()))
				collisionComp->InvokeCollision(entity, other);
		}
	}
}

