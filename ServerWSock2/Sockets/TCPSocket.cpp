#include "TCPSocket.h"
#include <iostream>
#include <cstring>

namespace Networking
{
    TCPSocket::TCPSocket(SOCKET s)
        : ISocket(s)
    {
    }

    TCPSocket::~TCPSocket()
    {
        disconnect();
    }

    void TCPSocket::connect(const char* host, const char* port)
    {
        disconnect();

        addrinfo hints;
        ZeroMemory(&hints, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        addrinfo* result = nullptr;
        int rv = getaddrinfo(host, port, &hints, &result);
        if (rv != 0) {
            std::cerr << "getaddrinfo failed: " << rv << "\n";
        }

        SOCKET s = INVALID_SOCKET;
        for (addrinfo* ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
            s = ::socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
            if (s == INVALID_SOCKET) {
                continue;
            }

            if (::connect(s, ptr->ai_addr, static_cast<int>(ptr->ai_addrlen)) == SOCKET_ERROR) {
                closesocket(s);
                s = INVALID_SOCKET;
                continue;
            }

            // connected
            break;
        }

        freeaddrinfo(result);

        if (s == INVALID_SOCKET) {
            std::cerr << "connect failed: " << WSAGetLastError() << "\n";
        }

        m_socket = s;
    }

    void TCPSocket::disconnect()
    {
        if (m_socket != INVALID_SOCKET) {
            ::closesocket(m_socket);
            m_socket = INVALID_SOCKET;
        }
        m_recvBuffer.clear();
    }

    bool TCPSocket::isConnected() const
    {
        return m_socket != INVALID_SOCKET;
    }

    bool TCPSocket::isConnectedTo(const char* host, const char* port) const
    {
        if (m_socket == INVALID_SOCKET) return false;

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

    bool TCPSocket::send(const void* data, int size)
    {
        if (m_socket == INVALID_SOCKET || data == nullptr || size <= 0) return false;
        const char* buf = reinterpret_cast<const char*>(data);
        int sent = 0;
        while (sent < size) {
            int n = ::send(m_socket, buf + sent, size - sent, 0);
            if (n == SOCKET_ERROR) {
                std::cerr << "send failed: " << WSAGetLastError() << "\n";
                return false;
            }
            sent += n;
        }
        return true;
    }

    int TCPSocket::receive(void* buffer, int bufferSize)
    {
        if (m_socket == INVALID_SOCKET || buffer == nullptr || bufferSize <= 0) return -1;
        int n = ::recv(m_socket, reinterpret_cast<char*>(buffer), bufferSize, 0);
        if (n == SOCKET_ERROR) {
            std::cerr << "recv failed: " << WSAGetLastError() << "\n";
            return -1;
        }
        return n;
    }

    bool TCPSocket::sendPacket(const Packet& packet)
    {
        if (m_socket == INVALID_SOCKET) return false;

        std::vector<uint8_t> out;
        packet.serialize(out);
        if (out.empty()) return false;

        const char* buf = reinterpret_cast<const char*>(out.data());
        int total = static_cast<int>(out.size());
        int sent = 0;
        while (sent < total) {
            int n = ::send(m_socket, buf + sent, total - sent, 0);
            if (n == SOCKET_ERROR) {
                std::cerr << "sendPacket failed: " << WSAGetLastError() << "\n";
                return false;
            }
            sent += n;
        }
        return true;
    }

    bool TCPSocket::receivePacket(Packet& outPacket)
    {
        if (m_socket == INVALID_SOCKET) return false;

        // Try to parse any already-received bytes first.
        if (Packet::tryDeserializeFromBuffer(m_recvBuffer, outPacket)) {
            return true;
        }

        // Otherwise keep reading until we can parse a full packet.
        constexpr int TempBufSize = 4096;
        char temp[TempBufSize];

        while (true) {
            int n = ::recv(m_socket, temp, TempBufSize, 0);
            if (n == 0) {
                // connection closed gracefully
                return false;
            }
            if (n == SOCKET_ERROR) {
                std::cerr << "receivePacket recv failed: " << WSAGetLastError() << "\n";
                return false;
            }

            m_recvBuffer.insert(m_recvBuffer.end(), reinterpret_cast<uint8_t*>(temp), reinterpret_cast<uint8_t*>(temp) + n);

            if (Packet::tryDeserializeFromBuffer(m_recvBuffer, outPacket)) {
                return true;
            }

            // otherwise loop and read more
        }
    }
}