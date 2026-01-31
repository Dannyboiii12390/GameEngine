#pragma once
#include <vector>
#include <memory>
#include <type_traits>
#include "Components/IComponent.h"
#include "Components/ComponentTranslation.h"
#include "Components/ComponentVelocity.h"

class Entity {

public:

	template<typename... Args>
	constexpr void AddComponent(EComponentType type, Args&&... args)
	{
		std::unique_ptr<IComponent> component;
		switch (type)
		{
		case EComponentType::Component_Translation:
			component = std::make_unique<ComponentTranslation>(std::forward<Args>(args)...);
			m_EntityType |= EComponentType::Component_Translation;
			break;
		case EComponentType::Component_Velocity:
			component = std::make_unique<ComponentVelocity>(std::forward<Args>(args)...);
			m_EntityType |= EComponentType::Component_Velocity;
			break;
		}
		m_Components.push_back(std::move(component));

		// set the bit(s) for this component
		m_EntityType = static_cast<EComponentType>(to_mask(m_EntityType) | to_mask(type));
	}

	template<typename T>
	T* GetComponent(EComponentType type)
	{
		IComponent* comp = GetComponent(type);
		return dynamic_cast<T*>(comp);
	}

	void RemoveComponent(EComponentType type)
	{
		for(auto it = m_Components.begin(); it != m_Components.end(); ++it)
		{
			if((*it)->GetType() == type)
			{
				m_Components.erase(it);
				// clear the bit(s) for this component
				m_EntityType = static_cast<EComponentType>(to_mask(m_EntityType) & ~to_mask(type));
				return;
			}
		}
	}

	// returns true if ALL bits in `type` are present in m_EntityType
	bool HasComponent(EComponentType type) const noexcept
	{
		return (to_mask(m_EntityType) & to_mask(type)) == to_mask(type);
	}

private:

	std::vector<std::unique_ptr<IComponent>> m_Components;
	EComponentType m_EntityType = EComponentType::Component_None;
};