#pragma once
#include "IComponent.h"
#include "../NetworkTypes.h"
#include <cstdint>

class ComponentNetwork : public IComponent {
public:
    uint32_t networkId;
    ObjectType type; // Static, Animated, Simulated
    PeerID ownerId;  // Specific PeerID or ALL_PEERS (0xFFFFFFFF)

    ComponentNetwork(uint32_t netId, ObjectType objType, PeerID owner) 
        : networkId(netId), type(objType), ownerId(owner) {
        // Ensure static/animated objects are owned by everyone
        if (type == ObjectType::Static || type == ObjectType::Animated) {
            ownerId = ALL_PEERS;
        }
    }

    bool IsOwnedByMe(PeerID localPeerId) const {
        return ownerId == ALL_PEERS || ownerId == localPeerId;
    }
    bool IsSimulated() const {
        return type == ObjectType::Simulated;
	}
};