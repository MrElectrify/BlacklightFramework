#ifndef BLACKLIGHT_NETWORKING_ENDPOINT_H_
#define BLACKLIGHT_NETWORKING_ENDPOINT_H_

/*
Endpoint implementation
4/18/19 22:05
*/

#if _WIN32 || _WIN64
#include <WinSock2.h>
#elif __linux__
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#endif

#ifdef _WIN32
#undef SetPort
#endif

namespace Blacklight
{
	namespace Networking
	{
		/*
		A class representing a network endpoint, only supporting IPv4 right now
		*/
		class Endpoint
		{
		public:
			using Hostname_t = const char*;
			using IP_t = unsigned long;
			using Port_t = unsigned short;

			// default constructor
			Endpoint();
			// copy constructor
			Endpoint(const Endpoint& other);
			// copy assignment constructor
			Endpoint& operator=(const Endpoint& other);
			// regular constructor
			Endpoint(Hostname_t hostname, Port_t port);
			// constructor for non-resolving context
			Endpoint(IP_t ip, Port_t port);

			// returns the long version of the address
			IP_t GetAddrLong() const;
			// returns a dot-string form of the address
			Hostname_t GetAddrStr() const;
			// returns the port
			Port_t GetPort() const;

			// sets the host after construction
			void SetHost(Hostname_t hostname);
			// sets the port after construction
			void SetPort(Port_t port);

			~Endpoint();
		private:
			IP_t m_ip;
			Port_t m_port;

			// instance counter to keep WSA initialized while endpoints exist
#if _WIN32 || _WIN64
			static WSADATA s_wsa;
			static int s_counter;

			static void CheckInitWSA();
			static void CheckUninitWSA();
#endif
		};
	}
}

#endif