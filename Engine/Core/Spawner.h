#pragma once

#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <functional>
#include <random>
#include "NetworkTypes.h"

namespace RuntimeSpawner
{
    enum class SpawnType : uint8_t
    {
        None = 0,
        SingleBurst = 1,
        Repeating = 2
    };

    enum class SpawnLocation : uint8_t
    {
        None = 0,
        Fixed = 1,
        Box = 2,
        Sphere = 3
    };

    enum class SpawnerShapeType : uint8_t
    {
        Sphere = 1,
        Cylinder = 2,
        Capsule = 3,
        Cuboid = 4
    };
}

struct SpawnerConfig
{
    std::string name;
    float startTime = 0.0f;
    RuntimeSpawner::SpawnType spawnType = RuntimeSpawner::SpawnType::None;
    RuntimeSpawner::SpawnLocation locationType = RuntimeSpawner::SpawnLocation::None;
    RuntimeSpawner::SpawnerShapeType shapeType = RuntimeSpawner::SpawnerShapeType::Sphere;

    // Location parameters
    glm::vec3 fixedPosition = glm::vec3(0.0f);
    glm::vec3 boxMin = glm::vec3(-1.0f);
    glm::vec3 boxMax = glm::vec3(1.0f);
    glm::vec3 sphereCenter = glm::vec3(0.0f);
    float sphereRadius = 1.0f;

    // Velocity ranges
    glm::vec3 linearVelMin = glm::vec3(-1.0f);
    glm::vec3 linearVelMax = glm::vec3(1.0f);
    glm::vec3 angularVelMin = glm::vec3(-1.0f);
    glm::vec3 angularVelMax = glm::vec3(1.0f);

    // Shape-specific ranges
    float radiusMin = 0.5f;
    float radiusMax = 0.5f;
    float heightMin = 1.0f;
    float heightMax = 1.0f;
    glm::vec3 sizeMin = glm::vec3(1.0f);
    glm::vec3 sizeMax = glm::vec3(1.0f);

    // Spawn behavior
    std::string material = "default";
    PeerID owner = 0;
    bool ownerSequential = false;

    // Single burst
    uint32_t burstCount = 1;

    // Repeating
    float repeatInterval = 1.0f;
    uint32_t repeatMaxCount = 10;
};

class Spawner
{
public:
    explicit Spawner(const SpawnerConfig& config);
    ~Spawner() = default;

    void Start();
    void Stop();
    void SetActive(bool active);
    bool IsActive() const { return m_isActive; }

    // returns true when Spawn() should be called
    bool Update(float deltaTime, float currentTime);

    using SpawnCallback = std::function<void(
        const glm::vec3& position,
        const glm::vec3& linearVelocity,
        const glm::vec3& angularVelocity,
        const glm::vec3& randomSize,
        float radius,
        float height,
        PeerID owner)>;

    void SetSpawnCallback(SpawnCallback callback)
    {
        m_spawnCallback = std::move(callback);
    }

    void Spawn(float currentTime);

    void SetPeers(const std::vector<PeerID>& activePeers)
    {
        m_peers = activePeers;
        m_nextPeerIndex = 0;
    }

    PeerID GetNextSequentialOwner()
    {
        if (m_peers.empty())
            return 0;
        const PeerID assignedOwner = m_peers[m_nextPeerIndex];
        m_nextPeerIndex = (m_nextPeerIndex + 1) % m_peers.size();
        return assignedOwner;
    }

    const SpawnerConfig& GetConfig() const { return m_config; }
    const std::string& GetName() const { return m_config.name; }
    float GetStartTime() const { return m_config.startTime; }
    uint32_t GetSpawnedCount() const { return m_spawnedCount; }

private:
    glm::vec3 GetSpawnLocation();
    glm::vec3 GetRandomLinearVelocity();
    glm::vec3 GetRandomAngularVelocity();
    float GetRandomRadius();
    float GetRandomHeight();
    glm::vec3 GetRandomSize();

    PeerID ResolveOwnerForSpawn();

    SpawnerConfig m_config;
    bool m_isActive = false;
    uint32_t m_spawnedCount = 0;
    float m_timeSinceStart = 0.0f;
    float m_timeSinceLastSpawn = 0.0f;
    bool m_hasStarted = false;

    std::mt19937 m_rng;
    std::uniform_real_distribution<float> m_uniformDist;

    std::vector<PeerID> m_peers;
    size_t m_nextPeerIndex = 0;

    SpawnCallback m_spawnCallback;
};