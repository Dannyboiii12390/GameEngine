#include "ComponentCollision.h"
#include "../Entity.h"
#include "../../../PhysicsEngine/Maths/CollisionResolution/ConservationOfMomentum.h"
#include "../../../PhysicsEngine/Maths/CollisionResolution/CollisionResolution.h"
#include <unordered_map>
#include <mutex>
#include <algorithm>

namespace
{
	std::mutex g_materialInteractionMutex;
	std::unordered_map<std::string, MaterialInteractionCoefficients> g_materialInteractions;

	std::string MakeMaterialPairKey(const std::string& a, const std::string& b)
	{
		if (a <= b)
			return a + "|" + b;
		return b + "|" + a;
	}
}

void ClearMaterialInteractions()
{
	std::lock_guard<std::mutex> lock(g_materialInteractionMutex);
	g_materialInteractions.clear();
}

void RegisterMaterialInteraction(
	const std::string& materialA,
	const std::string& materialB,
	const MaterialInteractionCoefficients& coeffs)
{
	if (materialA.empty() || materialB.empty())
		return;

	std::lock_guard<std::mutex> lock(g_materialInteractionMutex);
	g_materialInteractions[MakeMaterialPairKey(materialA, materialB)] = coeffs;
}

MaterialInteractionCoefficients GetMaterialInteraction(
	const std::string& materialA,
	const std::string& materialB)
{
	if (materialA.empty() || materialB.empty())
		return {};

	std::lock_guard<std::mutex> lock(g_materialInteractionMutex);
	const std::string key = MakeMaterialPairKey(materialA, materialB);
	auto it = g_materialInteractions.find(key);
	if (it != g_materialInteractions.end())
		return it->second;

	return {};
}

void CollisionResponse(Entity& self, Entity& other)
{
	auto* selfColComp = self.GetComponent<ComponentCollision>(EComponentType::Component_Collision);
	auto* col = selfColComp->GetCollider();

	const auto selfType = col->getType();
	if (selfType != Physics::EColliderType::SPHERE &&
		selfType != Physics::EColliderType::CAPSULE &&
		selfType != Physics::EColliderType::CYLINDER)
	{
		return;
	}

	Physics::Collider* otherCollider = nullptr;
	if (other.HasComponent(EComponentType::Component_Collision))
	{
		ComponentCollision* otherColComp = other.GetComponent<ComponentCollision>(EComponentType::Component_Collision);
		if (otherColComp)
			otherCollider = otherColComp->GetCollider();
	}
	if (!otherCollider)
		return;

	ComponentTransform* transSelf = self.GetComponent<ComponentTransform>(EComponentType::Component_Transform);
	ComponentVelocity* velSelf = self.GetComponent<ComponentVelocity>(EComponentType::Component_Velocity);
	ComponentPhysics* physSelf = self.GetComponent<ComponentPhysics>(EComponentType::Component_Physics);

	if (!transSelf || !velSelf)
		return;

	// Material interaction lookup
	std::string selfMaterial;
	std::string otherMaterial;
	if (auto* selfNet = self.GetComponent<ComponentNetwork>(EComponentType::Component_Network))
		selfMaterial = selfNet->materialName;
	if (auto* otherNet = other.GetComponent<ComponentNetwork>(EComponentType::Component_Network))
		otherMaterial = otherNet->materialName;

	const MaterialInteractionCoefficients mi = GetMaterialInteraction(selfMaterial, otherMaterial);
	const float restitution = std::clamp(mi.restitution, 0.0f, 1.0f);
	const float staticFriction = std::clamp(mi.staticFriction, 0.0f, 1.0f);
	const float dynamicFriction = std::clamp(mi.dynamicFriction, 0.0f, 1.0f);

	glm::vec3 posSelf = transSelf->Position();
	glm::vec3 vSelf = velSelf->GetPositionVelocity();
	float massSelf = (physSelf) ? physSelf->GetMass() : 1.0f;

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
	else if (otherCollider->getType() == Physics::EColliderType::CAPSULE)
	{
		auto* cap = dynamic_cast<Physics::Capsule*>(otherCollider);
		if (!cap) return;

		glm::vec3 A = cap->getA();
		glm::vec3 B = cap->getB();
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

	const float approachSpeed = glm::dot(vSelf, contactNormal);
	if (approachSpeed >= 0.0f)
		return;

	auto applyFriction = [&](glm::vec3& velocity, const glm::vec3& n)
	{
		const float normalSpeed = glm::dot(velocity, n);
		glm::vec3 tangential = velocity - (normalSpeed * n);
		const float tangentialSpeed = glm::length(tangential);

		if (tangentialSpeed <= (staticFriction * 0.1f))
		{
			velocity -= tangential;
			return;
		}

		velocity -= tangential * dynamicFriction;
	};

	constexpr float k_velocityRetention = 0.9f;
	constexpr float k_forceRetention = 0.8f;
	constexpr float k_velocityStopThreshold = 0.5f;
	constexpr float k_normalStopThreshold = 0.5f;
	constexpr float k_forceNormalStopThreshold = 0.7f;

	glm::vec3 savedForceSelf = (physSelf) ? physSelf->getTotalForce() : glm::vec3(0.0f);

	ComponentPhysics* physOther = other.GetComponent<ComponentPhysics>(EComponentType::Component_Physics);
	ComponentVelocity* velOther = other.GetComponent<ComponentVelocity>(EComponentType::Component_Velocity);

	glm::vec3 savedForceOther = (physOther) ? physOther->getTotalForce() : glm::vec3(0.0f);

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

		glm::vec3 v1nRest = glm::dot(v1_after, n) * n * restitution;
		glm::vec3 v2nRest = glm::dot(v2_after, n) * n * restitution;

		glm::vec3 newV1 = (v1nRest + u1t) * k_velocityRetention;
		glm::vec3 newV2 = (v2nRest + u2t) * k_velocityRetention;

		applyFriction(newV1, n);
		applyFriction(newV2, n);

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

		ComponentTransform* transOther = other.GetComponent<ComponentTransform>(EComponentType::Component_Transform);
		if (transOther)
		{
			const float separationBias = 0.01f;
			glm::vec3 resolvedPosOther = transOther->PreviousPosition() - contactNormal * separationBias;
			transOther->SetPosition(resolvedPosOther);
			otherCollider->setPosition(resolvedPosOther);
		}
	}
	else
	{
		glm::vec3 newV = Physics::ResolveVelocityAgainstFixedObject(vSelf, restitution, n);
		newV *= k_velocityRetention;

		applyFriction(newV, n);

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

	const float separationBias = 0.01f;
	glm::vec3 resolvedPos = transSelf->PreviousPosition() + contactNormal * separationBias;
	transSelf->SetPosition(resolvedPos);
	col->setPosition(resolvedPos);

	if (physSelf)
	{
		physSelf->ClearForces();
		glm::vec3 reapplied = savedForceSelf * k_forceRetention;
		float fnormal = glm::dot(reapplied, n);
		if (std::abs(fnormal) < k_forceNormalStopThreshold)
			reapplied -= fnormal * n;
		physSelf->ApplyForce(reapplied);
	}

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
	if (m_onCollision)
		m_onCollision(ent1, ent2);
}

