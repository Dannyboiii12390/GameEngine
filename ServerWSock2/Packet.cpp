#include "Packet.h"
#include <winsock2.h> // for htons/htonl/ntohs/ntohl
#include <cstring>
#include <algorithm>

namespace Networking {

void PacketHeader::toNetworkOrder()
{
    type = htons(type);
    flags = htons(flags);
    payloadSize = htonl(payloadSize);
}

void PacketHeader::toHostOrder()
{
    type = ntohs(type);
    flags = ntohs(flags);
    payloadSize = ntohl(payloadSize);
}

Packet::Packet(uint16_t type_)
{
    header.type = type_;
    header.flags = 0;
    header.payloadSize = 0;
}

void Packet::setPayload(const void* data, std::size_t size)
{
    payload.resize(size);
    if (size && data) {
        std::memcpy(payload.data(), data, size);
    }
    header.payloadSize = static_cast<uint32_t>(size);
}

std::size_t Packet::totalSize() const noexcept
{
    return PacketHeader::Size + payload.size();
}

void Packet::serialize(std::vector<uint8_t>& out) const
{
    out.clear();
    out.reserve(totalSize());

    PacketHeader netHeader = header;
    netHeader.toNetworkOrder();

    const uint8_t* hdrPtr = reinterpret_cast<const uint8_t*>(&netHeader);
    out.insert(out.end(), hdrPtr, hdrPtr + PacketHeader::Size);

    if (!payload.empty()) {
        out.insert(out.end(), payload.begin(), payload.end());
    }
}

bool Packet::tryDeserializeFromBuffer(std::vector<uint8_t>& buffer, Packet& outPacket)
{
    if (buffer.size() < PacketHeader::Size) {
        return false; // not enough bytes for header
    }

    // Read header bytes
    PacketHeader netHeader;
    std::memcpy(&netHeader, buffer.data(), PacketHeader::Size);
    netHeader.toHostOrder();

    std::size_t required = PacketHeader::Size + static_cast<std::size_t>(netHeader.payloadSize);
    if (buffer.size() < required) {
        return false; // not enough bytes for full packet yet
    }

    // Build packet
    outPacket.header = netHeader;
    outPacket.payload.resize(netHeader.payloadSize);
    if (netHeader.payloadSize > 0) {
        std::memcpy(outPacket.payload.data(), buffer.data() + PacketHeader::Size, netHeader.payloadSize);
    }

    // Remove consumed bytes from buffer (efficient enough for typical use)
    buffer.erase(buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(required));
    return true;
}

} // namespace Net