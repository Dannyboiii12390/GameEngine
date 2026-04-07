#pragma once
#include "NetworkTypes.h"
#include <vector>

class Spawner {
public:
    void SetPeers(const std::vector<PeerID>& activePeers) {
        peers = activePeers;
        nextPeerIndex = 0;
    }

    PeerID GetNextSequentialOwner() {
        if (peers.empty()) return LOCAL_PEER_ID;
        
        PeerID assignedOwner = peers[nextPeerIndex];
        nextPeerIndex = (nextPeerIndex + 1) % peers.size();
        return assignedOwner;
    }

private:
    std::vector<PeerID> peers;
    size_t nextPeerIndex = 0;
};