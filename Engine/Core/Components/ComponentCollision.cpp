
#include "ComponentCollision.h"
#include "../Entity.h"


void ComponentCollision::InvokeCollision(Entity& ent1, Entity& ent2)
{
    if (m_onCollision) m_onCollision(ent1, ent2);

    // reset position of ent1 to prevent sticking (best-effort)
    // This requires Entity to provide GetPosition() and SetPosition(const glm::vec3&).
    // If those functions do not exist in your Entity API, replace this block with
    // the appropriate transform accessors for your engine (e.g. Transform component).
    constexpr float nudge = 0.1f;
    // Guarded usage: assume typical Entity API. If not present, this will need to be adapted.
    // Compute separation and nudge ent1 away from ent2 to reduce overlap.
    {
        // The following calls assume Entity::GetPosition() exists and returns glm::vec3.
        // If your Entity uses a different API, update these calls accordingly.
		ComponentTranslation* trans1 = ent1.GetComponent<ComponentTranslation>(EComponentType::Component_Translation);
        ComponentTranslation* trans2 = ent2.GetComponent<ComponentTranslation>(EComponentType::Component_Translation);
		glm::vec3 pos1 = trans1->Position();
		glm::vec3 pos2 = trans2->Position();

        glm::vec3 sep = pos1 - pos2;
        float len = glm::length(sep);
        if (len > 1e-6f)
        {
            glm::vec3 dir = sep / len;
            trans1->SetPosition(pos1 + dir * nudge);
        }
        else
        {
            // Degenerate case: entities at (almost) same position. Nudge upwards.
            trans1->SetPosition(pos1 + glm::vec3(0.0f, nudge, 0.0f));
        }
    }
}