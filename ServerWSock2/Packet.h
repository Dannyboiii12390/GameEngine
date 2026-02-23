#pragma once
#include <cstdint>
#include <vector>
#include <cstddef>

namespace Networking {

// Simple packet header:
// - type: application message type (uint16_t)
// - flags: reserved bits for future use (uint16_t)
// - payloadSize: size in bytes of the payload (uint32_t)
//
// All header fields are stored on the wire in network byte order.
struct PacketHeader {
    uint16_t type;
    uint16_t flags;
    uint32_t payloadSize;

    static constexpr std::size_t Size = sizeof(type) + sizeof(flags) + sizeof(payloadSize);

    // Convert header fields to network byte order (before sending).
    void toNetworkOrder();

    // Convert header fields to host byte order (after receiving).
    void toHostOrder();
};

class Packet {
public:
    Packet(uint16_t type = 0);

    PacketHeader header;
    std::vector<uint8_t> payload;

    // Replace payload with provided data.
    void setPayload(const void* data, std::size_t size);

    // Total bytes when serialized (header + payload).
    std::size_t totalSize() const noexcept;

    // Serialize header (network order) + payload into `out`.
    void serialize(std::vector<uint8_t>& out) const;

    // Try to extract a complete Packet from a TCP receive buffer.
    // If a complete packet is present, fills `outPacket`, removes consumed bytes
    // from `buffer` and returns true. If not enough data, returns false and
    // leaves buffer unchanged.
    static bool tryDeserializeFromBuffer(std::vector<uint8_t>& buffer, Packet& outPacket);
};

} // namespace Networking