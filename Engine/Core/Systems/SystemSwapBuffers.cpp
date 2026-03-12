#include "SystemSwapBuffers.h"
#include "../Entity.h"
#include <omp.h>

void SystemSwapBuffers::OnUpdate(std::span<Entity> entities, float deltaTime)
{
	const int count = static_cast<int>(entities.size());

	// Each entity's buffer swap is independent — safe to parallelise.
	#pragma omp parallel for
	for (int i = 0; i < count; ++i)
	{
		auto& entity = entities[i];

		auto* translation = entity.GetComponent<ComponentTransform>(EComponentType::Component_Transform);
		auto* velocity    = entity.GetComponent<ComponentVelocity>(EComponentType::Component_Velocity);

		if (translation) translation->SwapBuffers();
		if (velocity)    velocity->SwapBuffers();
	}
}
