#ifndef BLACKLIGHT_SOCKETS_H_
#define BLACKLIGHT_SOCKETS_H_

/*
TCP socket implementation
4/18/19 23:48
*/

#include <Networking/Buffer.h>
#include <Networking/Endpoint.h>
#include <Networking/Error.h>
#include <Threads/Worker.h>

#include <functional>

namespace Blacklight
{
	namespace Networking
	{
		/*
		A polymorphic callback-based wrapper for a TCP socket inspired by boost::asio
		*/
		class TCPSocket
		{
		public:
			using ConnectCallback_t = std::function<void(const ErrorCode& ec)>;
			using IOCallback_t = std::function<void(const ErrorCode& ec, size_t bytesTransferred)>;
#ifdef _MSC_VER
			using Socket_t = SOCKET;
#elif __linux__
			using Socket_t = int;
#endif

			/*
			Calling a read or write call at the same time one is already executing will cause undefined behavior.
			Setting socket options using the raw socket may cause deadlocks. When we disconnect, the remote endpoint
			is cleared before returning an error code, so it may be necessary to store the connection information
			in a wrapper class if that is necessary to the operation of the application.
			*/

			// Default constructor
			TCPSocket(Threads::Worker& worker) noexcept;

			// Returns the raw underlying socket
			Socket_t GetRawSocket() const noexcept;
			// Sets the raw underlying socket
			void SetRawSocket(Socket_t socket) noexcept;

			// Returns the remote endpoint for a connection. Throws on error (not connected)
			const Endpoint& GetRemoteEndpoint() const;
			// Returns the remote endpoint for a connection. Returns ErrorCode in ec on error
			const Endpoint& GetRemoteEndpoint(ErrorCode& ec) const noexcept;
			// Sets the remote enpoint for a connection, and updates the connected status. Raw socket is set by the time this function is called by the acceptor or connect function.
			virtual void SetRemoteEndpoint(const Endpoint& remoteEndpoint) noexcept;

			// Blocking connect to the specified endpoint. Throws ErrorCode on error
			void Connect(const Endpoint& endpoint);
			// Blocking connect to the specified endpoint. Returns ErrorCode in ec on error
			void Connect(const Endpoint& endpoint, ErrorCode& ec) noexcept;
			// Asynchronous connect to the specified endpoint. Calls callback with ErrorCode in ec on error. Blocks worker thread while connecting
			void AsyncConnect(const Endpoint& endpoint, ConnectCallback_t&& callback) noexcept;
			
			// I/O Operations:
			// Blocking read from a socket, does not return until the buffer is full. Returns number of bytes read. Throws ErrorCode on error
			size_t Read(const Buffer& buf);
			// Blocking read from a socket, does not return until the buffer is full. Returns number of bytes read. Returns ErrorCode in ec on error
			size_t Read(const Buffer& buf, ErrorCode& ec) noexcept;
			// Blocking read from a socket, returns when any data is received with the number of bytes read, limited by the size of buf. Throws ErrorCode on error
			size_t ReadSome(const Buffer& buf);
			// Blocking read from a socket, returns when any data is received with the number of bytes read, limited by the size of buf. Returns ErrorCode in ec on error
			size_t ReadSome(const Buffer& buf, ErrorCode& ec) noexcept;
			// Asynchronous read from a socket, does not call the callback until the buffer is full, returns ErrorCode in ec
			void AsyncRead(const Buffer& buf, IOCallback_t&& callback) noexcept;
			// Asynchronous read from a socket, calls the callback when any data is received with the number of bytes read, limited by the size of buf, returns ErrorCode in ec
			void AsyncReadSome(const Buffer& buf, IOCallback_t&& callback) noexcept;
			// Blocking read from a socket, returns when the buffer is full. Throws ErrorCode on error

			// Blocking write to a socket, does not return until the buffer has been written fully. Returns number of bytes written. Throws ErrorCode on error
			size_t Write(const Buffer& buf);
			// Blocking write to a socket, does not return until the buffer has been written fully. Returns number of bytes written. Returns ErrorCode in ec on error
			size_t Write(const Buffer& buf, ErrorCode& ec) noexcept;
			// Blocking write to a socket, returns when some data has been written with the amount written. Throws ErrorCode on error
			size_t WriteSome(const Buffer& buf);
			// Blocking write to a socket, returns when some data has been written with the amount written. Returns ErrorCode in ec on error
			size_t WriteSome(const Buffer& buf, ErrorCode& ec) noexcept;
			// Asynchronous write to a socket, does not call the callback until the buffer has been written fully. Returns ErrorCode in ec on error
			void AsyncWrite(const Buffer& buf, IOCallback_t&& callback) noexcept;
			// Asynchronous write to a socket, calls the callback when any data is written to the socket. Returns ErrorCode in ec on error
			void AsyncWriteSome(const Buffer& buf, IOCallback_t&& callback) noexcept;

			// Shuts down the socket, and calls UpdateDisconnectStatus
			virtual void Stop() noexcept;

			// Returns whether or not the socket is connected (may return true if the other end does not gracefull close the connection until a read or write call is made)
			virtual bool IsConnected() const noexcept;

			virtual ~TCPSocket();
		protected:
			bool m_connected;
			Endpoint m_remoteEndpoint;
			Socket_t m_socket;
			Threads::Worker& m_worker;

			// updates our disconnected status and clears the remote endpoint
			virtual void UpdateDisconnectStatus() noexcept;
		private:
			// implicit thread call to connect to endpoint
			void AsyncConnectImpl(const Endpoint& endpoint, const ConnectCallback_t& callback) noexcept;
			// implicit thread call to read entire buffer
			void AsyncReadImpl(const Buffer& buf, const IOCallback_t& callback, size_t totalRead) noexcept;
			// implicit thread call to read some of the buffer
			void AsyncReadSomeImpl(const Buffer& buf, const IOCallback_t& callback) noexcept;
			// implicit thread call to write entire buffer
			void AsyncWriteImpl(const Buffer& buf, const IOCallback_t& callback, size_t totalWritten) noexcept;
			// implicit thread call to write some of the buffer
			void AsyncWriteSomeImpl(const Buffer& buf, const IOCallback_t& callback) noexcept;

			// low-level socket ops. To be implemented by any children that use custom socket ops

#ifdef _WIN32
			virtual int ConnectImpl(sockaddr* addr, int len) noexcept;
#elif __linux__
			virtual int ConnectImpl(sockaddr* addr, socklen_t len) noexcept;
#endif
			virtual int RecvImpl(void* buf, size_t len, int flags) noexcept;
			virtual int SendImpl(const void* buf, size_t len, int flags) noexcept;
		};
	}
}

#endif