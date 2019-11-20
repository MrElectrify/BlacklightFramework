#ifndef BLACKLIGHT_SSL_SSLSOCKET_H_
#define BLACKLIGHT_SSL_SSLSOCKET_H_

/*
TLS Socket wrapper for TCP Socket
4/19/19 21:54
*/

#include <Networking/TCPSocket.h>
#include <Networking/TLS/Context.h>

namespace Blacklight
{
	namespace Networking
	{
		namespace TLS
		{
			/*
			SSLSocket is a derived TCPSocket that sends data over a secure protocol (TLSv1.3)
			*/
			class TLSSocket : public TCPSocket
			{
			public:
				using impl_t = TCPSocket;
				using Context_t = TLS::Context;
				using ContextHandle_t = typename Context_t::NativeHandle_t;
				using HandshakeCallback_t = std::function<void(const ErrorCode&)>;
				using SSLHandle_t = ::SSL;

				// Constructor, takes an SSL context by reference (this context must not go out of scope)
				TLSSocket(Context_t& context, Blacklight::Threads::Worker& worker);

				// Sets the remote enpoint for a connection, and updates the connected status. Raw socket is set by the time this function is called by the acceptor or connect function.
				void SetRemoteEndpoint(const Endpoint& remoteEndpoint) noexcept;

				// Returns whether or not the socket is connected (may return true if the other end does not gracefully close the connection until a read or write call is made)
				bool IsConnected() const noexcept;

				// Blocking TLS handshake with the client. Throws ErrorCode on error
				void Handshake();
				// Blocking TLS handshake with the client. Returns ErrorCode in ec on error
				void Handshake(ErrorCode& ec) noexcept;
				// Asynchronous TLS handshake with the client. Returns ErrorCode in ec on error
				void AsyncHandshake(const HandshakeCallback_t& callback);

				~TLSSocket();
			private:
				bool m_client;
				bool m_handshake;
				ContextHandle_t* m_handle;
				SSLHandle_t* m_ssl;

				void UpdateDisconnectStatus() noexcept;

				void AsyncHandshakeImpl(const HandshakeCallback_t& callback) noexcept;

				// call connect, and store that we are in client mode
#ifdef _WIN32
				int ConnectImpl(sockaddr* addr, int len) noexcept;
#elif __linux__
				int ConnectImpl(sockaddr* addr, socklen_t len) noexcept;
#endif

				// override send and recv with SSL variants
				int RecvImpl(void* buf, size_t len, int flags) noexcept;
				int SendImpl(const void* buf, size_t len, int flags) noexcept;
			};
		}
	}
}

#endif