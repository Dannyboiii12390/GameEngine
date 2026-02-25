#include "TCPSocket.h"
#include <ws2tcpip.h>

#include <cstring>
#include <iostream>
#include <stdexcept>

namespace Networking
{

    static bool buildSockaddrForIpPort(const std::string& ip, uint16_t port, sockaddr_in& out) noexcept
    {
        std::memset(&out, 0, sizeof(out));
        out.sin_family = AF_INET;
        out.sin_port = htons(port);

        if (ip.empty()) {
            // caller must provide an IP; treat empty as invalid for connect
            return false;
        }

        int rv = inet_pton(AF_INET, ip.c_str(), &out.sin_addr);
        return (rv == 1);
    }

    TCPSocket::TCPSocket(const Address& addr)
        : m_address(addr), m_socket(INVALID_SOCKET)
    {
        // Create TCP socket
        m_socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (m_socket == INVALID_SOCKET) {
            throw std::runtime_error("TCPSocket: socket() failed - WSA error " + std::to_string(WSAGetLastError()));
        }

        // Build destination sockaddr and connect
        sockaddr_in dest{};
        if (!buildSockaddrForIpPort(m_address.getIP(), m_address.getPort(), dest)) {
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
            throw std::runtime_error("TCPSocket: invalid address for connect");
        }

        if (::connect(m_socket, reinterpret_cast<sockaddr*>(&dest), static_cast<int>(sizeof(dest))) == SOCKET_ERROR) {
            int err = WSAGetLastError();
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
            throw std::runtime_error("TCPSocket: connect() failed - WSA error " + std::to_string(err));
        }
    }

    TCPSocket::~TCPSocket()
    {
        if (m_socket != INVALID_SOCKET) {
            ::shutdown(m_socket, SD_BOTH);
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
        }
    }

    void TCPSocket::Send(const void* data, int size)
    {
        if (m_socket == INVALID_SOCKET) {
            std::cerr << "TCPSocket::Send() socket is invalid\n";
            return;
        }
        if (!data || size <= 0) {
            std::cerr << "TCPSocket::Send() invalid buffer/size\n";
            return;
        }

        const char* ptr = reinterpret_cast<const char*>(data);
        int remaining = size;
        while (remaining > 0) {
            int sent = ::send(m_socket, ptr, remaining, 0);
            if (sent == SOCKET_ERROR) {
                std::cerr << "TCPSocket::Send() send failed - WSA error " << WSAGetLastError() << "\n";
                return;
            }
            remaining -= sent;
            ptr += sent;
        }
    }

    int TCPSocket::Receive(void* buffer, int size)
    {
        if (m_socket == INVALID_SOCKET) {
            std::cerr << "TCPSocket::Receive() socket is invalid\n";
            return -1;
        }
        if (!buffer || size <= 0) {
            std::cerr << "TCPSocket::Receive() invalid buffer/size\n";
            return -1;
        }

        int recvd = ::recv(m_socket, reinterpret_cast<char*>(buffer), size, 0);
        if (recvd == SOCKET_ERROR) {
            int err = WSAGetLastError();
            std::cerr << "TCPSocket::Receive() recv failed - WSA error " << err << "\n";
            return -1;
        }
        return recvd;
    }

    bool TCPSocket::buildSockaddrForAddress(sockaddr_in& out) const noexcept
    {
        return buildSockaddrForIpPort(m_address.getIP(), m_address.getPort(), out);
    }

} // namespace Networking