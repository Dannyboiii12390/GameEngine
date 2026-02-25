#pragma once

#include "Address.h"
#include <winsock2.h>


namespace Networking
{
	class TCPSocket
	{
	public:
		// Creates the UDP socket and binds it to the provided local address.
		// Throws std::runtime_error on fatal errors.
		TCPSocket(const Address& addr);

		// Closes the socket.
		~TCPSocket();

		// Send `size` bytes from `data` to the remote address specified in the constructor.
		// Writes error info to stderr on failure.
		void Send(const void* data, int size);

		// Receive up to `size` bytes into `buffer` from the remote address specified in the constructor.
		// This call blocks until a datagram is received (or an error occurs).
		// If a datagram is received from a different peer it is ignored and the call continues waiting.
		// On success the function returns the number of bytes written into `buffer`.
		// On error it returns -1.
		int Receive(void* buffer, int size);

		// Optional: expose underlying socket for advanced usage
		SOCKET native_handle() const noexcept { return m_socket; }

	private:
		Address m_address;
		SOCKET m_socket = INVALID_SOCKET;

		// Helper to build sockaddr_in from m_address
		bool buildSockaddrForAddress(sockaddr_in& out) const noexcept;
	};
}