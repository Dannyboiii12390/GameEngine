#include "ListeningSocket.h"

#include <cstring>
#include <iostream>
#include <stdexcept>
#include <memory>
#include <ws2tcpip.h>

namespace Networking
{

    ListeningSocket::ListeningSocket(const Address& bindAddr, int backlog)
        : m_address(bindAddr), m_socket(INVALID_SOCKET)
    {

        m_socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (m_socket == INVALID_SOCKET) {
            throw std::runtime_error("ListeningSocket: socket() failed - WSA error " + std::to_string(WSAGetLastError()));
        }

        // Allow quick reuse of the address/port
        int opt = 1;
        if (::setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt)) == SOCKET_ERROR) {
            int err = WSAGetLastError();
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
            throw std::runtime_error("ListeningSocket: setsockopt(SO_REUSEADDR) failed - WSA error " + std::to_string(err));
        }

        sockaddr_in local{};
        if (!buildSockaddr(local)) {
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
            throw std::runtime_error("ListeningSocket: invalid bind address");
        }

        if (::bind(m_socket, reinterpret_cast<sockaddr*>(&local), static_cast<int>(sizeof(local))) == SOCKET_ERROR) {
            int err = WSAGetLastError();
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
            throw std::runtime_error("ListeningSocket: bind() failed - WSA error " + std::to_string(err));
        }

        if (::listen(m_socket, backlog) == SOCKET_ERROR) {
            int err = WSAGetLastError();
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
            throw std::runtime_error("ListeningSocket: listen() failed - WSA error " + std::to_string(err));
        }
    }

    ListeningSocket::~ListeningSocket()
    {
        Close();
    }

    SOCKET ListeningSocket::Accept(Address& outClient)
    {
        if (m_socket == INVALID_SOCKET) {
            std::cerr << "ListeningSocket::Accept() invalid listening socket\n";
            return INVALID_SOCKET;
        }

        sockaddr_in peer{};
        int peerLen = static_cast<int>(sizeof(peer));
        SOCKET clientSock = ::accept(m_socket, reinterpret_cast<sockaddr*>(&peer), &peerLen);

        if (clientSock == INVALID_SOCKET) {
            int err = WSAGetLastError();
            // Non-blocking mode may return WSAEWOULDBLOCK when no pending connection
            if (err == WSAEWOULDBLOCK || err == WSAEINPROGRESS) {
                return INVALID_SOCKET;
            }
            std::cerr << "ListeningSocket::Accept() accept failed - WSA error " << err << "\n";
            return INVALID_SOCKET;
        }

        char addrBuf[INET_ADDRSTRLEN] = { 0 };
        if (inet_ntop(AF_INET, &peer.sin_addr, addrBuf, static_cast<int>(sizeof(addrBuf))) == nullptr) {
            // Fallback: unknown IP, set empty string and port 0
            outClient = Address(std::string(), 0);
        }
        else {
            outClient = Address(std::string(addrBuf), ntohs(peer.sin_port));
        }

        return clientSock;
    }

    bool ListeningSocket::SetNonBlocking(bool nonBlocking)
    {
        if (m_socket == INVALID_SOCKET) return false;
        u_long mode = nonBlocking ? 1u : 0u;
        int rv = ioctlsocket(m_socket, FIONBIO, &mode);
        if (rv != 0) {
            std::cerr << "ListeningSocket::SetNonBlocking() failed - WSA error " << WSAGetLastError() << "\n";
            return false;
        }
        return true;
    }

    void ListeningSocket::Close()
    {
        if (m_socket != INVALID_SOCKET) {
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
        }
    }

    bool ListeningSocket::buildSockaddr(sockaddr_in& out) const noexcept
    {
        std::memset(&out, 0, sizeof(out));
        out.sin_family = AF_INET;
        out.sin_port = htons(m_address.getPort());

        const std::string& ip = m_address.getIP();
        if (ip.empty() || ip == "0.0.0.0") {
            out.sin_addr.s_addr = INADDR_ANY;
            return true;
        }

        int rv = inet_pton(AF_INET, ip.c_str(), &out.sin_addr);
        return (rv == 1);
    }

} // namespace Networking