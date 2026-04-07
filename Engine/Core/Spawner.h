#pragma once
#include "Entity.h"
#include "Components/ComponentNetwork.h"
#include "Components/ComponentTransform.h"
#include <vector>

class Spawner {
public:
    void SetConnectedPeers(const std::vector<PeerID>& activePeers) {
        m_activePeers = activePeers;
        m_nextPeerIndex = 0;
    }

    Entity SpawnSimulatedBox(glm::vec3 position, uint32_t networkId) {
        Entity e;
        e.AddComponent(EComponentType::Component_Transform, position, glm::vec3(0), glm::vec3(1));
        
        // Sequential Ownership Logic
        PeerID assignedOwner = 0;
        if (!m_activePeers.empty()) {
            assignedOwner = m_activePeers[m_nextPeerIndex];
            m_nextPeerIndex = (m_nextPeerIndex + 1) % m_activePeers.size();
        }

        // e.AddComponent(EComponentType::Component_Network, networkId, ObjectType::Simulated, assignedOwner);
        return e;
    }

private:
    std::vector<PeerID> m_activePeers;
    size_t m_nextPeerIndex = 0;
};