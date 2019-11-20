#include <Networking/Endpoint.h>
#include <Networking/Exception.h>

// remove
#include <stdio.h>

using Blacklight::Networking::Endpoint;

// windows-specific static members
#if _MSC_VER
WSAData Endpoint::s_wsa;
int Endpoint::s_counter = 0;
#endif

Endpoint::Endpoint() : m_ip(0), m_port(0)
{
#ifdef _MSC_VER
	CheckInitWSA();
#endif
}

Endpoint::Endpoint(const Endpoint& other)
{
#ifdef _MSC_VER
	CheckInitWSA();
#endif

	m_ip = other.m_ip;
	m_port = other.m_port;
}

Endpoint& Endpoint::operator=(const Endpoint& other)
{
#if _MSC_VER
	CheckInitWSA();
#endif

	m_ip = other.m_ip;
	m_port = other.m_port;

	return *this;
}

Endpoint::Endpoint(Hostname_t hostname, Port_t port)
{
#if _MSC_VER
	CheckInitWSA();
#endif

	SetHost(hostname);
	SetPort(port);
}

Endpoint::Endpoint(IP_t ip, Port_t port) : m_ip(ip), m_port(port) {}

Endpoint::IP_t Endpoint::GetAddrLong() const
{
	return m_ip;
}

const char* Endpoint::GetAddrStr() const
{
	return inet_ntoa(*reinterpret_cast<const in_addr*>(&m_ip));
}

Endpoint::Port_t Endpoint::GetPort() const
{
	return m_port;
}

void Endpoint::SetHost(Hostname_t hostname)
{
	auto host = gethostbyname(hostname);

	if (!host)
		throw Exception("Failed to resolve host\n");

	m_ip = *reinterpret_cast<IP_t*>(host->h_addr_list[0]);
}

void Endpoint::SetPort(Port_t port)
{
	m_port = port;
}

Endpoint::~Endpoint()
{
#ifdef _MSC_VER
	CheckUninitWSA();
#endif
}

#ifdef _MSC_VER
void Endpoint::CheckInitWSA()
{
	if (s_counter == 0)
	{
		if (WSAStartup(MAKEWORD(2, 0), &s_wsa))
			throw Exception("Failed to startup WSA\n");
	}

	++s_counter;
}

void Endpoint::CheckUninitWSA()
{
	--s_counter;

	if (s_counter == 0)
	{
		WSACleanup();
	}
}
#endif