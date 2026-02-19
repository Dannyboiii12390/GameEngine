
// Simple Winsock TCP echo server (C++14)
// Listens on port 54000 and echoes back any bytes it receives.
//
// Build: Visual Studio will need Ws2_32.lib linked. This file uses a pragma to ensure that.
//
// Behavior:
// - Single listening socket.
// - Each accepted client runs on its own detached std::thread.
// - Server can be stopped with Ctrl+C (SIGINT).
// - Minimal error logging to stdout.

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <thread>
#include <atomic>
#include <csignal>

#pragma comment(lib, "Ws2_32.lib")

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

void client_thread(SOCKET clientSocket, sockaddr_in clientAddr)
{
    char host[NI_MAXHOST] = {0};
    char svc[NI_MAXSERV] = {0};
    if (getnameinfo(reinterpret_cast<sockaddr*>(&clientAddr), sizeof(clientAddr), host, sizeof(host), svc, sizeof(svc), NI_NUMERICHOST | NI_NUMERICSERV) == 0) {
        std::cout << "Client connected: " << host << ":" << svc << "\n";
    } else {
        std::cout << "Client connected (addr unknown)\n";
    }

    const int bufSize = 1024;
    char buf[bufSize];

    while (true) {
        int bytesReceived = recv(clientSocket, buf, bufSize, 0);
        if (bytesReceived > 0) {
            // Echo back exactly what we received
            int bytesSent = 0;
            while (bytesSent < bytesReceived) {
                int n = send(clientSocket, buf + bytesSent, bytesReceived - bytesSent, 0);
                if (n == SOCKET_ERROR) {
                    std::cerr << "send failed: " << WSAGetLastError() << "\n";
                    break;
                }
                bytesSent += n;
            }
        } else if (bytesReceived == 0) {
            // connection closed by client
            std::cout << "Client disconnected\n";
            break;
        } else {
            int err = WSAGetLastError();
            std::cerr << "recv failed: " << err << "\n";
            break;
        }
    }

    closesocket(clientSocket);
}

int main()
{
    // Install a simple Ctrl+C handler to stop the server gracefully.
    std::signal(SIGINT, sigint_handler);

    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (result != 0) {
        std::cerr << "WSAStartup failed: " << result << "\n";
        return 1;
    }

    addrinfo hints;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;          // IPv4
    hints.ai_socktype = SOCK_STREAM;    // TCP
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;        // For bind with INADDR_ANY

    addrinfo* addrResult = nullptr;
    const char* port = "54000";
    result = getaddrinfo(nullptr, port, &hints, &addrResult);
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
    freeaddrinfo(addrResult);
    if (result == SOCKET_ERROR) {
        std::cerr << "bind failed: " << WSAGetLastError() << "\n";
        closesocket(g_listenSocket);
        WSACleanup();
        return 1;
    }

    if (listen(g_listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "listen failed: " << WSAGetLastError() << "\n";
        closesocket(g_listenSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Echo server listening on port " << port << ". Press Ctrl+C to stop.\n";

    while (g_running) {
        sockaddr_in clientAddr;
        int clientAddrLen = sizeof(clientAddr);
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

        // Launch a detached thread to handle this client
        std::thread(client_thread, clientSocket, clientAddr).detach();
    }

    std::cout << "Server shutting down...\n";
    if (g_listenSocket != INVALID_SOCKET) {
        closesocket(g_listenSocket);
        g_listenSocket = INVALID_SOCKET;
    }

    WSACleanup();
    return 0;
}