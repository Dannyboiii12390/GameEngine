#include "SystemSwapBuffers.h"
#include "../Entity.h"

void SystemSwapBuffers::OnUpdate(std::span<Entity> entities, float deltaTime)
{

	for (auto& entity : entities)
	{
		auto* translation = entity.GetComponent<ComponentTransform>(EComponentType::Component_Transform);
		auto* velocity = entity.GetComponent<ComponentVelocity>(EComponentType::Component_Velocity);

		if (translation) translation->SwapBuffers();
		if (velocity) velocity->SwapBuffers();
	}




}
