#pragma once
#include <iostream>


#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")


namespace Networking
{
	class Environment
	{
	public:
		Environment()
		{
			WSADATA wsaData;
			int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
			if (result != 0) {
				std::cerr << "WSAStartup failed: " << result << "\n";
			}
		}
		~Environment()
		{
			WSACleanup();
		}
	};
}