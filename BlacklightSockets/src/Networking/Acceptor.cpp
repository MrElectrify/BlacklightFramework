#include <Networking/Acceptor.h>
#include <Networking/Exception.h>

#include <chrono>
#include <utility>

#ifdef __linux__
#include <unistd.h>
#include <fcntl.h>
#endif

using Blacklight::Networking::Acceptor;
using Blacklight::Networking::Endpoint;
using Blacklight::Networking::ErrorCode;
using Blacklight::Networking::Exception;
using Blacklight::Networking::TCPSocket;
using Blacklight::Threads::Worker;

Acceptor::Acceptor(const Endpoint& localEndpoint, Worker& worker) : m_localEndpoint(localEndpoint), m_worker(worker), m_guard(worker)
{
	if ((m_listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == 0xFFFFFFFF)
		throw Exception("Failed to create listening socket");

	int opt = 1;
	if (setsockopt(m_listenSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&opt), sizeof(opt)))
		throw Exception("Failed to set SO_REUSEADDR");

#ifdef __linux__
	if (setsockopt(m_listenSocket, SOL_SOCKET, SO_REUSEPORT, reinterpret_cast<char*>(&opt), sizeof(opt)))
		throw Exception("Failed to set SO_REUSEPORT");
#endif

	sockaddr_in listenAddr;
	listenAddr.sin_family = AF_INET;
	listenAddr.sin_port = htons(localEndpoint.GetPort());

#ifdef _MSC_VER
	listenAddr.sin_addr.S_un.S_addr = localEndpoint.GetAddrLong();
#elif __linux__
	listenAddr.sin_addr.s_addr = localEndpoint.GetAddrLong();
#endif

	if (bind(m_listenSocket, reinterpret_cast<sockaddr*>(&listenAddr), sizeof(listenAddr)))
		throw Exception("Failed to bind to address");

	if (listen(m_listenSocket, 3))
		throw Exception("Failed to listen");
}

void Acceptor::Accept(TCPSocket& socket)
{
	sockaddr_in peerAddr;

#ifdef _MSC_VER
	int size = sizeof(peerAddr);
#elif __linux__
	socklen_t size = sizeof(peerAddr);
#endif
	auto newSock = AcceptImpl(reinterpret_cast<sockaddr*>(&peerAddr), &size);

	if (newSock == -1)
	{
		// there was an issue accepting
#ifdef _MSC_VER
		throw ErrorCode(WSAGetLastError());
#elif __linux__
		throw ErrorCode(errno);
#endif
	}
	else
	{
		// set nonblocking
#ifdef _WIN32
		u_long arg = 0;
		if (ioctlsocket(newSock, FIONBIO, &arg) == SOCKET_ERROR)
#elif __linux__
		if (fcntl(newSock, F_SETFL, fcntl(newSock, F_GETFL, 0) | O_NONBLOCK) == -1)
#endif
			throw ErrorCode(ErrorCode_DISCONNECTED);

		socket.SetRawSocket(newSock);

#ifdef _MSC_VER
		Endpoint remoteEndpoint(peerAddr.sin_addr.S_un.S_addr, ntohs(peerAddr.sin_port));
#elif __linux__
		Endpoint remoteEndpoint(peerAddr.sin_addr.s_addr, ntohs(peerAddr.sin_port));
#endif

		socket.SetRemoteEndpoint(remoteEndpoint);
	}
}

void Acceptor::Accept(TCPSocket& socket, ErrorCode& e) noexcept
{
	try
	{
		Accept(socket);
	}
	catch (const ErrorCode& ec)
	{
		e = ec;
	}
}

void Acceptor::AsyncAccept(TCPSocket& socket, AcceptCallback_t&& callback)
{
	m_worker.QueueJob(std::bind(&Acceptor::AsyncAcceptImpl, this, std::ref(socket), callback));
}

const Endpoint& Acceptor::GetLocalEndpoint() const
{
	return m_localEndpoint;
}

void Acceptor::AsyncAcceptImpl(TCPSocket& socket, const AcceptCallback_t& callback)
{
	// check if we would block
	fd_set set;
	FD_ZERO(&set);
	FD_SET(m_listenSocket, &set);
	timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 0;

	if (select(m_listenSocket + 1, &set, nullptr, nullptr, &tv) == 0)
	{
		// we would block, let's queue ourselves again
		m_worker.QueueJob(std::bind(&Acceptor::AsyncAcceptImpl, this, std::ref(socket), callback));
		return;
	}

	ErrorCode ec;

	Accept(socket, ec);

	callback(ec);
}

#ifdef _WIN32
int Acceptor::AcceptImpl(sockaddr* addr, int* addrlen)
#elif __linux__
int Acceptor::AcceptImpl(sockaddr* addr, socklen_t* addrlen)
#endif
{
	return accept(m_listenSocket, addr, addrlen);
}