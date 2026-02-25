#pragma once

#include "Address.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include "Environment.h"

namespace Networking
{
	// Simple TCP listening socket wrapper.
	// - Constructor creates socket, sets SO_REUSEADDR, binds and starts listening.
	// - Accept returns a native SOCKET for the accepted client and fills outClient.
	// - Destructor closes the listening socket.
	class ListeningSocket
	{
	public:
		// Create, bind and listen on the provided local address. Throws std::runtime_error on failure.
		ListeningSocket(const std::shared_ptr<Environment> p_env, const Address& bindAddr, int backlog = SOMAXCONN);

		// Close socket on destruction.
		~ListeningSocket();

		// Accept a pending connection. Returns INVALID_SOCKET on error or when non-blocking and no connection is ready.
		// On success, returns the accepted native SOCKET and fills outClient with the peer address/port.
		SOCKET Accept(Address& outClient);

		// Set blocking / non-blocking mode. Returns false on failure.
		bool SetNonBlocking(bool nonBlocking);

		// Close the listening socket explicitly.
		void Close();

		bool IsValid() const noexcept { return m_socket != INVALID_SOCKET; }
		SOCKET NativeHandle() const noexcept { return m_socket; }

	private:
		bool buildSockaddr(sockaddr_in& out) const noexcept;

	private:
		Address m_address;
		SOCKET m_socket = INVALID_SOCKET;

		std::shared_ptr<Environment> m_env; // keep shared ownership of environment to ensure it outlives this socket
	};
}