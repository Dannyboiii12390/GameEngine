#include "SystemCollision.h"
#include "../Components/ComponentCollision.h"
#include "../Entity.h"




void SystemCollision::OnUpdate(std::span<Entity> entities, float deltaTime)
{
	EComponentType requiredComponents = EComponentType::Component_Collision | EComponentType::Component_Transform;


	for (Entity& entity : entities)
	{
		if (!entity.HasComponent(requiredComponents))
		{
			continue;
		}
		ComponentCollision* collisionComp = entity.GetComponent<ComponentCollision>(EComponentType::Component_Collision);
		ComponentTransform* thisTransform = entity.GetComponent<ComponentTransform>(EComponentType::Component_Transform);

		auto* collider = collisionComp->GetCollider();
		collider->setPosition(thisTransform->Position());
		collider->setRotation(thisTransform->Rotation());

	}
	for(Entity& entity : entities)
	{
		if (!entity.HasComponent(requiredComponents))
		{
			continue;
		}
		ComponentCollision* collisionComp = entity.GetComponent<ComponentCollision>(EComponentType::Component_Collision);
		for(Entity& other : entities)
		{
			if (&entity == &other) continue; // skip self
			if (!other.HasComponent(EComponentType::Component_Collision))
			{
				continue;
			}
			ComponentCollision* otherCollision = other.GetComponent<ComponentCollision>(EComponentType::Component_Collision);

			bool isColliding = collisionComp->Collided(*otherCollision->GetCollider());
			if(isColliding)
			{
				collisionComp->InvokeCollision(entity, other);
			}
		}
	}
}

