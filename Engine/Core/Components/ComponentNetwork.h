#pragma once
#include "IComponent.h"
#include "../NetworkTypes.h"
#include <cstdint>
#include <string>

class ComponentNetwork : public IComponent {
public:
    uint32_t networkId;
    ObjectType type; // Static, Animated, Simulated
    PeerID ownerId;  // Specific PeerID or ALL_PEERS (0xFFFFFFFF)

    // Runtime metadata from scene object
    std::string materialName;
    uint8_t shapeType = 0;
    uint8_t behaviourType = 0;
    uint8_t collisionType = 0;

    ComponentNetwork(
        uint32_t netId,
        ObjectType objType,
        PeerID owner,
        std::string material = {},
        uint8_t shape = 0,
        uint8_t behaviour = 0,
        uint8_t collision = 0)
        : IComponent(EComponentType::Component_Network),
        networkId(netId),
        type(objType),
        ownerId(owner),
        materialName(std::move(material)),
        shapeType(shape),
        behaviourType(behaviour),
        collisionType(collision)
    {
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