#include "SystemCollision.h"
#include "../Components/ComponentCollision.h"
#include "../Entity.h"
#include <execution>



void SystemCollision::OnUpdate(std::span<Entity> entities, float deltaTime)
{
	auto checkCollisionForEntity = [&entities](Entity& entity)
	{
			EComponentType requiredComponents = EComponentType::Component_Collision | EComponentType::Component_Transform;
		if (!entity.HasComponent(requiredComponents))
		{
			return;
		}
		ComponentCollision* collisionComp = entity.GetComponent<ComponentCollision>(EComponentType::Component_Collision);
		ComponentTransform* thisTransform = entity.GetComponent<ComponentTransform>(EComponentType::Component_Transform);
		auto* collider = collisionComp->GetCollider();
		collider->setPosition(thisTransform->Position());
		collider->setRotation(thisTransform->Rotation());
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
	};
	std::for_each(std::execution::par_unseq, entities.begin(), entities.end(), checkCollisionForEntity);
	/*
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
	*/
}

