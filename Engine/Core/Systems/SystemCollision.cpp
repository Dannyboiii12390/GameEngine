#include "SystemCollision.h"
#include "../Components/ComponentCollision.h"
#include "../Components/ComponentTransform.h"
#include "../Components/ComponentVelocity.h"
#include "../Entity.h"
#include <omp.h>
#include "../../DebugUtils.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

//#define USE_COMPUTE // Comment out to use CPU-based collision detection instead of GPU compute shader

namespace
{
	struct CellCoord
	{
		int x;
		int y;
		int z;

		bool operator==(const CellCoord& other) const
		{
			return x == other.x && y == other.y && z == other.z;
		}
	};

	struct CellCoordHasher
	{
		size_t operator()(const CellCoord& coord) const noexcept
		{
			const size_t hx = static_cast<size_t>(coord.x * 73856093);
			const size_t hy = static_cast<size_t>(coord.y * 19349663);
			const size_t hz = static_cast<size_t>(coord.z * 83492791);
			return hx ^ hy ^ hz;
		}
	};

	struct BroadPhaseBody
	{
		int entityIndex;
		ComponentCollision* collision;
		glm::vec3 min;
		glm::vec3 max;
	};

	uint64_t MakePairKey(int a, int b)
	{
		const uint32_t low = static_cast<uint32_t>(std::min(a, b));
		const uint32_t high = static_cast<uint32_t>(std::max(a, b));
		return (static_cast<uint64_t>(low) << 32u) | high;
	}

	bool BuildAabb(const Physics::Collider& collider, glm::vec3& outMin, glm::vec3& outMax, bool& outInfinite)
	{
		outInfinite = false;

		switch (collider.getType())
		{
		case Physics::EColliderType::SPHERE:
		{
			const auto& sphere = static_cast<const Physics::Sphere&>(collider);
			const glm::vec3 center = sphere.getPos();
			const float radius = sphere.getRadius();
			const glm::vec3 radiusVec(radius);
			outMin = center - radiusVec;
			outMax = center + radiusVec;
			return true;
		}
		case Physics::EColliderType::CAPSULE:
		{
			const auto& capsule = static_cast<const Physics::Capsule&>(collider);
			const glm::vec3 a = capsule.getA();
			const glm::vec3 b = capsule.getB();
			const float radius = capsule.getRadius();
			const glm::vec3 r(radius);
			outMin = glm::min(a, b) - r;
			outMax = glm::max(a, b) + r;
			return true;
		}
		case Physics::EColliderType::CYLINDER:
		{
			const auto& cylinder = static_cast<const Physics::Cylinder&>(collider);
			const glm::vec3 a = cylinder.getA();
			const glm::vec3 b = cylinder.getB();
			const float radius = cylinder.getRadius();
			const glm::vec3 r(radius);
			outMin = glm::min(a, b) - r;
			outMax = glm::max(a, b) + r;
			return true;
		}
		case Physics::EColliderType::LINEINF:
		case Physics::EColliderType::PLANE:
			outInfinite = true;
			return false;
		default:
			return false;
		}
	}
}

void SystemCollision::OnUpdate(std::span<Entity> entities, float deltaTime)
{
	EComponentType requiredComponents = EComponentType::Component_Collision | EComponentType::Component_Transform;

	const int count = static_cast<int>(entities.size());

	// Sync collider positions from the committed (read) transform buffer.
	#pragma omp parallel for
	for (int i = 0; i < count; ++i)
	{
		Entity& entity = entities[i];

		if (!entity.HasComponent(requiredComponents))
			continue;

		ComponentCollision* collisionComp = entity.GetComponent<ComponentCollision>(EComponentType::Component_Collision);
		ComponentTransform* thisTransform = entity.GetComponent<ComponentTransform>(EComponentType::Component_Transform);

		Physics::Collider* collider = collisionComp->GetCollider();
		if (!collider)
			continue;

		collider->setPosition(thisTransform->Position());
		collider->setRotation(thisTransform->Rotation());
	}



	std::vector<int> collidableIndices;
	collidableIndices.reserve(count);

	for (int i = 0; i < count; ++i)
	{
		if (!entities[i].HasComponent(requiredComponents))
			continue;

		ComponentCollision* collisionComp = entities[i].GetComponent<ComponentCollision>(EComponentType::Component_Collision);
		if (collisionComp && collisionComp->GetCollider())
		{
			collidableIndices.push_back(i);
		}
	}

	std::vector<BroadPhaseBody> finiteBodies;
	std::vector<BroadPhaseBody> infiniteBodies;
	finiteBodies.reserve(collidableIndices.size());

	float diameterAccum = 0.0f;
	int diameterSamples = 0;

	for (int idx : collidableIndices)
	{
		ComponentCollision* collision = entities[idx].GetComponent<ComponentCollision>(EComponentType::Component_Collision);
		Physics::Collider* collider = collision->GetCollider();

		if (!collider)
			continue;

		glm::vec3 aabbMin{};
		glm::vec3 aabbMax{};
		bool infinite = false;

		if (BuildAabb(*collider, aabbMin, aabbMax, infinite))
		{
			finiteBodies.push_back({ idx, collision, aabbMin, aabbMax });
			
			const glm::vec3 diff = aabbMax - aabbMin;
			const float maxDiff = std::max({ diff.x, diff.y, diff.z });
			diameterAccum += maxDiff;

			++diameterSamples;
		}
		else if (infinite)
		{
			infiniteBodies.push_back({ idx, collision, aabbMin, aabbMax });
		}
	}

	const float averageDiameter = diameterSamples > 0 ? diameterAccum / static_cast<float>(diameterSamples) : 1.0f;
	const float cellSize = std::max(0.5f, averageDiameter);

	std::unordered_map<CellCoord, std::vector<int>, CellCoordHasher> grid;
	grid.reserve(finiteBodies.size() * 2);

	for (int i = 0; i < static_cast<int>(finiteBodies.size()); ++i)
	{
		const BroadPhaseBody& body = finiteBodies[i];
		const glm::vec3 cellMinF = glm::floor(body.min / cellSize);
		const glm::vec3 cellMaxF = glm::floor(body.max / cellSize);

		const CellCoord cellMin{ static_cast<int>(cellMinF.x), static_cast<int>(cellMinF.y), static_cast<int>(cellMinF.z) };
		const CellCoord cellMax{ static_cast<int>(cellMaxF.x), static_cast<int>(cellMaxF.y), static_cast<int>(cellMaxF.z) };

		for (int z = cellMin.z; z <= cellMax.z; ++z)
		{
			for (int y = cellMin.y; y <= cellMax.y; ++y)
			{
				for (int x = cellMin.x; x <= cellMax.x; ++x)
				{
					grid[{ x, y, z }].push_back(i);
				}
			}
		}
	}

	std::vector<std::pair<int, int>> candidatePairs;
	candidatePairs.reserve(finiteBodies.size() * 2);

	std::unordered_set<uint64_t> seenPairs;
	seenPairs.reserve(finiteBodies.size() * 4);

	for (const auto& cell : grid)
	{
		const std::vector<int>& indices = cell.second;
		for (size_t i = 0; i < indices.size(); ++i)
		{
			for (size_t j = i + 1; j < indices.size(); ++j)
			{
				const int a = finiteBodies[indices[i]].entityIndex;
				const int b = finiteBodies[indices[j]].entityIndex;
				const uint64_t key = MakePairKey(a, b);

				if (seenPairs.insert(key).second)
				{
					candidatePairs.emplace_back(a, b);
				}
			}
		}
	}

	for (const BroadPhaseBody& infinite : infiniteBodies)
	{
		for (const BroadPhaseBody& body : finiteBodies)
		{
			const uint64_t key = MakePairKey(infinite.entityIndex, body.entityIndex);
			if (seenPairs.insert(key).second)
			{
				candidatePairs.emplace_back(infinite.entityIndex, body.entityIndex);
			}
		}
	}

	for (size_t i = 0; i < infiniteBodies.size(); ++i)
	{
		for (size_t j = i + 1; j < infiniteBodies.size(); ++j)
		{
			const int a = infiniteBodies[i].entityIndex;
			const int b = infiniteBodies[j].entityIndex;
			const uint64_t key = MakePairKey(a, b);
			if (seenPairs.insert(key).second)
			{
				candidatePairs.emplace_back(a, b);
			}
		}
	}

	#pragma omp parallel for
	for (int p = 0; p < static_cast<int>(candidatePairs.size()); ++p)
	{
		const auto [aIndex, bIndex] = candidatePairs[p];
		Entity& entityA = entities[aIndex];
		Entity& entityB = entities[bIndex];

		ComponentCollision* collisionA = entityA.GetComponent<ComponentCollision>(EComponentType::Component_Collision);
		ComponentCollision* collisionB = entityB.GetComponent<ComponentCollision>(EComponentType::Component_Collision);

		if (!collisionA || !collisionB)
			continue;

		if (collisionA->Collided(*collisionB->GetCollider()))
		{
			//#pragma omp critical
			{
				collisionA->InvokeCollision(entityA, entityB);
				collisionB->InvokeCollision(entityB, entityA);
			}
		}
	}
}


