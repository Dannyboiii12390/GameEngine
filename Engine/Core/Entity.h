#pragma once
#include <vector>
#include <memory>

#include "Components/IComponent.h"
#include "Components/ComponentTransform.h"
#include "Components/ComponentVelocity.h"
#include "Components/ComponentGeometry.h"
#include "Components/ComponentPhysics.h"
#include "Components/ComponentCollision.h"
#include "Components/ComponentNetwork.h"
#include "Components/ComponentAnimation.h"

#include <iostream>

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
	void AddComponent(EComponentType type, Args&&... args) 
	{
		switch (type)
		{
			case EComponentType::Component_Transform:
			{
				if constexpr (std::is_constructible_v<ComponentTransform, Args...>)
				{
					auto component = std::make_unique<ComponentTransform>(std::forward<Args>(args)...);
					m_Components[type] = std::move(component);
					m_EntityType = static_cast<EComponentType>(to_mask(m_EntityType) | to_mask(EComponentType::Component_Transform));
				}
				break;
			}
			case EComponentType::Component_Velocity:
			{
				if constexpr (std::is_constructible_v<ComponentVelocity, Args...>)
				{
					auto component = std::make_unique<ComponentVelocity>(std::forward<Args>(args)...);
					m_Components[type] = std::move(component);
					m_EntityType = static_cast<EComponentType>(to_mask(m_EntityType) | to_mask(EComponentType::Component_Velocity));
				}
				break;
			}
			case EComponentType::Component_Geometry:
			{
				if constexpr (std::is_constructible_v<ComponentGeometry, Args...>)
				{
					auto component = std::make_unique<ComponentGeometry>(std::forward<Args>(args)...);
					m_Components[type] = std::move(component);
					m_EntityType = static_cast<EComponentType>(to_mask(m_EntityType) | to_mask(EComponentType::Component_Geometry));
				}
				break;
			}
			case EComponentType::Component_Physics:
			{
				if constexpr (std::is_constructible_v<ComponentPhysics, Args...>)
				{
					auto component = std::make_unique<ComponentPhysics>(std::forward<Args>(args)...);
					m_Components[type] = std::move(component);
					m_EntityType = static_cast<EComponentType>(to_mask(m_EntityType) | to_mask(EComponentType::Component_Physics));
				}
				break;
			}
			case EComponentType::Component_Collision:
			{
				if constexpr (std::is_constructible_v<ComponentCollision, Args...>)
				{
					auto component = std::make_unique<ComponentCollision>(std::forward<Args>(args)...);
					m_Components[type] = std::move(component);
					m_EntityType = static_cast<EComponentType>(to_mask(m_EntityType) | to_mask(EComponentType::Component_Collision));
				}
				break;
			}
			case EComponentType::Component_Network:
			{
				if constexpr (std::is_constructible_v<ComponentNetwork, Args...>)
				{
					auto component = std::make_unique<ComponentNetwork>(std::forward<Args>(args)...);
					m_Components[type] = std::move(component);
					m_EntityType = static_cast<EComponentType>(to_mask(m_EntityType) | to_mask(EComponentType::Component_Network));
				}
				break;
			}
			case EComponentType::Component_Animation:
			{
				if constexpr (std::is_constructible_v<ComponentAnimation, Args...>)
				{
					auto component = std::make_unique<ComponentAnimation>(std::forward<Args>(args)...);
					m_Components[type] = std::move(component);
					m_EntityType = static_cast<EComponentType>(to_mask(m_EntityType) | to_mask(EComponentType::Component_Animation));
				}
				break;
			}
			default:
				std::cout << "Unsupported component type " << std::endl;
		}
	}
	void Destroy()
	{
		for (auto& comp : m_Components)
		{
			if (HasComponent(EComponentType::Component_Geometry))
			{
				auto* geom = GetComponent<ComponentGeometry>(EComponentType::Component_Geometry);
				geom->Destroy();
			}
		}
		m_Components.clear();
	}

	// Retrieve first component with matching type (non-template)
	IComponent* GetComponent(EComponentType type)
	{
		auto it = m_Components.find(type);
		if (it != m_Components.end())
			return it->second.get();
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
		size_t erased = m_Components.erase(type);
		if (erased > 0)
		{
			m_EntityType = static_cast<EComponentType>(to_mask(m_EntityType) & ~to_mask(type));
		}
	}

	// returns true if ALL bits in `type` are present in m_EntityType
	bool HasComponent(EComponentType type) const noexcept
	{
		return (to_mask(m_EntityType) & to_mask(type)) == to_mask(type);
	}

private:
	std::unordered_map<EComponentType, std::unique_ptr<IComponent>> m_Components;
	EComponentType m_EntityType = EComponentType::Component_None;


};