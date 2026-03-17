#include "SystemCollision.h"
#include "../Components/ComponentCollision.h"
#include "../Components/ComponentTransform.h"
#include "../Components/ComponentVelocity.h"
#include "../Entity.h"
#include <omp.h>
#include "../../DebugUtils.h"

//#define USE_COMPUTE // Comment out to use CPU-based collision detection instead of GPU compute shader


void SystemCollision::OnUpdate(std::span<Entity> entities, float deltaTime)
{
	EComponentType requiredComponents = EComponentType::Component_Collision | EComponentType::Component_Transform;

	const int count = static_cast<int>(entities.size());

	// Sync collider positions from the committed (read) transform buffer.
	// Each entity is independent — safe to parallelise.
	#pragma omp parallel for
	for (int i = 0; i < count; ++i)
	{
		Entity& entity = entities[i];

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
	#pragma omp parallel for 
	for (int i = 0; i < count; ++i)
	{
		Entity& entity = entities[i];

		if (!entity.HasComponent(requiredComponents))
			continue;

		ComponentCollision* collisionComp = entity.GetComponent<ComponentCollision>(EComponentType::Component_Collision);

		for (int j = 0; j < count; ++j)
		{
			if (i == j) continue;

			Entity& other = entities[j];

			if (!other.HasComponent(EComponentType::Component_Collision))
				continue;

			ComponentCollision* otherCollision = other.GetComponent<ComponentCollision>(EComponentType::Component_Collision);

			if (collisionComp->Collided(*otherCollision->GetCollider()))
			{
				#pragma omp critical
				collisionComp->InvokeCollision(entity, other); // this is the main bottleneck when using CPU-based collision detection, as the collision response may write to shared buffers. 
				//In a real implementation, we would want to design this to minimize contention (e.g. by accumulating responses in thread-local storage and applying them after the parallel loop).
			}
		}
	}
}


