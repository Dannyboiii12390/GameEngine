#pragma once

#include "ISystem.h"

class SystemCollision : public ISystem
{
public:
	// Inherited via ISystem
	void OnUpdate(std::span<Entity> entities, float deltaTime) override;
};
