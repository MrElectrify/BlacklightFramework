#include <Networking/Error.h>
#include <Networking/TCPSocket.h>

#ifdef __linux__
#include <unistd.h>
#include <fcntl.h>
#endif

using Blacklight::Networking::Buffer;
using Blacklight::Networking::Endpoint;
using Blacklight::Networking::ErrorCode;
using Blacklight::Networking::TCPSocket;

TCPSocket::TCPSocket(Blacklight::Threads::Worker& worker) noexcept : m_connected(false), m_socket(0), m_worker(worker) {}

TCPSocket::Socket_t TCPSocket::GetRawSocket() const noexcept
{
	return m_socket;
}

void TCPSocket::SetRawSocket(Socket_t socket) noexcept
{
	m_socket = socket;
}

const Endpoint& TCPSocket::GetRemoteEndpoint() const
{
	// we are not connected, do not return an endpoint. call TCPSocket's method in case there is an extra parameter for child classes, which should not affect raw connection status
	if (TCPSocket::IsConnected() == false)
		throw ErrorCode(ErrorCode_DISCONNECTED);

	return m_remoteEndpoint;
}

const Endpoint& TCPSocket::GetRemoteEndpoint(ErrorCode& ec) const noexcept
{
	try
	{
		ec = ErrorCode_SUCCESS;

		return GetRemoteEndpoint();
	}
	catch (const ErrorCode& e)
	{
		ec = e;
		
		// remoteEndpoint should be default constructed
		return m_remoteEndpoint;
	}
}

void TCPSocket::SetRemoteEndpoint(const Endpoint& remoteEndpoint) noexcept
{
	m_remoteEndpoint = remoteEndpoint;

	// update our connection status
	m_connected = true;
}

void TCPSocket::Connect(const Endpoint& endpoint)
{
	// use the base here, as this is the base for connect
	if (TCPSocket::IsConnected())
		throw ErrorCode(ErrorCode_CONNECTED);

	// make a new socket to use to connect
	m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (m_socket == 0)
#ifdef _WIN32
		throw ErrorCode(WSAGetLastError());
#elif __linux__
		throw ErrorCode(errno);
#endif

	// setup the sockaddr_in
	sockaddr_in remoteAddr;
	remoteAddr.sin_family = AF_INET;
#ifdef _WIN32
	remoteAddr.sin_addr.S_un.S_addr = endpoint.GetAddrLong();
#elif __linux__
	remoteAddr.sin_addr.s_addr = endpoint.GetAddrLong();
#endif
	remoteAddr.sin_port = htons(endpoint.GetPort());

	// use our implemented connect to attempt to connect to the endpoint
	auto res = ConnectImpl(reinterpret_cast<sockaddr*>(&remoteAddr), sizeof(remoteAddr));
	if (res == -1)
	{
#ifdef _WIN32
		throw ErrorCode(WSAGetLastError());
#elif __linux__
		throw ErrorCode(errno);
#endif
	}

	// set nonblocking
#ifdef _WIN32
	u_long arg = 0;
	if (ioctlsocket(m_socket, FIONBIO, &arg) == SOCKET_ERROR)
		throw ErrorCode(WSAGetLastError());
#elif __linux__
	if (fcntl(m_socket, F_SETFL, fcntl(m_socket, F_GETFL, 0) | O_NONBLOCK) == -1)
		throw ErrorCode(errno);
#endif

	// update the remote endpoint now that we've connected, updating our connection status
	SetRemoteEndpoint(endpoint);
}

void TCPSocket::Connect(const Endpoint& endpoint, ErrorCode& ec) noexcept
{
	try
	{
		ec = ErrorCode_SUCCESS;

		Connect(endpoint);
	}
	catch (const ErrorCode& e)
	{
		ec = e;
	}
}

void TCPSocket::AsyncConnect(const Endpoint& endpoint, ConnectCallback_t&& callback) noexcept
{
	m_worker.QueueJob(std::bind(&TCPSocket::AsyncConnectImpl, this, endpoint, callback));
}

// this implementation does not use ReadSome for performance reasons
size_t TCPSocket::Read(const Buffer& buf)
{
	// do not attempt to read if we aren't connected
	if (IsConnected() == false)
		throw ErrorCode(ErrorCode_DISCONNECTED);

	size_t nBytesRead = 0;
	// Do not stop until the buffer is full
	while (buf.GetSize() > nBytesRead)
	{
		auto read = RecvImpl(reinterpret_cast<char*>(buf.GetData()) + nBytesRead, buf.GetSize() - nBytesRead, 0);

		if (read == -1)
		{
#ifdef _WIN32
			if (WSAGetLastError() != WSAEWOULDBLOCK)
#elif __linux__
			if (errno != EWOULDBLOCK &&
				errno != EAGAIN)
#endif
			{
				// update our disconnected state
				Stop();

#ifdef _WIN32
				throw ErrorCode(WSAGetLastError());
#elif __linux__
				throw ErrorCode(errno);
#endif
			}
		}
		else if (read == 0)
		{
			// udpdate our disconnected state
			Stop();

			throw ErrorCode(ErrorCode_EOF);
		}
		else
			nBytesRead += read;
	}

	return nBytesRead;
}

size_t TCPSocket::Read(const Buffer& buf, ErrorCode& ec) noexcept
{
	try
	{
		ec = ErrorCode_SUCCESS;

		return Read(buf);
	}
	catch (const ErrorCode& e)
	{
		ec = e;

		return 0;
	}
}

size_t TCPSocket::ReadSome(const Buffer& buf)
{
	if (IsConnected() == false)
		throw ErrorCode(ErrorCode_DISCONNECTED);

	while (true)
	{
		auto read = RecvImpl(reinterpret_cast<char*>(buf.GetData()), buf.GetSize(), 0);

		if (read == -1)
		{
			// if we got an error other than a nonblocking error, then we are no longer connected
#ifdef _WIN32
			if (WSAGetLastError() != WSAEWOULDBLOCK)
#elif __linux__
			if (errno != EWOULDBLOCK &&
				errno != EAGAIN)
#endif
			{
				// update our disconnected state
				Stop();
			}

#ifdef _WIN32
			throw ErrorCode(WSAGetLastError());
#elif __linux__
			throw ErrorCode(errno);
#endif
		}
		else if (read == 0)
		{
			// update our disconnected state
			Stop();

			throw ErrorCode(ErrorCode_EOF);
		}
		// we read some, return
		else
			return read;
	}
}

size_t TCPSocket::ReadSome(const Buffer& buf, ErrorCode& ec) noexcept
{
	try
	{
		ec = ErrorCode_SUCCESS;

		return ReadSome(buf);
	}
	catch (const ErrorCode& e)
	{
		ec = e;

		return 0;
	}
}

void TCPSocket::AsyncRead(const Buffer& buf, IOCallback_t&& callback) noexcept
{
	m_worker.QueueJob(std::bind(&TCPSocket::AsyncReadImpl, this, buf, callback, 0));
}

void TCPSocket::AsyncReadSome(const Buffer& buf, IOCallback_t&& callback) noexcept
{
	m_worker.QueueJob(std::bind(&TCPSocket::AsyncReadSomeImpl, this, buf, callback));
}

size_t TCPSocket::Write(const Buffer& buf)
{
	// don't attempt a write if we are not connected
	if (IsConnected() == false)
		throw ErrorCode(ErrorCode_DISCONNECTED);

	size_t bytesSent = 0;
	while (buf.GetSize() > bytesSent)
	{
		auto sent = SendImpl(reinterpret_cast<char*>(buf.GetData()) + bytesSent, buf.GetSize() - bytesSent, 0);
		bytesSent += sent;

		if (sent == -1)
		{
			// if we got an error other than a nonblocking error, then we are no longer connected
#ifdef _WIN32
			if (WSAGetLastError() != WSAEWOULDBLOCK)
#elif __linux__
			if (errno != EWOULDBLOCK &&
				errno != EAGAIN)
#endif
			{
				// update our disconnected state
				Stop();

#ifdef _WIN32
				throw ErrorCode(WSAGetLastError());
#elif __linux__
				throw ErrorCode(errno);
#endif
			}
		}
		else
			bytesSent += bytesSent;
	}

	return bytesSent;
}

size_t TCPSocket::Write(const Buffer& buf, ErrorCode& ec) noexcept
{
	try
	{
		ec = ErrorCode_SUCCESS;
		
		return Write(buf);
	}
	catch (const ErrorCode& e)
	{
		ec = e;

		return 0;
	}
}

size_t TCPSocket::WriteSome(const Buffer& buf)
{
	// do not attempt a write if we are not connected
	if (IsConnected() == false)
		throw ErrorCode(ErrorCode_DISCONNECTED);

	while (true)
	{
		auto sent = SendImpl(reinterpret_cast<char*>(buf.GetData()), buf.GetSize(), 0);

		if (sent == -1)
		{
			// if we got an error other than a nonblocking error, then we are no longer connected
#ifdef _WIN32
			if (WSAGetLastError() != WSAEWOULDBLOCK)
#elif __linux__
			if (errno != EWOULDBLOCK &&
				errno != EAGAIN)
#endif
			{
				// update our disconnected state
				Stop();
			}

#ifdef _WIN32
			throw ErrorCode(WSAGetLastError());
#elif __linux__
			throw ErrorCode(errno);
#endif
		}
		// we sent some, return
		else
			return sent;
	}
}

size_t TCPSocket::WriteSome(const Buffer& buf, ErrorCode& ec) noexcept
{
	try
	{
		ec = ErrorCode_SUCCESS;

		return WriteSome(buf);
	}
	catch (const ErrorCode& e)
	{
		ec = e;

		return 0;
	}
}

void TCPSocket::AsyncWrite(const Buffer& buf, IOCallback_t&& callback) noexcept
{
	m_worker.QueueJob(std::bind(&TCPSocket::AsyncWriteImpl, this, buf, callback, 0));
}

void TCPSocket::AsyncWriteSome(const Buffer& buf, IOCallback_t&& callback) noexcept
{
	m_worker.QueueJob(std::bind(&TCPSocket::AsyncWriteSomeImpl, this, buf, callback));
}

void TCPSocket::Stop() noexcept
{
	// shutdown the socket
#ifdef _WIN32
	shutdown(m_socket, SD_BOTH);
#elif __linux__
	shutdown(m_socket, SHUT_RDWR);
#endif

	m_socket = 0;

	// update the disconnected state
	UpdateDisconnectStatus();
}

bool TCPSocket::IsConnected() const noexcept
{
	return m_connected;
}

TCPSocket::~TCPSocket()
{
	if (IsConnected() == true)
		Stop();
}

void TCPSocket::AsyncConnectImpl(const Endpoint& endpoint, const ConnectCallback_t& callback) noexcept
{
	ErrorCode ec;

	Connect(endpoint, ec);

	callback(ec);
}

void TCPSocket::AsyncReadImpl(const Buffer& buf, const IOCallback_t& callback, size_t totalRead) noexcept
{
	auto read = RecvImpl(buf.GetData(), buf.GetSize(), 0);

	if (read == -1)
	{
#ifdef _WIN32
		if (WSAGetLastError() == WSAEWOULDBLOCK)
#elif __linux__
		if (errno == EWOULDBLOCK ||
			errno == EAGAIN)
#endif
		{
			// we would block, queue another
			m_worker.QueueJob(std::bind(&TCPSocket::AsyncReadImpl, this, buf, callback, totalRead));
		}
		else
		{
			// we failed to read
#ifdef _WIN32
			callback(WSAGetLastError(), totalRead);
#elif __linux__
			callback(errno, totalRead);
#endif
		}
	}
	else if (read == 0)
	{
		callback(ErrorCode_EOF, totalRead);
	}
	else
	{
		totalRead += read;

		if (buf.GetSize() > static_cast<unsigned>(read))
		{
			// there is still more data we need to read
			// create a new buffer that is adjusted by how many bytes we read
			Buffer newBuf(reinterpret_cast<char*>(buf.GetData()) + read, buf.GetSize() - read);

			// we can pass newBuf even though it appears by reference, because std::bind actually creates a copy of it
			m_worker.QueueJob(std::bind(&TCPSocket::AsyncReadImpl, this, newBuf, callback, totalRead));
		}
		else
		{
			callback(ErrorCode_SUCCESS, totalRead);
		}
	}
}

void TCPSocket::AsyncReadSomeImpl(const Buffer& buf, const IOCallback_t& callback) noexcept
{
	auto read = RecvImpl(buf.GetData(), buf.GetSize(), 0);

	if (read == -1)
	{
#ifdef _WIN32
		if (WSAGetLastError() == WSAEWOULDBLOCK)
#elif __linux__
		if (errno == EWOULDBLOCK ||
			errno == EAGAIN)
#endif
		{
			// we would block, queue another
			m_worker.QueueJob(std::bind(&TCPSocket::AsyncReadSomeImpl, this, buf, callback));
		}
		else
		{
			// we failed to read
#ifdef _WIN32
			callback(WSAGetLastError(), 0);
#elif __linux__
			callback(errno, 0);
#endif
		}
	}
	else if (read == 0)
	{
		callback(ErrorCode_EOF, 0);
	}
	else
	{
		callback(ErrorCode_SUCCESS, read);
	}
}

void TCPSocket::AsyncWriteImpl(const Buffer& buf, const IOCallback_t& callback, size_t totalSent) noexcept
{
	ErrorCode ec;

	auto sent = WriteSome(buf, ec);

	// update our total sent
	totalSent += sent;

	if (ec != ErrorCode_SUCCESS)
	{
#ifdef _WIN32
		if (ec == WSAEWOULDBLOCK)
#elif __linux__
		if (ec == EWOULDBLOCK ||
			ec == EAGAIN)
#endif
		{
			// we would block, queue another
			m_worker.QueueJob(std::bind(&TCPSocket::AsyncReadImpl, this, buf, callback, totalSent));
		}
		else
		{
			// we failed to read
			callback(ec, totalSent);
		}
	}
	else if (buf.GetSize() > sent)
	{
		// there is still more data we need to read
		// create a new buffer that is adjusted by how many bytes we read
		Buffer newBuf(reinterpret_cast<char*>(buf.GetData()) + sent, buf.GetSize() - sent);

		// we can pass newBuf even though it appears by reference, because std::bind actually creates a copy of it
		m_worker.QueueJob(std::bind(&TCPSocket::AsyncReadImpl, this, newBuf, callback, totalSent));
	}
	else
	{
		callback(ec, totalSent);
	}
}

void TCPSocket::AsyncWriteSomeImpl(const Buffer& buf, const IOCallback_t& callback) noexcept
{
	ErrorCode ec;

	auto sent = WriteSome(buf, ec);

#ifdef _WIN32
	if (ec == WSAEWOULDBLOCK)
#elif __linux__
	if (ec == EWOULDBLOCK ||
		ec == EAGAIN)
#endif
	{
		// we would block, queue another
		m_worker.QueueJob(std::bind(&TCPSocket::AsyncWriteSomeImpl, this, buf, callback));
	}
	else
	{
		callback(ec, sent);
	}
}

void TCPSocket::UpdateDisconnectStatus() noexcept
{
	// clear the remote endpoint
	m_remoteEndpoint = Endpoint();

	m_connected = false;
}

#ifdef _WIN32
int TCPSocket::ConnectImpl(sockaddr* addr, int len) noexcept
#elif __linux__
int TCPSocket::ConnectImpl(sockaddr* addr, socklen_t len) noexcept
#endif
{
	return connect(m_socket, addr, len);
}

int TCPSocket::RecvImpl(void* buf, size_t len, int flags) noexcept
{
	return recv(m_socket, reinterpret_cast<char*>(buf), len, flags);
}

int TCPSocket::SendImpl(const void* buf, size_t len, int flags) noexcept
{
	return send(m_socket, reinterpret_cast<const char*>(buf), len, flags);
}