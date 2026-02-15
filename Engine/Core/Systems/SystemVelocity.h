#pragma once

#include "ISystem.h"
#include <chrono>
#include <span>
#include "../Components/IComponent.h"
#include "../Entity.h"

class Entity;

class SystemVelocity : public ISystem
{
public:
	SystemVelocity() : ISystem() { m_SystemType = ESystemType::System_Velocity; }

	SystemVelocity(std::span<Entity> ents) : ISystem()
	{ 
		m_SystemType = ESystemType::System_Velocity; 
		m_Entities = ents;
	} 
	void OnUpdate(float deltaTime) 
	{
		for(auto& entity : m_Entities)
		{
			EComponentType type = EComponentType::Component_Velocity | EComponentType::Component_Translation;
			if(entity.HasComponent(type))
			{
				ComponentVelocity* velocity = entity.GetComponent<ComponentVelocity>(EComponentType::Component_Velocity);
				ComponentTranslation* translation = entity.GetComponent<ComponentTranslation>(EComponentType::Component_Translation);
				
				glm::vec3 deltaPos = velocity->GetPositionVelocity() * static_cast<float>(deltaTime);
				glm::vec3 deltaRot = velocity->GetRotationalVelocity() * static_cast<float>(deltaTime);
				glm::vec3 deltaScale = velocity->GetScaleVelocity() * static_cast<float>(deltaTime);
				
				translation->ChangePosition(deltaPos);
				translation->ChangeRotation(deltaRot);
				translation->ChangeScale(deltaScale);
			}
		}
	}

private:

	std::span<Entity> m_Entities;

};