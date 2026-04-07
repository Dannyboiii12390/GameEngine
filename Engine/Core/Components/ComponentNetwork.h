#pragma once
#include "IComponent.h"
#include "../NetworkTypes.h"
#include <cstdint>

class ComponentNetwork : public IComponent {
public:
    uint32_t networkId; // A globally unique ID for state synchronization
    ObjectType type;    // Static, Animated, or Simulated
    PeerID ownerId;     // ALL_PEERS (0xFFFFFFFF) or a specific PeerID (0, 1, 2...)

    ComponentNetwork(uint32_t netId, ObjectType objType, PeerID owner)
        : networkId(netId), type(objType), ownerId(owner) {
    }

    bool IsOwnedByMe(PeerID localPeerId) const {
        return ownerId == ALL_PEERS || ownerId == localPeerId;
    }

    bool IsSimulated() const {
        return type == ObjectType::Simulated;
    }
};