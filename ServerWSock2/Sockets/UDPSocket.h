#pragma once
#include "ISocket.h"
#include <winsock2.h>
#include <ws2tcpip.h>

namespace Networking
{
    class UDPSocket : public ISocket
    {
    public:
        UDPSocket();
        explicit UDPSocket(SOCKET s);
        ~UDPSocket() override;

        // For UDP, connect sets a default peer (optional). If host==nullptr or port==nullptr, socket remains unconnected.
        void connect(const char* host, const char* port) override;
        void disconnect() override;

        bool isConnected() const override;
        bool isConnectedTo(const char* host, const char* port) const override;

        // send will use connected peer if set, otherwise will fail.
        bool send(const void* data, int size) override;
        // receive receives a single datagram (or up to bufferSize bytes).
        int receive(void* buffer, int bufferSize) override;

		bool sendPacket(const Packet& packet) override;
		bool receivePacket(Packet& packet) override;

    private:
        bool m_connectedPeer;
    };
}