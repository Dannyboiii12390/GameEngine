#pragma once
#include "../NetworkTypes.h"
#include "../Components/ComponentNetwork.h"
#include "../Components/ComponentTransform.h"
#include <vector>
#include <span>
#include "../Entity.h"
#include "ISystem.h" // Add this include

class SystemNetworkSync : public ISystem // Inherit from ISystem
{
public:
    SystemNetworkSync() { m_SystemType = ESystemType::System_Network_Sync; }

    // Implement OnUpdate to match ISystem interface
    void OnUpdate(std::span<Entity> entities, float deltaTime) override
    {
        // Example: You may want to pass localPeerId and convert entities to pointers if needed
        // For now, just call Update with dummy PeerID and pointer vector
        PeerID localPeerId = 0;
        std::vector<Entity*> entityPtrs;
        for (auto& e : entities)
            entityPtrs.push_back(&e);
        Update(localPeerId, entityPtrs);
    }

    void Update(PeerID localPeerId, const std::vector<Entity*>& entities)
    {
        std::vector<SyncPacket> outgoingPackets;

        // 1. GATHER STATE: Broadcast state of simulated objects I own
        for (auto* entity : entities)
        {
            auto* netComp = entity->GetComponent<ComponentNetwork>(EComponentType::Component_Network);
            auto* transform = entity->GetComponent<ComponentTransform>(EComponentType::Component_Transform);

            if (netComp && transform && netComp->IsSimulated() && netComp->IsOwnedByMe(localPeerId))
            {
                SyncPacket packet{};
                packet.objectId = netComp->networkId;

                auto pos = transform->GetTransformMatrix() * glm::vec4(0, 0, 0, 1);
                packet.posX = pos.x;
                packet.posY = pos.y;
                packet.posZ = pos.z;

                outgoingPackets.push_back(packet);
            }
        }

        if (!outgoingPackets.empty())
        {
            BroadcastToPeers(outgoingPackets);
        }

        std::vector<SyncPacket> incomingPackets = ReceiveFromPeers();
        for (const auto& packet : incomingPackets)
        {
            ApplyStateToObject(packet, entities);
        }
    }

private:
    void BroadcastToPeers(std::span<SyncPacket> packets)
    {
        // Feed `packets.data()` and `packets.size_bytes()` to your existing UDP sockets
    }

    std::vector<SyncPacket> ReceiveFromPeers()
    {
        // Read from your UDP sockets and reconstruct SyncPackets
        return {};
    }

    void ApplyStateToObject(const SyncPacket& packet, const std::vector<Entity*>& entities)
    {
        for (auto* entity : entities)
        {
            auto* netComp = entity->GetComponent<ComponentNetwork>(EComponentType::Component_Network);
            if (netComp && netComp->networkId == packet.objectId)
            {
                auto* transform = entity->GetComponent<ComponentTransform>(EComponentType::Component_Transform);
                if (transform)
                {
                    transform->SetPosition({ packet.posX, packet.posY, packet.posZ });
                }
                break;
            }
        }
    }
};