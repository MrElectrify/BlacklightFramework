#include <Networking/TLS/TLSSocket.h>

#define UNUSED(x) (void)x

using Blacklight::Networking::Endpoint;
using Blacklight::Networking::ErrorCode;
using Blacklight::Networking::TLS::Context;
using Blacklight::Networking::TLS::TLSSocket;
using Blacklight::Threads::Worker;

TLSSocket::TLSSocket(Context& context, Worker& worker) 
	: TCPSocket(worker), m_client(false), m_handshake(false)
{
	// store the native SSL handle
	m_handle = context.GetNativeHandle();

	m_ssl = SSL_new(m_handle);
}

void TLSSocket::SetRemoteEndpoint(const Endpoint& remoteEndpoint) noexcept
{
	// set the SSL file descriptor
	SSL_set_fd(m_ssl, m_socket);

	TCPSocket::SetRemoteEndpoint(remoteEndpoint);
}

bool TLSSocket::IsConnected() const noexcept
{
	// check to make sure the handshake was completed
	return TCPSocket::IsConnected() == true
		&& m_handshake == true;
}

void TLSSocket::Handshake()
{
	if (TCPSocket::IsConnected() == false)
		throw ErrorCode(ErrorCode_DISCONNECTED);

	int res;

	if (m_client)
	{
		res = SSL_connect(m_ssl);
	}
	else
	{
		res = SSL_accept(m_ssl);
	}

	if (res != 1)
	{
		throw ErrorCode(ErrorCode_HANDSHAKE);
	}

	m_handshake = true;
}

void TLSSocket::Handshake(ErrorCode& ec) noexcept
{
	try
	{
		ec = ErrorCode_SUCCESS;

		Handshake();
	}
	catch (const ErrorCode& e)
	{
		ec = e;
	}
}

void TLSSocket::AsyncHandshake(const HandshakeCallback_t& callback)
{
	m_worker.QueueJob(std::bind(&TLSSocket::AsyncHandshakeImpl, this, callback));
}

TLSSocket::~TLSSocket()
{
	// free their SSL handle
	SSL_free(m_ssl);
} 

void TLSSocket::UpdateDisconnectStatus() noexcept
{
	TCPSocket::UpdateDisconnectStatus();

	// reset the handshake status
	m_handshake = false;
}

void TLSSocket::AsyncHandshakeImpl(const HandshakeCallback_t& callback) noexcept
{
	ErrorCode ec;

	Handshake(ec);

	callback(ec);
}

#ifdef _WIN32
int TLSSocket::ConnectImpl(sockaddr* addr, int len) noexcept
#elif __linux__
int TLSSocket::ConnectImpl(sockaddr* addr, socklen_t len) noexcept
#endif
{
	// store the fact that we are a client
	m_client = true;

	return connect(m_socket, addr, len);
}

int TLSSocket::RecvImpl(void* buf, size_t len, int flags) noexcept
{
	UNUSED(flags);

	// update linux nonblocking status

	return SSL_read(m_ssl, buf, len);
}

int TLSSocket::SendImpl(const void* buf, size_t len, int flags) noexcept
{
	UNUSED(flags);

	return SSL_write(m_ssl, buf, len);
}