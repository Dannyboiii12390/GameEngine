#pragma once
#include <vector>
#include <memory>
#include <type_traits>
#include <glm/glm.hpp>
#include "Components/IComponent.h"
#include "Components/ComponentTranslation.h"
#include "Components/ComponentVelocity.h"
#include "Components/ComponentGeometry.h"

class Entity {

public:
	Entity() = default;
	// Make Entity non-copyable to avoid implicit copy that tries to copy unique_ptrs
	Entity(const Entity&) = delete;
	Entity& operator=(const Entity&) = delete;

	// Movable
	Entity(Entity&&) noexcept = default;
	Entity& operator=(Entity&&) noexcept = default;

	template<typename... Args>
	void AddComponent(EComponentType type, Args... args) 
	{
		switch (type)
		{
			case EComponentType::Component_Translation:
			{
				auto component = std::make_unique<ComponentTranslation>(args...);
				m_Components.push_back(std::move(component));
				m_EntityType = static_cast<EComponentType>(to_mask(m_EntityType) | to_mask(EComponentType::Component_Translation));
				break;
			}
			case EComponentType::Component_Velocity:
			{
				auto component = std::make_unique<ComponentVelocity>(args...);
				m_Components.push_back(std::move(component));
				m_EntityType = static_cast<EComponentType>(to_mask(m_EntityType) | to_mask(EComponentType::Component_Velocity));
				break;
			}
			case EComponentType::Component_Geometry:
			{
				auto component = std::make_unique<ComponentGeometry>();
				m_Components.push_back(std::move(component));
				m_EntityType = static_cast<EComponentType>(to_mask(m_EntityType) | to_mask(EComponentType::Component_Geometry));
				break;
			}
		}
	}
	// Retrieve first component with matching type (non-template)
	IComponent* GetComponent(EComponentType type)
	{
		for (const auto& comp : m_Components)
		{
			if (comp->GetType() == type)
				return comp.get();
		}
		return nullptr;
	}

	// Typed getter using runtime dynamic_cast (IComponent is polymorphic)
	template<typename T>
	T* GetComponent(EComponentType type)
	{
		IComponent* comp = GetComponent(type);
		return dynamic_cast<T*>(comp);
	}

	void RemoveComponent(EComponentType type)
	{
		for (auto it = m_Components.begin(); it != m_Components.end(); ++it)
		{
			if ((*it)->GetType() == type)
			{
				m_Components.erase(it);
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