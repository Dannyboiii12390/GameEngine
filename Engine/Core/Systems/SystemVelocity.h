#pragma once

#include "ISystem.h"
#include <chrono>
#include <span>
#include "../Components/IComponent.h"
#include "../Entity.h"
#include <omp.h>

class Entity;

class SystemVelocity : public ISystem
{
public:
	SystemVelocity() : ISystem() { m_SystemType = ESystemType::System_Velocity; }

	void OnUpdate(std::span<Entity> entities, float deltaTime) override
	{
		const int count = static_cast<int>(entities.size());

		// Each entity's velocity/transform state is independent — safe to parallelise.
		#pragma omp parallel for
		for (int i = 0; i < count; ++i)
		{
			auto& entity = entities[i];

			EComponentType type = EComponentType::Component_Velocity | EComponentType::Component_Transform;
			if (!entity.HasComponent(type))
				continue;

			ComponentVelocity* velocity     = entity.GetComponent<ComponentVelocity>(EComponentType::Component_Velocity);
			ComponentTransform* translation = entity.GetComponent<ComponentTransform>(EComponentType::Component_Transform);

			// Snapshot position before integration so CollisionResponse can restore it.
			translation->SavePreviousPosition();

			glm::vec3 deltaPos   = velocity->GetPositionVelocity()   * static_cast<float>(deltaTime);
			glm::vec3 deltaRot   = velocity->GetRotationalVelocity() * static_cast<float>(deltaTime);
			glm::vec3 deltaScale = velocity->GetScaleVelocity()      * static_cast<float>(deltaTime);

			translation->ChangePosition(deltaPos);
			translation->ChangeRotation(deltaRot);
			translation->ChangeScale(deltaScale);

			// Seed the velocity write buffer from the current read buffer so
			// SystemPhysics always starts from the last committed velocity.
			// SystemPhysics writes its result on top and owns the velocity swap.
			velocity->SetPositionalVelocity(velocity->GetPositionVelocity());
			velocity->SetRotationalVelocity(velocity->GetRotationalVelocity());
			velocity->SetScalarVelocity(velocity->GetScaleVelocity());
		}
	}
};