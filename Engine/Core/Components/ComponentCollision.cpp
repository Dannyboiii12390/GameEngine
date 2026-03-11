
#include "ComponentCollision.h"
#include "../Entity.h"
#include "../../../PhysicsEngine/Maths/CollisionResolution/ConservationOfMomentum.h"
#include "../../../PhysicsEngine/Maths/CollisionResolution/CollisionResolution.h"

void CollisionResponse(Entity& self, Entity& other)
{
	auto* selfColComp = self.GetComponent<ComponentCollision>(EComponentType::Component_Collision);
	auto* col = selfColComp->GetCollider();
	if (col->getType() != Physics::EColliderType::SPHERE)
		return;

	// Gather collider pointer for other entity
	Physics::Collider* otherCollider = nullptr;
	if (other.HasComponent(EComponentType::Component_Collision))
	{
		ComponentCollision* otherColComp = other.GetComponent<ComponentCollision>(EComponentType::Component_Collision);
		if (otherColComp)
			otherCollider = otherColComp->GetCollider();
	}
	if (!otherCollider)
		return;

	// Required components on self to update velocity/position
	ComponentTransform* transSelf = self.GetComponent<ComponentTransform>(EComponentType::Component_Transform);
	ComponentVelocity* velSelf = self.GetComponent<ComponentVelocity>(EComponentType::Component_Velocity);
	ComponentPhysics* physSelf = self.GetComponent<ComponentPhysics>(EComponentType::Component_Physics);

	if (!transSelf || !velSelf)
		return;

	// Current data for self
	glm::vec3 posSelf = transSelf->Position();
	glm::vec3 vSelf = velSelf->GetPositionVelocity();
	float massSelf = (physSelf) ? physSelf->GetMass() : 1.0f;
	float invMassSelf = (physSelf) ? physSelf->GetInverseMass() : 1.0f;

	// compute contact normal from other -> points from other surface toward sphere center
	glm::vec3 contactNormal(0.0f);
	const float EPS = 1e-6f;

	if (otherCollider->getType() == Physics::EColliderType::PLANE)
	{
		auto* plane = dynamic_cast<Physics::Plane*>(otherCollider);
		if (!plane) return;

		glm::vec3 proj = plane->projectPoint(posSelf);
		glm::vec3 dir = posSelf - proj;
		float dist = glm::length(dir);
		if (dist > EPS)
			contactNormal = dir / dist;
		else
		{
			float signedD = plane->signedDistance(posSelf);
			contactNormal = (std::abs(signedD) > EPS) ? glm::normalize(posSelf - proj) : glm::vec3(0.0f, 1.0f, 0.0f);
		}
	}
	else if (otherCollider->getType() == Physics::EColliderType::SPHERE)
	{
		auto* otherSphere = dynamic_cast<Physics::Sphere*>(otherCollider);
		if (!otherSphere) return;
		glm::vec3 dir = posSelf - otherSphere->getPos();
		float len = glm::length(dir);
		contactNormal = (len > EPS) ? (dir / len) : glm::vec3(0.0f, 1.0f, 0.0f);
	}
	else if (otherCollider->getType() == Physics::EColliderType::CYLINDER)
	{
		auto* cyl = dynamic_cast<Physics::Cylinder*>(otherCollider);
		if (!cyl) return;

		glm::vec3 A = cyl->getA();
		glm::vec3 B = cyl->getB();
		glm::vec3 AB = B - A;
		float L = glm::length(AB);
		if (L <= EPS)
		{
			glm::vec3 diff = posSelf - A;
			float dlen = glm::length(diff);
			contactNormal = (dlen > EPS) ? diff / dlen : glm::vec3(0.0f, 1.0f, 0.0f);
		}
		else
		{
			glm::vec3 u = AB / L;
			float t_raw = glm::dot(posSelf - A, u);
			float t_clamped = std::clamp(t_raw, 0.0f, L);
			glm::vec3 closest = A + u * t_clamped;
			glm::vec3 diff = posSelf - closest;
			float dlen = glm::length(diff);
			contactNormal = (dlen > EPS) ? diff / dlen : glm::vec3(0.0f, 1.0f, 0.0f);
		}
	}
	else
	{
		return;
	}

	// Only resolve if the entity is actually moving toward the surface.
	// This prevents re-triggering the response every frame when resting against a surface.
	const float approachSpeed = glm::dot(vSelf, contactNormal);
	if (approachSpeed >= 0.0f)
		return;

	// --- energy loss parameters ------------------------------------------------
	// Fraction of velocity to keep after a collision (1.0 = perfectly elastic).
	constexpr float k_velocityRetention = 0.9f; // each collision reduces velocity magnitude by 10%
	// Fraction of accumulated force to re-apply after clearing forces (simulates loss of stored impulse)
	constexpr float k_forceRetention = 0.8f; // keep 80% of pre-collision accumulated force
	// Thresholds to stop tiny persistent oscillations (zero-out small velocities/forces)
	constexpr float k_velocityStopThreshold = 0.5f;      // velocity magnitude below this -> stop
	constexpr float k_normalStopThreshold = 0.08f;       // small normal component will be removed
	constexpr float k_forceNormalStopThreshold = 0.2f;   // small normal force component ignored
	// --------------------------------------------------------------------------

	// Save accumulated forces so we can re-apply a reduced amount after clearing.
	glm::vec3 savedForceSelf = (physSelf) ? physSelf->getTotalForce() : glm::vec3(0.0f);

	// If the other entity participates in physics with non-zero inverse mass -> two-body resolution
	ComponentPhysics* physOther = other.GetComponent<ComponentPhysics>(EComponentType::Component_Physics);
	ComponentVelocity* velOther = other.GetComponent<ComponentVelocity>(EComponentType::Component_Velocity);

	glm::vec3 savedForceOther = (physOther) ? physOther->getTotalForce() : glm::vec3(0.0f);

	// reuse contact normal for both branches
	glm::vec3 n = contactNormal;

	bool otherHasFiniteMass = (physOther && physOther->GetInverseMass() > 0.0f);
	if (otherHasFiniteMass)
	{
		float massOther = physOther->GetMass();
		glm::vec3 vOther = (velOther) ? velOther->GetPositionVelocity() : glm::vec3(0.0f);

		glm::vec3 u1n = glm::dot(vSelf, n) * n;
		glm::vec3 u1t = vSelf - u1n;
		glm::vec3 u2n = glm::dot(vOther, n) * n;
		glm::vec3 u2t = vOther - u2n;

		auto [v1_after, v2_after] = Physics::InElasticCollision(u1n, u2n, massSelf, massOther);

		// Apply velocity retention to simulate inelastic collisions (loss of kinetic energy)
		glm::vec3 newV1 = (v1_after + u1t) * k_velocityRetention;
		glm::vec3 newV2 = (v2_after + u2t) * k_velocityRetention;

		// Clamp small normal oscillations and stop tiny overall velocities to avoid indefinite bouncing
		if (glm::length(newV1) < k_velocityStopThreshold)
			newV1 = glm::vec3(0.0f);
		else
		{
			float vn1 = glm::dot(newV1, n);
			if (std::abs(vn1) < k_normalStopThreshold)
				newV1 -= vn1 * n;
		}

		velSelf->SetPositionalVelocity(newV1);

		if (velOther)
		{
			if (glm::length(newV2) < k_velocityStopThreshold)
				newV2 = glm::vec3(0.0f);
			else
			{
				float vn2 = glm::dot(newV2, n);
				if (std::abs(vn2) < k_normalStopThreshold)
					newV2 -= vn2 * n;
			}
			velOther->SetPositionalVelocity(newV2);
		}
	}
	else
	{
		// For collisions with fixed geometry, use a restitution < 1 to reduce bounce;
		// retention still applied afterwards for material damping.
		float restitution = 0.85f;
		glm::vec3 newV = Physics::ResolveVelocityAgainstFixedObject(vSelf, restitution, n);
		// reduce velocity to simulate inelastic impact with fixed object
		newV *= k_velocityRetention;

		// Clamp small oscillations and stop tiny velocities
		if (glm::length(newV) < k_velocityStopThreshold)
			newV = glm::vec3(0.0f);
		else
		{
			float vn = glm::dot(newV, n);
			if (std::abs(vn) < k_normalStopThreshold)
				newV -= vn * n;
		}

		velSelf->SetPositionalVelocity(newV);
	}

	// Restore the entity to its pre-integration position, then push it a small
	// epsilon along the contact normal so it starts the next frame clearly outside
	// the surface and does not immediately re-trigger the collision response.
	const float separationBias = 0.01f;
	glm::vec3 resolvedPos = transSelf->PreviousPosition() + contactNormal * separationBias;
	transSelf->SetPosition(resolvedPos);

	// Keep the collider world position in sync with the corrected transform.
	col->setPosition(transSelf->Position());

	// Clear accumulated forces to prevent stored impulses from immediately
	// driving the entity back into penetration on the next physics tick.
	// Re-apply a fraction of the previous accumulated force so some momentum is retained,
	// but collisions gradually bleed off stored force.
	if (physSelf)
	{
		physSelf->ClearForces();
		glm::vec3 reapplied = savedForceSelf * k_forceRetention;
		// remove small normal force so we don't re-introduce tiny upward impulses
		float fnormal = glm::dot(reapplied, n);
		if (std::abs(fnormal) < k_forceNormalStopThreshold)
			reapplied -= fnormal * n;
		physSelf->ApplyForce(reapplied);
	}

	// If the other entity is dynamic, reduce its stored forces similarly so two-body collisions
	// also lose momentum over time.
	if (physOther)
	{
		physOther->ClearForces();
		glm::vec3 reappliedOther = savedForceOther * k_forceRetention;
		float fnormalOther = glm::dot(reappliedOther, n);
		if (std::abs(fnormalOther) < k_forceNormalStopThreshold)
			reappliedOther -= fnormalOther * n;
		physOther->ApplyForce(reappliedOther);
	}
}

void ComponentCollision::InvokeCollision(Entity& ent1, Entity& ent2)
{
    // Call user callback first so game logic can run
    if (m_onCollision)
        m_onCollision(ent1, ent2);
}

