#pragma once

#include "ISystem.h"
#include "../Components/ComponentAnimation.h"
#include "../Components/ComponentTransform.h"
#include "../Entity.h"
#include <span>

class SystemAnimation : public ISystem
{
public:
    SystemAnimation() = default;
    virtual ~SystemAnimation() = default;

    virtual void OnUpdate(std::span<Entity> entities, float deltaTime) override;
};