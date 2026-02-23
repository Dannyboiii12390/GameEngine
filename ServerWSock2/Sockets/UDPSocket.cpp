#include "UDPSocket.h"
#include <iostream>
#include <cstring>

namespace Networking
{

    UDPSocket::UDPSocket()
        : m_connectedPeer(false)
    {
        // create UDP socket lazily in connect or when needed
        m_socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (m_socket == INVALID_SOCKET) {
            std::cerr << "UDPSocket: socket creation failed: " << WSAGetLastError() << "\n";
        }
    }

    UDPSocket::UDPSocket(SOCKET s)
        : ISocket(s), m_connectedPeer(false)
    {
    }

    UDPSocket::~UDPSocket()
    {
        disconnect();
    }

    void UDPSocket::connect(const char* host, const char* port)
    {
        addrinfo hints;
        ZeroMemory(&hints, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_protocol = IPPROTO_UDP;

        addrinfo* result = nullptr;
        int rv = getaddrinfo(host, port, &hints, &result);
        if (rv != 0) {
            std::cerr << "getaddrinfo failed: " << rv << "\n";
        }

        bool ok = false;
        for (addrinfo* ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
            if (::connect(m_socket, ptr->ai_addr, static_cast<int>(ptr->ai_addrlen)) == 0) {
                ok = true;
                break;
            }
        }

        freeaddrinfo(result);

        if (!ok) {
            std::cerr << "UDPSocket connect failed: " << WSAGetLastError() << "\n";
        }

        m_connectedPeer = true;
    }

    void UDPSocket::disconnect()
    {
        if (m_socket != INVALID_SOCKET) {
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
        }
        m_connectedPeer = false;
    }

    bool UDPSocket::isConnected() const
    {
        return m_socket != INVALID_SOCKET;
    }

    bool UDPSocket::isConnectedTo(const char* host, const char* port) const
    {
        if (!m_connectedPeer || m_socket == INVALID_SOCKET) return false;

        sockaddr_storage addr{};
        int addrLen = static_cast<int>(sizeof(addr));
        if (getpeername(m_socket, reinterpret_cast<sockaddr*>(&addr), &addrLen) == SOCKET_ERROR) {
            return false;
        }

        char remoteHost[NI_MAXHOST] = { 0 };
        char remoteServ[NI_MAXSERV] = { 0 };
        if (getnameinfo(reinterpret_cast<sockaddr*>(&addr), addrLen, remoteHost, sizeof(remoteHost), remoteServ, sizeof(remoteServ), NI_NUMERICHOST | NI_NUMERICSERV) != 0) {
            return false;
        }

        return (std::strcmp(remoteHost, host) == 0) && (std::strcmp(remoteServ, port) == 0);
    }

    bool UDPSocket::send(const void* data, int size)
    {
        if (m_socket == INVALID_SOCKET || data == nullptr || size <= 0 || !m_connectedPeer) return false;

        const char* buf = reinterpret_cast<const char*>(data);
        int sent = ::send(m_socket, buf, size, 0);
        if (sent == SOCKET_ERROR) {
            std::cerr << "UDPSocket send failed: " << WSAGetLastError() << "\n";
            return false;
        }

        return sent == size;
    }

    int UDPSocket::receive(void* buffer, int bufferSize)
    {
        if (m_socket == INVALID_SOCKET || buffer == nullptr || bufferSize <= 0) return -1;
        // For connected UDP, recv behaves like recvfrom but returns only data from connected peer.
        int n = ::recv(m_socket, reinterpret_cast<char*>(buffer), bufferSize, 0);
        if (n == SOCKET_ERROR) {
            std::cerr << "UDPSocket recv failed: " << WSAGetLastError() << "\n";
            return -1;
        }
        return n;
    }

    bool UDPSocket::sendPacket(const Packet& packet)
    {
        if (m_socket == INVALID_SOCKET || !m_connectedPeer) return false;

        std::vector<uint8_t> out;
        packet.serialize(out);
        if (out.empty()) return false;

        const char* buf = reinterpret_cast<const char*>(out.data());
        int total = static_cast<int>(out.size());
        int sent = ::send(m_socket, buf, total, 0);
        if (sent == SOCKET_ERROR) {
            std::cerr << "UDPSocket sendPacket failed: " << WSAGetLastError() << "\n";
            return false;
        }
        return sent == total;
    }

    bool UDPSocket::receivePacket(Packet& outPacket)
    {
        if (m_socket == INVALID_SOCKET) return false;

        // Maximum UDP datagram size practical to receive
        constexpr int MaxDatagram = 65536;
        std::vector<uint8_t> buf(MaxDatagram);
        int n = ::recv(m_socket, reinterpret_cast<char*>(buf.data()), static_cast<int>(buf.size()), 0);
        if (n == 0) {
            // connection closed/unlikely for UDP but treat as error
            return false;
        }
        if (n == SOCKET_ERROR) {
            std::cerr << "UDPSocket receivePacket recv failed: " << WSAGetLastError() << "\n";
            return false;
        }

        buf.resize(n);
        // Try to parse a single packet from this datagram
        if (!Packet::tryDeserializeFromBuffer(buf, outPacket)) {
            // Packet incomplete or malformed
            return false;
        }

        return true;
    }
}