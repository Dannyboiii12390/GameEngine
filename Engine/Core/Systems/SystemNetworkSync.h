#pragma once
#include "../NetworkTypes.h"
#include "../Components/ComponentNetwork.h"
#include "../Components/ComponentTransform.h"
#include "../Components/ComponentVelocity.h"
#include <vector>
#include <span>
#include <memory>
#include <atomic>
#include <functional>
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

    void SetSpawnHandler(std::function<void(const SpawnPacket&)> handler)
    {
        m_spawnHandler = std::move(handler);
    }

    void OnUpdate(std::span<Entity> entities, float deltaTime) override
    {
        std::vector<Entity*> entityPtrs;
        entityPtrs.reserve(entities.size());
        for (auto& e : entities)
            entityPtrs.push_back(&e);

        const PeerID localPeerId = m_localPeerId ? m_localPeerId->load() : 0;
        Update(localPeerId, entityPtrs);
    }

    void Update(PeerID localPeerId, const std::vector<Entity*>& entities)
    {
        std::vector<NetworkMessage> outgoingMessages;

        for (auto* entity : entities)
        {
            auto* netComp = entity->GetComponent<ComponentNetwork>(EComponentType::Component_Network);
            auto* transform = entity->GetComponent<ComponentTransform>(EComponentType::Component_Transform);
            auto* velocity = entity->GetComponent<ComponentVelocity>(EComponentType::Component_Velocity);

            if (netComp && transform && netComp->IsSimulated() && netComp->IsOwnedByMe(localPeerId))
            {
                SyncPacket packet{};
                packet.sourcePeerId = localPeerId; // NEW
                packet.objectId = netComp->networkId;

                glm::vec3 pos = transform->Position();
                packet.posX = pos.x; packet.posY = pos.y; packet.posZ = pos.z;

                glm::vec3 rot = transform->Rotation();
                packet.rotX = rot.x; packet.rotY = rot.y; packet.rotZ = rot.z; packet.rotW = 0.0f;

                glm::vec3 scale = transform->Scale();
                packet.scaleX = scale.x; packet.scaleY = scale.y; packet.scaleZ = scale.z;

                if (velocity)
                {
                    const glm::vec3 lv = velocity->GetPositionVelocity();
                    const glm::vec3 av = velocity->GetRotationalVelocity();
                    packet.velX = lv.x; packet.velY = lv.y; packet.velZ = lv.z;
                    packet.angVelX = av.x; packet.angVelY = av.y; packet.angVelZ = av.z;
                }

                NetworkMessage msg{};
                msg.type = NetworkMessageType::SyncState;
                msg.sync = packet;
                outgoingMessages.push_back(msg);
            }
        }

        if (!outgoingMessages.empty())
            BroadcastToPeers(outgoingMessages);

        std::vector<NetworkMessage> incomingMessages = ReceiveFromPeers();
        for (const auto& msg : incomingMessages)
        {
            if (msg.type == NetworkMessageType::SyncState)
            {
                ApplyStateToObject(msg.sync, entities, localPeerId);
            }
            else if (msg.type == NetworkMessageType::SpawnEntity && m_spawnHandler)
            {
                m_spawnHandler(msg.spawn);
            }
        }
    }

private:
    std::atomic<PeerID>* m_localPeerId = nullptr;
    std::shared_ptr<SharedNetworkData> m_networkData;
    std::function<void(const SpawnPacket&)> m_spawnHandler;

    void BroadcastToPeers(std::span<NetworkMessage> messages)
    {
        if (!m_networkData) return;
        std::lock_guard<std::mutex> lock(m_networkData->outgoingMutex);
        m_networkData->outgoingPackets.insert(m_networkData->outgoingPackets.end(), messages.begin(), messages.end());
    }

    std::vector<NetworkMessage> ReceiveFromPeers()
    {
        if (!m_networkData) return {};
        std::lock_guard<std::mutex> lock(m_networkData->incomingMutex);
        auto copy = m_networkData->incomingPackets;
        m_networkData->incomingPackets.clear();
        return copy;
    }

    void ApplyStateToObject(const SyncPacket& packet, const std::vector<Entity*>& entities, PeerID localPeerId)
    {
        // NEW: ignore packets originated by this peer (extra safety)
        if (packet.sourcePeerId == localPeerId)
            return;

        for (auto* entity : entities)
        {
            auto* netComp = entity->GetComponent<ComponentNetwork>(EComponentType::Component_Network);
            if (!netComp || netComp->networkId != packet.objectId)
                continue;

            if (netComp->IsSimulated() && netComp->IsOwnedByMe(localPeerId))
                return; // owner keeps authoritative local simulation

            auto* transform = entity->GetComponent<ComponentTransform>(EComponentType::Component_Transform);
            if (transform)
            {
                glm::vec3 syncedPosition(packet.posX, packet.posY, packet.posZ);
                glm::vec3 syncedRotation(packet.rotX, packet.rotY, packet.rotZ);
                glm::vec3 syncedScale(packet.scaleX, packet.scaleY, packet.scaleZ);

                transform->SetPosition(syncedPosition);
                transform->SetRotation(syncedRotation);
                transform->SetScale(syncedScale);
                transform->SwapBuffers();
                transform->SetPosition(syncedPosition);
                transform->SetRotation(syncedRotation);
                transform->SetScale(syncedScale);
            }

            auto* velocity = entity->GetComponent<ComponentVelocity>(EComponentType::Component_Velocity);
            if (velocity)
            {
                const glm::vec3 lv(packet.velX, packet.velY, packet.velZ);
                const glm::vec3 av(packet.angVelX, packet.angVelY, packet.angVelZ);
                velocity->SetPositionalVelocity(lv);
                velocity->SetRotationalVelocity(av);
                velocity->SwapBuffers();
                velocity->SetPositionalVelocity(lv);
                velocity->SetRotationalVelocity(av);
            }
            break;
        }
    }
};