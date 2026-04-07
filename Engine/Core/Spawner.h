#pragma once
#include "Entity.h"
#include "Components/ComponentNetwork.h"
#include <vector>
#include <cstdint>

class Spawner {
public:
    // Update this whenever peers connect or disconnect
    void UpdateActivePeers(const std::vector<PeerID>& activePeers) {
        connectedPeers = activePeers;
        // Make sure the local peer is in the list too if they can own spawned objects!
    }

    // Called when spawning a simulated object over the network
    PeerID GetNextSequentialOwner() {
        if (connectedPeers.empty()) {
            return 0; // Fallback to local peer ID if no network
        }

        PeerID assignedOwner = connectedPeers[nextPeerIndex];
        nextPeerIndex = (nextPeerIndex + 1) % connectedPeers.size();
        return assignedOwner;
    }

    void SpawnSimulatedObject(uint32_t networkId /* other params */) {
        PeerID owner = GetNextSequentialOwner();

        // Entity creation logic...
        // Entity* newEntity = ...
        // newEntity->AddComponent(new NetworkComponent(networkId, ObjectType::Simulated, owner));
    }

private:
    std::vector<PeerID> connectedPeers;
    size_t nextPeerIndex = 0;
};