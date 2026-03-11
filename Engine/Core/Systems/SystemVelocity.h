#pragma once

#include "ISystem.h"
#include <chrono>
#include <span>
#include "../Components/IComponent.h"
#include "../Entity.h"

#include <algorithm>
#include <execution>

class Entity;

class SystemVelocity : public ISystem
{
public:
	SystemVelocity() : ISystem() { m_SystemType = ESystemType::System_Velocity; }

	void OnUpdate(std::span<Entity> entities, float deltaTime) override
	{

		auto velForEntity = [deltaTime](Entity& entity)
		{
			EComponentType type = EComponentType::Component_Velocity | EComponentType::Component_Transform;
			if (entity.HasComponent(type))
			{
				ComponentVelocity* velocity = entity.GetComponent<ComponentVelocity>(EComponentType::Component_Velocity);
				ComponentTransform* translation = entity.GetComponent<ComponentTransform>(EComponentType::Component_Transform);

				// Snapshot position before integration so CollisionResponse can restore it.
				translation->SavePreviousPosition();

				glm::vec3 deltaPos = velocity->GetPositionVelocity() * static_cast<float>(deltaTime);
				glm::vec3 deltaRot = velocity->GetRotationalVelocity() * static_cast<float>(deltaTime);
				glm::vec3 deltaScale = velocity->GetScaleVelocity() * static_cast<float>(deltaTime);

				translation->ChangePosition(deltaPos);
				translation->ChangeRotation(deltaRot);
				translation->ChangeScale(deltaScale);

				translation->SwapBuffers();
			}
		};
		std::for_each(std::execution::par_unseq, entities.begin(), entities.end(), velForEntity);
		
	}
};