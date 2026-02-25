#pragma once
#include <string>

namespace Networking
{
	class Address
	{
	public:
		Address(std::string ip, uint16_t port)
			: m_ip(std::move(ip)), m_port(port)
		{
		}
		Address(std::string ip, std::string portStr)
			: m_ip(std::move(ip)), m_port(static_cast<uint16_t>(std::stoi(portStr)))
		{
		}
		Address& operator=(const Address& other) = default;
		Address(const Address& other) = default;

		std::string getIP() const { return m_ip; }

		uint16_t getPort() const { return m_port; }

	private:

		std::string m_ip;
		uint16_t m_port;
	};
}
