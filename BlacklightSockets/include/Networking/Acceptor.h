#ifndef BLACKLIGHT_NETWORKING_ACCEPTOR_H_
#define BLACKLIGHT_NETWORKING_ACCEPTOR_H_

/*
Acceptor for TCP connections
4/18/19 20:34
*/

#include <Networking/Endpoint.h>
#include <Networking/Error.h>
#include <Networking/TCPSocket.h>

#include <Threads/ExecuteGuard.h>
#include <Threads/Worker.h>

#include <functional>

/*
Socket acceptor
4/18/19 17:20
*/

namespace Blacklight
{
	namespace Networking
	{
		// polymorphic socket listener and acceptor
		class Acceptor
		{
		public:
			using AcceptCallback_t = std::function<void(ErrorCode ec)>;
#ifdef _WIN32
			using Socket_t = SOCKET;
#elif __linux__
			using Socket_t = int;
#endif
			// constructor for an acceptor, requires an endpoint to listen on, and a worker for asynchronous calls
			Acceptor(const Endpoint& localEndpoint, Threads::Worker& worker);

			// blocking accept call, throws Blacklight::Network::ErrorCode on fail
			void Accept(TCPSocket& socket);
			// blocking accept call, returning an error code in ec otherwise
			void Accept(TCPSocket& socket, ErrorCode& ec) noexcept;
			// asynchronous accept call, sets up the TCPSocket on success, returns an error code (or success) to callback on completion
			void AsyncAccept(TCPSocket& socket, AcceptCallback_t&& callback);
			
			// returns the local endpoint for the acceptor
			const Endpoint& GetLocalEndpoint() const;
		protected:
			Socket_t m_listenSocket;
			Endpoint m_localEndpoint;

			Threads::Worker& m_worker;
		private:
			Threads::ExecuteGuard<Threads::Worker> m_guard;

			void AsyncAcceptImpl(TCPSocket& socket, const AcceptCallback_t& callback);

#ifdef _WIN32
			virtual int AcceptImpl(sockaddr* addr, int* addrlen);
#elif __linux__
			virtual int AcceptImpl(sockaddr* addr, socklen_t* addrlen);
#endif
		};
	}
}

#endif