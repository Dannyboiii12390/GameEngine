
#include "ComponentCollision.h"
#include "../Entity.h"


void ComponentCollision::InvokeCollision(Entity& ent1, Entity& ent2)
{
    if (m_onCollision) m_onCollision(ent1, ent2);
}