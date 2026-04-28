#pragma once
#include <cstdint>
#include <vector>
#include <mutex>
#include <array>

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

enum class NetworkMessageType : uint8_t
{
    SyncState = 1,
    SpawnEntity = 2
};

#pragma pack(push, 1)
struct SyncPacket {
    PeerID sourcePeerId; // NEW: sender ID for host relay + self-filtering
    uint32_t objectId;
    float posX, posY, posZ;
    float rotX, rotY, rotZ, rotW;
    float scaleX, scaleY, scaleZ;
    float velX, velY, velZ;
    float angVelX, angVelY, angVelZ;
};

struct SpawnPacket {
    PeerID sourcePeerId;
    uint32_t objectId;
    uint8_t objectType;
    PeerID ownerId;
    uint8_t shapeType;
    uint8_t behaviourType;
    uint8_t collisionType;
    float posX, posY, posZ;
    float scaleX, scaleY, scaleZ;
    float velX, velY, velZ;
    float angVelX, angVelY, angVelZ;
    float radius;
    float height;
    std::array<char, 64> materialName{};
};

struct NetworkMessage {
    NetworkMessageType type = NetworkMessageType::SyncState;
    uint8_t reserved[3]{};
    union {
        SyncPacket sync;
        SpawnPacket spawn;
    };
};
#pragma pack(pop)

struct SharedNetworkData {
    std::mutex incomingMutex;
    std::vector<NetworkMessage> incomingPackets;

    std::mutex outgoingMutex;
    std::vector<NetworkMessage> outgoingPackets;
};