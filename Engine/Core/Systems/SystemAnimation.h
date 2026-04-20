#pragma once

#include "ISystem.h"
#include <span>

class Entity;

class SystemAnimation : public ISystem
{
public:
    SystemAnimation() : ISystem() {}
    virtual ~SystemAnimation() = default;

    virtual void OnUpdate(std::span<Entity> entities, float deltaTime) override;
};