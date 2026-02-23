

/*
[ Game Systems ]
        ↓
[ Server Core ]
        ↓
[ Connection Manager ]
        ↓
[ Packet Serializer ]
        ↓
[ Transport Interface ]
        ↓
[ Winsock2 / IOCP ]
*/
/*
Connection
Transport
Event Driven Loop

*/

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <thread>
#include <atomic>
#include <csignal>
#include <memory>

#pragma comment(lib, "Ws2_32.lib")

#include "../ServerWSock2/Sockets/TCPSocket.h"
#include "../ServerWSock2/Sockets/UDPSocket.h"
#include "../ServerWSock2/Packet.h"

static std::atomic_bool g_running(true);
static SOCKET g_listenSocket = INVALID_SOCKET;

void sigint_handler(int)
{
    g_running = false;
    if (g_listenSocket != INVALID_SOCKET) {
        // Closing the listening socket will cause accept() to fail/unblock.
        closesocket(g_listenSocket);
        g_listenSocket = INVALID_SOCKET;
    }
}

void client_thread(std::shared_ptr<Networking::TCPSocket> client, sockaddr_in clientAddr)
{
    char host[NI_MAXHOST] = { 0 };
    char svc[NI_MAXSERV] = { 0 };
    if (getnameinfo(reinterpret_cast<sockaddr*>(&clientAddr), sizeof(clientAddr), host, sizeof(host), svc, sizeof(svc), NI_NUMERICHOST | NI_NUMERICSERV) == 0) {
        std::cout << "Client connected: " << host << ":" << svc << "\n";
    }
    else {
        std::cout << "Client connected (addr unknown)\n";
    }

    const int bufSize = 1024;
    char buf[bufSize];

    // Buffer for assembling incoming TCP stream into packets
    std::vector<uint8_t> tcpRecvBuffer;

    while (g_running && client->isConnected()) {
        int bytesReceived = client->receive(buf, bufSize);
        if (bytesReceived > 0) {
            // append received bytes to stream buffer
            tcpRecvBuffer.insert(tcpRecvBuffer.end(), reinterpret_cast<uint8_t*>(buf), reinterpret_cast<uint8_t*>(buf) + bytesReceived);

            // Try to extract and handle all complete packets present in the buffer
            Networking::Packet pkt;
            while (Networking::Packet::tryDeserializeFromBuffer(tcpRecvBuffer, pkt)) {
                // Echo back the same packet
                std::vector<uint8_t> out;
                pkt.serialize(out);
                if (!out.empty()) {
                    // send may perform full-send; use loop in case of partial socket implementation
                    const char* sendBuf = reinterpret_cast<const char*>(out.data());
                    int total = static_cast<int>(out.size());
                    int sent = 0;
                    while (sent < total) {
                        // original send returns bool on success for entire buffer; call raw send to be safe
                        int n = ::send(client->getSocket(), sendBuf + sent, total - sent, 0);
                        if (n == SOCKET_ERROR) {
                            std::cerr << "send failed (TCPSocket)\n";
                            break;
                        }
                        sent += n;
                    }
                }
            }
        }
        else if (bytesReceived == 0) {
            // connection closed by client
            std::cout << "Client disconnected\n";
            break;
        }
        else {
            std::cerr << "receive failed (TCPSocket)\n";
            break;
        }
    }

    client->disconnect();
}

int main()
{
    // Install a simple Ctrl+C handler to stop the server gracefully.
    std::signal(SIGINT, sigint_handler);

    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        std::cerr << "WSAStartup failed: " << result << "\n";
        return 1;
    }

    // Prepare hints for TCP listening (IPv4)
    addrinfo hintsTcp;
    ZeroMemory(&hintsTcp, sizeof(hintsTcp));
    hintsTcp.ai_family = AF_INET;          // IPv4
    hintsTcp.ai_socktype = SOCK_STREAM;    // TCP
    hintsTcp.ai_protocol = IPPROTO_TCP;
    hintsTcp.ai_flags = AI_PASSIVE;        // For bind with INADDR_ANY

    addrinfo* addrResult = nullptr;
    const char* port = "54000";
    result = getaddrinfo(nullptr, port, &hintsTcp, &addrResult);
    if (result != 0) {
        std::cerr << "getaddrinfo failed: " << result << "\n";
        WSACleanup();
        return 1;
    }

    g_listenSocket = socket(addrResult->ai_family, addrResult->ai_socktype, addrResult->ai_protocol);
    if (g_listenSocket == INVALID_SOCKET) {
        std::cerr << "socket failed: " << WSAGetLastError() << "\n";
        freeaddrinfo(addrResult);
        WSACleanup();
        return 1;
    }

    // Allow address reuse quickly after restart
    int opt = 1;
    setsockopt(g_listenSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

    result = bind(g_listenSocket, addrResult->ai_addr, static_cast<int>(addrResult->ai_addrlen));
    if (result == SOCKET_ERROR) {
        std::cerr << "bind failed (TCP): " << WSAGetLastError() << "\n";
        closesocket(g_listenSocket);
        freeaddrinfo(addrResult);
        WSACleanup();
        return 1;
    }

    // Also set up a UDP socket (UDPSocket) bound to the same port for datagram handling
    Networking::UDPSocket udpSocket;
    // bind UDP socket using the same addrinfo results (addrResult uses AF_INET)
    if (bind(udpSocket.getSocket(), addrResult->ai_addr, static_cast<int>(addrResult->ai_addrlen)) == SOCKET_ERROR) {
        std::cerr << "bind failed (UDP): " << WSAGetLastError() << "\n";
        // Not fatal for TCP server, continue but print warning
    }

    freeaddrinfo(addrResult);

    if (listen(g_listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "listen failed: " << WSAGetLastError() << "\n";
        closesocket(g_listenSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Echo server listening on port " << port << ". Press Ctrl+C to stop.\n";

    // UDP echo thread (parses incoming datagrams as Packets and replies with serialized Packet)
    std::thread([&udpSocket]() {
        const int bufSize = 65536;
        std::vector<uint8_t> buf(bufSize);
        sockaddr_in fromAddr;
        int fromLen = sizeof(fromAddr);

        SOCKET s = udpSocket.getSocket();
        while (g_running && s != INVALID_SOCKET) {
            int n = recvfrom(s, reinterpret_cast<char*>(buf.data()), static_cast<int>(buf.size()), 0, reinterpret_cast<sockaddr*>(&fromAddr), &fromLen);
            if (n == SOCKET_ERROR) {
                int err = WSAGetLastError();
                if (!g_running) break;
                std::cerr << "UDP recvfrom failed: " << err << "\n";
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            std::vector<uint8_t> datagram(buf.begin(), buf.begin() + n);
            Networking::Packet pkt;
            if (Networking::Packet::tryDeserializeFromBuffer(datagram, pkt)) {
                // Serialize and send back as datagram
                std::vector<uint8_t> out;
                pkt.serialize(out);
                if (!out.empty()) {
                    int sent = sendto(s, reinterpret_cast<const char*>(out.data()), static_cast<int>(out.size()), 0, reinterpret_cast<sockaddr*>(&fromAddr), fromLen);
                    if (sent == SOCKET_ERROR) {
                        std::cerr << "UDP sendto failed: " << WSAGetLastError() << "\n";
                    }
                }
            }
            else {
                // Datagram did not contain a valid Packet - ignore or optionally echo raw
            }
            fromLen = sizeof(fromAddr); // reset for next recvfrom
        }
        }).detach();

    while (g_running) {
        sockaddr_in clientAddr;
        int clientAddrLen = static_cast<int>(sizeof(clientAddr));
        SOCKET clientSocket = accept(g_listenSocket, reinterpret_cast<sockaddr*>(&clientAddr), &clientAddrLen);
        if (clientSocket == INVALID_SOCKET) {
            int err = WSAGetLastError();
            if (!g_running) {
                // Server is shutting down; accept was interrupted by closesocket.
                break;
            }
            std::cerr << "accept failed: " << err << "\n";
            // brief sleep to avoid tight loop if accept keeps failing
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        // Wrap the accepted SOCKET in a TCPSocket and start a handler thread
        auto tcpClient = std::make_shared<Networking::TCPSocket>(clientSocket);
        std::thread(client_thread, tcpClient, clientAddr).detach();
    }

    std::cout << "Server shutting down...\n";
    if (g_listenSocket != INVALID_SOCKET) {
        closesocket(g_listenSocket);
        g_listenSocket = INVALID_SOCKET;
    }

    // Ensure UDP socket closed
    udpSocket.disconnect();

    WSACleanup();
    return 0;
}