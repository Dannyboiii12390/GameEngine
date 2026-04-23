#include "Spawner.h"
#include <glm/gtc/random.hpp>
#include <algorithm>
#include <cmath>

Spawner::Spawner(const SpawnerConfig& config)
    : m_config(config), m_rng(std::random_device{}()), m_uniformDist(0.0f, 1.0f)
{
}

void Spawner::Start()
{
    m_hasStarted = false;
    m_timeSinceStart = 0.0f;
    m_timeSinceLastSpawn = 0.0f;
    m_spawnedCount = 0;
}

void Spawner::Stop()
{
    m_isActive = false;
    m_hasStarted = false;
}

void Spawner::SetActive(bool active)
{
    m_isActive = active;
}

bool Spawner::Update(float deltaTime, float currentTime)
{
    if (!m_isActive)
        return false;

    if (!m_hasStarted)
    {
        if (currentTime >= m_config.startTime)
        {
            m_hasStarted = true;
            m_timeSinceStart = 0.0f;
            m_timeSinceLastSpawn = 0.0f;
        }
        else
        {
            return false;
        }
    }

    m_timeSinceStart += deltaTime;
    m_timeSinceLastSpawn += deltaTime;

    switch (m_config.spawnType)
    {
    case RuntimeSpawner::SpawnType::SingleBurst:
        return m_spawnedCount < m_config.burstCount && m_timeSinceStart <= deltaTime;

    case RuntimeSpawner::SpawnType::Repeating:
    {
        if (m_spawnedCount >= m_config.repeatMaxCount)
            return false;

        const float interval = std::max(0.0001f, m_config.repeatInterval);
        if (m_timeSinceLastSpawn >= interval)
        {
            m_timeSinceLastSpawn = 0.0f;
            return true;
        }
        return false;
    }

    default:
        return false;
    }
}

PeerID Spawner::ResolveOwnerForSpawn()
{
    if (m_config.ownerSequential)
        return GetNextSequentialOwner();
    return m_config.owner;
}

void Spawner::Spawn(float currentTime)
{
    (void)currentTime;

    if (!m_spawnCallback)
        return;

    uint32_t countToSpawn = 0;

    switch (m_config.spawnType)
    {
    case RuntimeSpawner::SpawnType::SingleBurst:
        if (m_spawnedCount < m_config.burstCount)
            countToSpawn = m_config.burstCount - m_spawnedCount;
        break;

    case RuntimeSpawner::SpawnType::Repeating:
        if (m_spawnedCount < m_config.repeatMaxCount)
            countToSpawn = 1;
        break;

    default:
        break;
    }

    for (uint32_t i = 0; i < countToSpawn; ++i)
    {
        const glm::vec3 position = GetSpawnLocation();
        const glm::vec3 linearVel = GetRandomLinearVelocity();
        const glm::vec3 angularVel = GetRandomAngularVelocity();
        const glm::vec3 randomSize = GetRandomSize();
        const float radius = GetRandomRadius();
        const float height = GetRandomHeight();
        const PeerID owner = ResolveOwnerForSpawn();

        m_spawnCallback(position, linearVel, angularVel, randomSize, radius, height, owner);
        ++m_spawnedCount;
    }
}

glm::vec3 Spawner::GetSpawnLocation()
{
    switch (m_config.locationType)
    {
    case RuntimeSpawner::SpawnLocation::Fixed:
        return m_config.fixedPosition;

    case RuntimeSpawner::SpawnLocation::Box:
    {
        const float x = m_config.boxMin.x + m_uniformDist(m_rng) * (m_config.boxMax.x - m_config.boxMin.x);
        const float y = m_config.boxMin.y + m_uniformDist(m_rng) * (m_config.boxMax.y - m_config.boxMin.y);
        const float z = m_config.boxMin.z + m_uniformDist(m_rng) * (m_config.boxMax.z - m_config.boxMin.z);
        return glm::vec3(x, y, z);
    }

    case RuntimeSpawner::SpawnLocation::Sphere:
    {
        const glm::vec3 randomDir = glm::ballRand(1.0f);
        const float randomRadius = m_uniformDist(m_rng) * std::max(0.0f, m_config.sphereRadius);
        return m_config.sphereCenter + randomDir * randomRadius;
    }

    default:
        return glm::vec3(0.0f);
    }
}

glm::vec3 Spawner::GetRandomLinearVelocity()
{
    const float x = m_config.linearVelMin.x + m_uniformDist(m_rng) * (m_config.linearVelMax.x - m_config.linearVelMin.x);
    const float y = m_config.linearVelMin.y + m_uniformDist(m_rng) * (m_config.linearVelMax.y - m_config.linearVelMin.y);
    const float z = m_config.linearVelMin.z + m_uniformDist(m_rng) * (m_config.linearVelMax.z - m_config.linearVelMin.z);
    return glm::vec3(x, y, z);
}

glm::vec3 Spawner::GetRandomAngularVelocity()
{
    const float x = m_config.angularVelMin.x + m_uniformDist(m_rng) * (m_config.angularVelMax.x - m_config.angularVelMin.x);
    const float y = m_config.angularVelMin.y + m_uniformDist(m_rng) * (m_config.angularVelMax.y - m_config.angularVelMin.y);
    const float z = m_config.angularVelMin.z + m_uniformDist(m_rng) * (m_config.angularVelMax.z - m_config.angularVelMin.z);
    return glm::vec3(x, y, z);
}

float Spawner::GetRandomRadius()
{
    return m_config.radiusMin + m_uniformDist(m_rng) * (m_config.radiusMax - m_config.radiusMin);
}

float Spawner::GetRandomHeight()
{
    return m_config.heightMin + m_uniformDist(m_rng) * (m_config.heightMax - m_config.heightMin);
}

glm::vec3 Spawner::GetRandomSize()
{
    const float x = m_config.sizeMin.x + m_uniformDist(m_rng) * (m_config.sizeMax.x - m_config.sizeMin.x);
    const float y = m_config.sizeMin.y + m_uniformDist(m_rng) * (m_config.sizeMax.y - m_config.sizeMin.y);
    const float z = m_config.sizeMin.z + m_uniformDist(m_rng) * (m_config.sizeMax.z - m_config.sizeMin.z);
    return glm::vec3(x, y, z);
}