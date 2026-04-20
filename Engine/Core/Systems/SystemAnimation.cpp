#include "SystemAnimation.h"
#include "../Entity.h"
#include "../Components/ComponentTransform.h"
#include "../Components/ComponentAnimation.h"
#include "../Components/ComponentCollision.h"
#include <iostream>

void SystemAnimation::OnUpdate(std::span<Entity> entities, float deltaTime)
{
    static int callCount = 0;
    if (callCount++ % 100 == 0)
    {
        std::cout << "SystemAnimation::OnUpdate called! EntityCount: " << entities.size() 
                  << " DeltaTime: " << deltaTime << std::endl;
    }

    for (auto& entity : entities)
    {
        // Skip entities without animation component
        if (!entity.HasComponent(EComponentType::Component_Animation))
            continue;

        auto* animComp = entity.GetComponent<ComponentAnimation>(EComponentType::Component_Animation);
        auto* transformComp = entity.GetComponent<ComponentTransform>(EComponentType::Component_Transform);
        auto* collisionComp = entity.GetComponent<ComponentCollision>(EComponentType::Component_Collision);

        if (!animComp || !transformComp)
            continue;

        // Update the animation state (advances m_currentTime)
        animComp->Update(deltaTime);

        // Get the interpolated transform from the animation
        glm::vec3 animPos, animRot;
        float alpha;
        animComp->GetInterpolatedTransform(animPos, animRot, alpha);

        // Debug: Print animated position
        static int posCallCount = 0;
        if (posCallCount++ % 100 == 0 && animComp->IsPlaying())
        {
            std::cout << "Animated Position: (" << animPos.x << ", " << animPos.y << ", " << animPos.z 
                      << ") Alpha: " << alpha << std::endl;
        }

        // Apply animated position and rotation to entity transform's write buffer
        Transform* writeBuffer = transformComp->WriteBuffer();
        if (writeBuffer)
        {
            writeBuffer->Position = animPos;
            writeBuffer->Rotation = animRot;
            // Keep the scale from the original transform
            const Transform* readBuffer = transformComp->ReadBuffer();
            if (readBuffer)
            {
                writeBuffer->Scale = readBuffer->Scale;
            }
        }

        // Update collider position to match the animated position
        if (collisionComp)
        {
            auto* collider = collisionComp->GetCollider();
            if (collider)
            {
                collider->setPosition(animPos);
            }
        }
    }
}