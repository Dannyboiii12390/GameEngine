#pragma once
#include "ISocket.h"
#include <winsock2.h>
#include <ws2tcpip.h>

namespace Networking
{
    class TCPSocket : public ISocket
    {
    public:
        TCPSocket() = default;
        explicit TCPSocket(SOCKET s);
        ~TCPSocket() override;

        void connect(const char* host, const char* port) override;
        void disconnect() override;

        bool isConnected() const override;
        bool isConnectedTo(const char* host, const char* port) const override;

        bool send(const void* data, int size) override;
        int receive(void* buffer, int bufferSize) override;

		bool sendPacket(const Packet& packet) override;
		bool receivePacket(Packet& packet) override;

    private:
		std::vector<uint8_t> m_recvBuffer; // Buffer for accumulating received data until complete packets can be extracted
    };
}