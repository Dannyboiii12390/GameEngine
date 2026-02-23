#pragma once
#include <span>

#include <winsock2.h>
#include <ws2tcpip.h>
#include "../Packet.h"
#pragma comment(lib, "Ws2_32.lib")

namespace Networking
{
	class ISocket
	{

	public:

		virtual ~ISocket() = default;

		ISocket() : m_socket(INVALID_SOCKET) {}
		ISocket(SOCKET s) : m_socket(s) {}

		//connect should normally be handled in the constructor
		//disconnect should normally be handled in the destructor

		virtual void connect(const char* host, const char* port) = 0;
		virtual void disconnect() = 0;
		virtual bool isConnected() const = 0;
		virtual bool isConnectedTo(const char* host, const char* port) const = 0;

		virtual bool send(const void* data, int size) = 0;
		virtual int receive(void* buffer, int bufferSize) = 0;

		virtual bool sendPacket(const Packet& packet) = 0;
		virtual bool receivePacket(Packet& packet) = 0;

		SOCKET getSocket() const { return m_socket; }

	protected:
		SOCKET m_socket;

	};

}



