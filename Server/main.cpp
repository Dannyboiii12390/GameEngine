#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <chrono>

#include "../PhysicsEngine/Networking/Environment.h"
#include "../PhysicsEngine/Networking/Address.h"
#include "../PhysicsEngine/Networking/ListeningSocket.h"
#include "../PhysicsEngine/Networking/Packet.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <flatbuffers/flatbuffers.h>

static void handleClient(SOCKET clientSock, Networking::Address clientAddr)
{
    std::cout << "Client connected: " << clientAddr.getIP() << ":" << clientAddr.getPort() << "\n";

    std::vector<uint8_t> recvBuffer;               // accumulated stream buffer
    recvBuffer.reserve(8192);

    constexpr int TEMP_BUF_SIZE = 4096;
    std::vector<uint8_t> temp(TEMP_BUF_SIZE);

    while (true)
    {
        int received = ::recv(clientSock,
            reinterpret_cast<char*>(temp.data()),
            static_cast<int>(temp.size()),
            0);

        if (received == 0) {
            std::cout << "Client disconnected: " << clientAddr.getIP() << ":" << clientAddr.getPort() << "\n";
            break;
        }
        if (received == SOCKET_ERROR) {
            int err = WSAGetLastError();
            std::cerr << "recv failed for " << clientAddr.getIP() << ":" << clientAddr.getPort() << " - WSA error " << err << "\n";
            break;
        }

        // append to accumulator
        recvBuffer.insert(recvBuffer.end(), temp.begin(), temp.begin() + received);

        // attempt to extract packets from the accumulator
        Networking::Packet pkt;
        while (Networking::Packet::tryDeserializeFromBuffer(recvBuffer, pkt)) {
            // Process packet (example: print info and echo back)
            std::string payloadText;
            if (pkt.payloadSize() > 0) {
                const uint8_t* p = pkt.payloadData();
                // Try to interpret payload as a FlatBuffers root string.
                const flatbuffers::String* fbStr = flatbuffers::GetRoot<flatbuffers::String>(p);
                if (fbStr && fbStr->c_str()) {
                    payloadText.assign(fbStr->c_str());
                }
                else {
                    payloadText.assign(reinterpret_cast<const char*>(p), pkt.payloadSize());
                }
            }

            std::cout << "Received packet type=" << pkt.header.type
                << " payloadSize=" << pkt.payloadSize()
                << " text=\"" << payloadText << "\"\n";

            // Echo the packet back (preserve original payload bytes)
            std::vector<uint8_t> out;
            pkt.serialize(out);

            int totalSent = 0;
            while (totalSent < static_cast<int>(out.size())) {
                int sent = ::send(clientSock,
                    reinterpret_cast<const char*>(out.data() + totalSent),
                    static_cast<int>(out.size() - totalSent),
                    0);
                if (sent == SOCKET_ERROR) {
                    std::cerr << "send failed to " << clientAddr.getIP() << ":" << clientAddr.getPort()
                        << " - WSA error " << WSAGetLastError() << "\n";
                    goto cleanup;
                }
                totalSent += sent;
            }
        }
    }

cleanup:
    closesocket(clientSock);
}

int main()
{
    try
    {
		auto env = std::make_shared<Networking::Environment>();

        // Listen on all interfaces, port 54000
        Networking::Address bindAddr("0.0.0.0", 54000);

        Networking::ListeningSocket listener(env, bindAddr, SOMAXCONN);
        std::cout << "Server listening on port " << bindAddr.getPort() << "\n";

        while (true)
        {
            Networking::Address clientAddr("", 0);
            SOCKET clientSock = listener.Accept(clientAddr);

            if (clientSock == INVALID_SOCKET) {
                // No pending connection right now (if non-blocking) or transient error.
                // Sleep briefly to avoid busy loop.
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                continue;
            }

            // Spawn a detached thread to service the client
            std::thread t(handleClient, clientSock, clientAddr);
            t.detach();
        }
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Server fatal error: " << ex.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}