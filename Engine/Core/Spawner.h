#pragma once

#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <memory>
#include <functional>
#include <random>
#include "NetworkTypes.h"

namespace Simulation {
    enum class SpawnType : uint8_t {
        NONE = 0,
        SingleBurst = 1,
        Repeating = 2
    };

    enum class SpawnLocation : uint8_t {
        NONE = 0,
        Fixed = 1,
        Box = 2,
        Sphere = 3
    };

    enum class SpawnerShapeType : uint8_t {
        Sphere = 1,
        Cylinder = 2,
        Capsule = 3,
        Cuboid = 4
    };
}

class Entity;

struct SpawnerConfig {
    std::string name;
    float startTime = 0.0f;
    Simulation::SpawnType spawnType = Simulation::SpawnType::NONE;
    Simulation::SpawnLocation locationType = Simulation::SpawnLocation::NONE;
    Simulation::SpawnerShapeType shapeType = Simulation::SpawnerShapeType::Sphere;

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

    // Shape-specific parameters
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

    // Single burst specific
    uint32_t burstCount = 1;

    // Repeating specific
    float repeatInterval = 1.0f;
    uint32_t repeatMaxCount = 10;
};

class Spawner {
public:
    explicit Spawner(const SpawnerConfig& config);
    ~Spawner() = default;

    // Lifecycle
    void Start();
    void Stop();
    void SetActive(bool active);
    bool IsActive() const { return m_isActive; }

    // Update: returns true if spawn should occur this frame
    bool Update(float deltaTime, float currentTime);

    // Spawning
    void SetSpawnCallback(std::function<Entity(const glm::vec3&, const glm::vec3&, const glm::vec3&, const glm::vec3&, float)> callback) {
        m_spawnCallback = callback;
    }

    void Spawn(float currentTime);

    // Sequential ownership tracking
    void SetPeers(const std::vector<PeerID>& activePeers) {
        m_peers = activePeers;
        m_nextPeerIndex = 0;
    }

    PeerID GetNextSequentialOwner() {
        if (m_peers.empty()) return 0;
        PeerID assignedOwner = m_peers[m_nextPeerIndex];
        m_nextPeerIndex = (m_nextPeerIndex + 1) % m_peers.size();
        return assignedOwner;
    }

    // Accessors
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

    std::function<Entity(const glm::vec3&, const glm::vec3&, const glm::vec3&, const glm::vec3&, float)> m_spawnCallback;
};