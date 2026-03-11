

#include "SystemManager.h"

void SystemManager::Update(std::span<Entity> entities, float dt)
{
	for(auto& system : m_systems)
	{
		system->OnUpdate(entities, dt);
	}
}