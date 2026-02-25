#pragma once

#include <cstdint>
#include <vector>
#include <cstddef>
#include <cstring>
#include <winsock2.h> // for htons/htonl/ntohs/ntohl

#ifdef USE_FLATBUFFERS
#include <flatbuffers/flatbuffers.h>
#endif

namespace Networking {

    struct PacketHeader {
        uint16_t type = 0;
        uint16_t flags = 0;
        uint32_t payloadSize = 0;

        static constexpr std::size_t Size = sizeof(type) + sizeof(flags) + sizeof(payloadSize);

        void toNetworkOrder() {
            type = htons(type);
            flags = htons(flags);
            payloadSize = htonl(payloadSize);
        }

        void toHostOrder() {
            type = ntohs(type);
            flags = ntohs(flags);
            payloadSize = ntohl(payloadSize);
        }
    };

    class Packet {
    public:
        Packet(uint16_t type = 0)
        {
            header.type = type;
            header.flags = 0;
            header.payloadSize = 0;
        }

        PacketHeader header;
        std::vector<uint8_t> payload;

        void setPayload(const void* data, std::size_t size)
        {
            payload.resize(size);
            if (size && data) {
                std::memcpy(payload.data(), data, size);
            }
            header.payloadSize = static_cast<uint32_t>(size);
        }

#ifdef USE_FLATBUFFERS
        void setFromFlatBufferBuilder(const flatbuffers::FlatBufferBuilder& builder, uint16_t type)
        {
            const uint8_t* buf = builder.GetBufferPointer();
            std::size_t size = builder.GetSize();
            setPayload(buf, size);
            header.type = type;
        }
#endif

        std::size_t totalSize() const noexcept
        {
            return PacketHeader::Size + payload.size();
        }

        // Serialize header (network order) + payload into `out`.
        // Ensure the header payloadSize is synchronized with the actual payload length.
        void serialize(std::vector<uint8_t>& out) const
        {
            out.clear();
            out.reserve(totalSize());

            PacketHeader netHeader = header;
            // Defensive: always use actual payload size on the wire
            netHeader.payloadSize = static_cast<uint32_t>(payload.size());
            netHeader.toNetworkOrder();

            const uint8_t* hdrPtr = reinterpret_cast<const uint8_t*>(&netHeader);
            out.insert(out.end(), hdrPtr, hdrPtr + PacketHeader::Size);

            if (!payload.empty()) {
                out.insert(out.end(), payload.begin(), payload.end());
            }
        }

        static bool tryDeserializeFromBuffer(std::vector<uint8_t>& buffer, Packet& outPacket)
        {
            if (buffer.size() < PacketHeader::Size) {
                return false;
            }

            PacketHeader netHeader;
            std::memcpy(&netHeader, buffer.data(), PacketHeader::Size);
            netHeader.toHostOrder();

            std::size_t required = PacketHeader::Size + static_cast<std::size_t>(netHeader.payloadSize);
            if (buffer.size() < required) {
                return false;
            }

            outPacket.header = netHeader;
            outPacket.payload.resize(netHeader.payloadSize);
            if (netHeader.payloadSize > 0) {
                std::memcpy(outPacket.payload.data(), buffer.data() + PacketHeader::Size, netHeader.payloadSize);
            }

            buffer.erase(buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(required));
            return true;
        }

        const uint8_t* payloadData() const noexcept { return payload.empty() ? nullptr : payload.data(); }
        std::size_t payloadSize() const noexcept { return payload.size(); }
    };

} // namespace Networking