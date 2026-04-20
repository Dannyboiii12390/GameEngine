#include "SystemAnimation.h"

void SystemAnimation::OnUpdate(std::span<Entity> entities, float deltaTime)
{
    for (auto& entity : entities)
    {
        if (!entity.HasComponent(EComponentType::Component_Animation))
            continue;

        auto* animComp = entity.GetComponent<ComponentAnimation>(EComponentType::Component_Animation);
        auto* transformComp = entity.GetComponent<ComponentTransform>(EComponentType::Component_Transform);

        if (!animComp || !transformComp)
            continue;

        // Update animation
        animComp->Update(deltaTime);

        // Get interpolated transform
        glm::vec3 animPos, animRot;
        float alpha;
        animComp->GetInterpolatedTransform(animPos, animRot, alpha);

        // Apply to entity transform
        Transform* writeBuffer = transformComp->WriteBuffer();
        if (writeBuffer)
        {
            writeBuffer->Position = animPos;
            writeBuffer->Rotation = animRot;
        }

        transformComp->SwapBuffers();
    }
}