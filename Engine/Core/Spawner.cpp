#include "Spawner.h"
#include <glm/gtc/random.hpp>

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

    // Check if start time has been reached
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

    bool shouldSpawn = false;

    switch (m_config.spawnType)
    {
    case Simulation::SpawnType::SingleBurst:
    {
        // Single burst: spawn all objects at once when start time is reached
        if (m_spawnedCount < m_config.burstCount && m_timeSinceStart <= deltaTime)
        {
            shouldSpawn = true;
        }
        break;
    }
    case Simulation::SpawnType::Repeating:
    {
        // Repeating: spawn at regular intervals
        if (m_spawnedCount < m_config.repeatMaxCount && m_timeSinceLastSpawn >= m_config.repeatInterval)
        {
            shouldSpawn = true;
            m_timeSinceLastSpawn = 0.0f;
        }
        break;
    }
    default:
        break;
    }

    return shouldSpawn;
}

void Spawner::Spawn(float currentTime)
{
    if (!m_spawnCallback)
        return;

    uint32_t countToSpawn = 0;

    switch (m_config.spawnType)
    {
    case Simulation::SpawnType::SingleBurst:
    {
        if (m_spawnedCount < m_config.burstCount)
        {
            countToSpawn = m_config.burstCount - m_spawnedCount;
        }
        break;
    }
    case Simulation::SpawnType::Repeating:
    {
        if (m_spawnedCount < m_config.repeatMaxCount)
        {
            countToSpawn = 1; // Spawn one object per interval
        }
        break;
    }
    default:
        break;
    }

    for (uint32_t i = 0; i < countToSpawn; ++i)
    {
        glm::vec3 position = GetSpawnLocation();
        glm::vec3 linearVel = GetRandomLinearVelocity();
        glm::vec3 angularVel = GetRandomAngularVelocity();
        float shapeParam = 0.0f;

        // Select shape parameter based on shape type
        switch (m_config.shapeType)
        {
        case Simulation::SpawnerShapeType::Sphere:
            shapeParam = GetRandomRadius();
            break;
        case Simulation::SpawnerShapeType::Cylinder:
        case Simulation::SpawnerShapeType::Capsule:
            shapeParam = GetRandomHeight();
            break;
        case Simulation::SpawnerShapeType::Cuboid:
            shapeParam = 0.0f; // Size handled separately
            break;
        }

        // Invoke callback to create entity
        m_spawnCallback(position, linearVel, angularVel, GetRandomSize(), shapeParam);
        m_spawnedCount++;
    }
}

glm::vec3 Spawner::GetSpawnLocation()
{
    switch (m_config.locationType)
    {
    case Simulation::SpawnLocation::Fixed:
    {
        return m_config.fixedPosition;
    }
    case Simulation::SpawnLocation::Box:
    {
        float x = m_config.boxMin.x + m_uniformDist(m_rng) * (m_config.boxMax.x - m_config.boxMin.x);
        float y = m_config.boxMin.y + m_uniformDist(m_rng) * (m_config.boxMax.y - m_config.boxMin.y);
        float z = m_config.boxMin.z + m_uniformDist(m_rng) * (m_config.boxMax.z - m_config.boxMin.z);
        return glm::vec3(x, y, z);
    }
    case Simulation::SpawnLocation::Sphere:
    {
        glm::vec3 randomDir = glm::ballRand(1.0f);
        float randomRadius = m_uniformDist(m_rng) * m_config.sphereRadius;
        return m_config.sphereCenter + randomDir * randomRadius;
    }
    default:
        return glm::vec3(0.0f);
    }
}

glm::vec3 Spawner::GetRandomLinearVelocity()
{
    float x = m_config.linearVelMin.x + m_uniformDist(m_rng) * (m_config.linearVelMax.x - m_config.linearVelMin.x);
    float y = m_config.linearVelMin.y + m_uniformDist(m_rng) * (m_config.linearVelMax.y - m_config.linearVelMin.y);
    float z = m_config.linearVelMin.z + m_uniformDist(m_rng) * (m_config.linearVelMax.z - m_config.linearVelMin.z);
    return glm::vec3(x, y, z);
}

glm::vec3 Spawner::GetRandomAngularVelocity()
{
    float x = m_config.angularVelMin.x + m_uniformDist(m_rng) * (m_config.angularVelMax.x - m_config.angularVelMin.x);
    float y = m_config.angularVelMin.y + m_uniformDist(m_rng) * (m_config.angularVelMax.y - m_config.angularVelMin.y);
    float z = m_config.angularVelMin.z + m_uniformDist(m_rng) * (m_config.angularVelMax.z - m_config.angularVelMin.z);
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
    float x = m_config.sizeMin.x + m_uniformDist(m_rng) * (m_config.sizeMax.x - m_config.sizeMin.x);
    float y = m_config.sizeMin.y + m_uniformDist(m_rng) * (m_config.sizeMax.y - m_config.sizeMin.y);
    float z = m_config.sizeMin.z + m_uniformDist(m_rng) * (m_config.sizeMax.z - m_config.sizeMin.z);
    return glm::vec3(x, y, z);
}