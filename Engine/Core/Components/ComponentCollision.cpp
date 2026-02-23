
#include "ComponentCollision.h"
#include "../Entity.h"


void ComponentCollision::InvokeCollision(Entity& ent1, Entity& ent2)
{
    // Call user callback first so game logic can run
    if (m_onCollision)
        m_onCollision(ent1, ent2);
}