#pragma once
#include "../NetworkTypes.h"
#include "../Components/ComponentNetwork.h"
#include "../Components/ComponentTransform.h"
#include <vector>
#include <span>
#include <memory>
#include <atomic>
#include "../Entity.h"
#include "ISystem.h"

class SystemNetworkSync : public ISystem
{
public:
    SystemNetworkSync(std::atomic<PeerID>* localPeerId, std::shared_ptr<SharedNetworkData> networkData)
        : m_localPeerId(localPeerId), m_networkData(networkData)
    { 
        m_SystemType = ESystemType::System_Network_Sync; 
    }

    void OnUpdate(std::span<Entity> entities, float deltaTime) override
    {
        std::vector<Entity*> entityPtrs;
        for (auto& e : entities)
            entityPtrs.push_back(&e);

        const PeerID localPeerId = m_localPeerId ? m_localPeerId->load() : 0;
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

                // Sync Position
                glm::vec3 pos = transform->Position();
                packet.posX = pos.x;
                packet.posY = pos.y;
                packet.posZ = pos.z;

                // Sync Rotation
                glm::vec3 rot = transform->Rotation();
                packet.rotX = rot.x;
                packet.rotY = rot.y;
                packet.rotZ = rot.z;

                // Sync Scale
                glm::vec3 scale = transform->Scale();
                packet.scaleX = scale.x;
                packet.scaleY = scale.y;
                packet.scaleZ = scale.z;

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
    std::atomic<PeerID>* m_localPeerId = nullptr;
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
                    glm::vec3 syncedRotation(packet.rotX, packet.rotY, packet.rotZ);
                    glm::vec3 syncedScale(packet.scaleX, packet.scaleY, packet.scaleZ);
                    
                    // 1. Write the new transform values into the WriteBuffer
                    transform->SetPosition(syncedPosition);
                    transform->SetRotation(syncedRotation);
                    transform->SetScale(syncedScale);
                    
                    // 2. Swap the buffers to commit it to the ReadBuffer instantly. 
                    // This ensures the render thread and next physics step start from this synced transform.
                    transform->SwapBuffers();
                    
                    // 3. Write it again so the 'new' WriteBuffer is synchronized 
                    // and stays physically consistent for delta implementations.
                    transform->SetPosition(syncedPosition);
                    transform->SetRotation(syncedRotation);
                    transform->SetScale(syncedScale);
                }
                break;
            }
        }
    }
};