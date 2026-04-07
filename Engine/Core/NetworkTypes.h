#pragma once
#include <cstdint>
#include <vector>
#include <mutex>

// An ID representing a unique peer in the network
using PeerID = uint32_t;
constexpr PeerID LOCAL_PEER_ID = 0; // Assigned dynamically upon network connection
constexpr PeerID ALL_PEERS = 0xFFFFFFFF; // For static/animated objects

enum class ObjectType {
    Static,
    Animated,
    Simulated
};

struct Ownership {
    ObjectType type;
    PeerID ownerId;

    bool IsOwnedByMe(PeerID myId) const {
        return ownerId == ALL_PEERS || ownerId == myId;
    }
};

// Data packet to sync dynamic state every frame
#pragma pack(push, 1)
struct SyncPacket {
    uint32_t objectId;
    float posX, posY, posZ;
    float rotX, rotY, rotZ, rotW;
    float scaleX, scaleY, scaleZ; // Added scale fields
    float velX, velY, velZ;
};
#pragma pack(pop)

struct SharedNetworkData {
    std::mutex incomingMutex;
    std::vector<SyncPacket> incomingPackets;

    std::mutex outgoingMutex;
    std::vector<SyncPacket> outgoingPackets;
};