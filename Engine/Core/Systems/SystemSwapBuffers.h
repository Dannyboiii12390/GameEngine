
#include "ISystem.h"


class SystemSwapBuffers : public ISystem
{
	// Inherited via ISystem
	void OnUpdate(std::span<Entity> entities, float deltaTime) override;
};
