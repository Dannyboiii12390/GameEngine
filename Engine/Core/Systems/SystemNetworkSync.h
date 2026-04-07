#pragma once
#include "../NetworkTypes.h"
#include "../Components/ComponentNetwork.h"
#include "../Components/ComponentTransform.h"
#include <vector>
#include <span>
#include <memory>
#include "../Entity.h"
#include "ISystem.h" // Add this include

class SystemNetworkSync : public ISystem // Inherit from ISystem
{
public:
    SystemNetworkSync(PeerID localPeerId, std::shared_ptr<SharedNetworkData> networkData)
        : m_localPeerId(localPeerId), m_networkData(networkData)
    { 
        m_SystemType = ESystemType::System_Network_Sync; 
    }

    // Implement OnUpdate to match ISystem interface
    void OnUpdate(std::span<Entity> entities, float deltaTime) override
    {
        std::vector<Entity*> entityPtrs;
        for (auto& e : entities)
            entityPtrs.push_back(&e);
        Update(m_localPeerId, entityPtrs);
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
    PeerID m_localPeerId;
    std::shared_ptr<SharedNetworkData> m_networkData;

    void BroadcastToPeers(std::span<SyncPacket> packets)
    {
        if (!m_networkData) return;
        std::lock_guard<std::mutex> lock(m_networkData->outgoingMutex);
        m_networkData->outgoingPackets.insert(m_networkData->outgoingPackets.end(), packets.begin(), packets.end());
    }

    std::vector<SyncPacket> ReceiveFromPeers()
    {
        if (!m_networkData) return {};
        std::lock_guard<std::mutex> lock(m_networkData->incomingMutex);
        auto copy = m_networkData->incomingPackets;
        m_networkData->incomingPackets.clear();
        return copy;
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
                    glm::vec3 syncedPosition(packet.posX, packet.posY, packet.posZ);
                    
                    // 1. Write the new position into the WriteBuffer
                    transform->SetPosition(syncedPosition);
                    
                    // 2. Swap the buffers to commit it to the ReadBuffer instantly. 
                    // This ensures the render thread and next physics step start from this synced position.
                    transform->SwapBuffers();
                    
                    // 3. Write it again so the 'new' WriteBuffer is synchronized 
                    // and stays physically consistent for delta implementations.
                    transform->SetPosition(syncedPosition);
                }
                break;
            }
        }
    }
};