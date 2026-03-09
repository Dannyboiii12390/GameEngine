#include "SystemCollision.h"
#include "../Components/ComponentCollision.h"
#include "../Entity.h"

void SystemCollision::OnUpdate(std::span<Entity> entities, float deltaTime)
{
	EComponentType requiredComponents = EComponentType::Component_Collision | EComponentType::Component_Transform;

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
			
			ComponentTransform* thisTransform = entity.GetComponent<ComponentTransform>(EComponentType::Component_Transform);
			glm::vec3 thisNewPos = thisTransform->Position();
			collisionComp->GetCollider()->setPosition(thisNewPos);

			bool isColliding = collisionComp->Collided(*otherCollision->GetCollider());
			if(isColliding)
			{
				collisionComp->InvokeCollision(entity, other);
			}
		}
	}
}
