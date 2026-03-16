#include "SystemCollision.h"
#include "../Components/ComponentCollision.h"
#include "../Components/ComponentTransform.h"
#include "../Components/ComponentVelocity.h"
#include "../Entity.h"
#include <omp.h>
#include "../../DebugUtils.h"

//#define USE_COMPUTE // Comment out to use CPU-based collision detection instead of GPU compute shader

#ifdef USE_COMPUTE

#include "../../Renderer/VulkanRHI.h"
#include "../../Renderer/ComputeShader.h"
#include <algorithm> // for std::min

void SystemCollision::OnUpdate(std::span<Entity> entities, float deltaTime)
{
	// Early-outs / safety
	if (m_NumEntities == 0 || m_EntitySize == 0)
		return;

	// Build list of entities that actually have the required components.
	const EComponentType required = EComponentType::Component_Collision | EComponentType::Component_Transform;
	std::vector<uint32_t> indices;
	indices.reserve(entities.size());
	for (uint32_t i = 0; i < entities.size(); ++i)
	{
		if (entities[i].HasComponent(required))
			indices.push_back(i);
	}

	const uint32_t collCount = static_cast<uint32_t>(indices.size());
	if (collCount == 0)
		return;

	// Prepare host-side input buffer sized to GPU allocation (m_NumEntities).
	// Populate only the first collCount entries; remaining entries remain zeroed.
	std::vector<ShaderEntityGPU> gpuIn;
	gpuIn.resize(m_NumEntities); // zero-initialised

	for (uint32_t dst = 0; dst < collCount; ++dst)
	{
		Entity& ent = entities[indices[dst]];

		ShaderEntityGPU entry{};
		auto* colComp = ent.GetComponent<ComponentCollision>(EComponentType::Component_Collision);
		auto* trans = ent.GetComponent<ComponentTransform>(EComponentType::Component_Transform);
		auto* vel = ent.HasComponent(EComponentType::Component_Velocity) ? ent.GetComponent<ComponentVelocity>(EComponentType::Component_Velocity) : nullptr;

		// position
		{
			const glm::vec3 p = trans->Position();
			entry.position = glm::vec4(p, 0.0f);
		}

		// velocity
		{
			glm::vec3 v(0.0f);
			if (vel) v = vel->GetPositionVelocity();
			entry.velocity = glm::vec4(v, 0.0f);
		}

		// collider metadata packing (must match shader expectations)
		if (colComp && colComp->GetCollider())
		{
			const Physics::Collider* c = colComp->GetCollider();
			entry.collider.x = static_cast<float>(c->getType());
			switch (c->getType())
			{
			case Physics::EColliderType::SPHERE:
			{
				auto* sph = dynamic_cast<const Physics::Sphere*>(c);
				if (sph) entry.collider.y = sph->getRadius();
				break;
			}
			case Physics::EColliderType::CYLINDER:
			{
				auto* cyl = dynamic_cast<const Physics::Cylinder*>(c);
				if (cyl)
				{
					entry.collider.y = cyl->getRadius();
					glm::vec3 A = cyl->getA();
					glm::vec3 B = cyl->getB();
					entry.collider.z = 0.5f * glm::length(B - A);
				}
				break;
			}
			case Physics::EColliderType::CAPSULE:
			{
				auto* cap = dynamic_cast<const Physics::Capsule*>(c);
				if (cap)
				{
					entry.collider.y = cap->getRadius();
					glm::vec3 A = cap->getA();
					glm::vec3 B = cap->getB();
					entry.collider.z = 0.5f * glm::length(B - A);
				}
				break;
			}
			default:
				break;
			}
		}

		gpuIn[dst] = entry;
	}

	// Upload input buffer (binding 0)
	const VkDeviceSize uploadSize = static_cast<VkDeviceSize>(m_NumEntities) * m_EntitySize;
	m_ComputeShader.Upload(0, gpuIn.data(), uploadSize);

	// Update push-constant to the active collidable count + dt so shader uses correct range and integrates.
	struct PC { uint32_t entityCount; float dt; };
	PC pc{ collCount, deltaTime };
	m_ComputeShader.PushConstants(&pc, static_cast<uint32_t>(sizeof(pc)));

	// Dispatch compute shader. Local size must match shader's layout(local_size_x=...).
	const uint32_t localSizeX = 256u;
	m_ComputeShader.Dispatch(collCount, localSizeX);

	// Read back output buffer (binding 1)
	std::vector<ShaderEntityGPU> gpuOut;
	gpuOut.resize(m_NumEntities);
	m_ComputeShader.Readback(1, gpuOut.data(), uploadSize);

	// Apply GPU results back to the exact entities we packed (use indices mapping).
	for (uint32_t src = 0; src < collCount; ++src)
	{
		Entity& ent = entities[indices[src]];
		auto* trans = ent.GetComponent<ComponentTransform>(EComponentType::Component_Transform);
		auto* col = ent.GetComponent<ComponentCollision>(EComponentType::Component_Collision);

		// apply position into write buffer
		glm::vec3 newPos = glm::vec3(gpuOut[src].position);
		trans->SetPosition(newPos);

		// update collider position
		if (col && col->GetCollider())
			col->GetCollider()->setPosition(newPos);

		// apply positional velocity if present
		if (ent.HasComponent(EComponentType::Component_Velocity))
		{
			auto* vel = ent.GetComponent<ComponentVelocity>(EComponentType::Component_Velocity);
			vel->SetPositionalVelocity(glm::vec3(gpuOut[src].velocity));
		}
	}
}
#else
void SystemCollision::OnUpdate(std::span<Entity> entities, float deltaTime)
{
	EComponentType requiredComponents = EComponentType::Component_Collision | EComponentType::Component_Transform;

	const int count = static_cast<int>(entities.size());

	// Sync collider positions from the committed (read) transform buffer.
	// Each entity is independent — safe to parallelise.
	#pragma omp parallel for
	for (int i = 0; i < count; ++i)
	{
		Entity& entity = entities[i];

		if (!entity.HasComponent(requiredComponents))
			continue;

		ComponentCollision* collisionComp = entity.GetComponent<ComponentCollision>(EComponentType::Component_Collision);
		ComponentTransform* thisTransform = entity.GetComponent<ComponentTransform>(EComponentType::Component_Transform);

		collisionComp->GetCollider()->setPosition(thisTransform->Position());
		collisionComp->GetCollider()->setRotation(thisTransform->Rotation());
	}

	// Detect and respond. CollisionResponse writes corrections into the write
	// buffer. SystemVelocity owns all buffer swapping at the start of the
	// next frame, so no swaps are needed here.
	#pragma omp parallel for 
	for (int i = 0; i < count; ++i)
	{
		Entity& entity = entities[i];

		if (!entity.HasComponent(requiredComponents))
			continue;

		ComponentCollision* collisionComp = entity.GetComponent<ComponentCollision>(EComponentType::Component_Collision);

		for (int j = 0; j < count; ++j)
		{
			if (i == j) continue;

			Entity& other = entities[j];

			if (!other.HasComponent(EComponentType::Component_Collision))
				continue;

			ComponentCollision* otherCollision = other.GetComponent<ComponentCollision>(EComponentType::Component_Collision);

			if (collisionComp->Collided(*otherCollision->GetCollider()))
			{
				#pragma omp critical
				collisionComp->InvokeCollision(entity, other);
			}
		}
	}
}
#endif


