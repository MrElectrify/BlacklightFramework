#ifndef BLACKLIGHT_NETWORKING_BLE_LEGACY_BLESOCKET_H_
#define BLACKLIGHT_NETWORKING_BLE_LEGACY_BLESOCKET_H_

/*
BLESocket
5/27/19 10:21
*/

// BLE
#include <Networking/BLE/Context.h>

// Crypto
#include <Crypto/AES.h>
#include <Crypto/RSA.h>

// Base TCP Socket
#include <Networking/TCPSocket.h>

// STL
#include <string>
#include <vector>

namespace Blacklight
{
	namespace Networking
	{
		namespace BLE
		{
			namespace Legacy
			{
				/*
				 *	Socket that implements Blacklight Encryption (BLE)
				 *	BLE is similar to SSL in that the handshake is
				 *	implemented using RSA and AES, specifically RSA-4096
				 *	for the key transfer and AES-256-GCM for the data
				 *	transmission.
				 */
				class BLESocket : public TCPSocket
				{
				public:
					using impl_t = TCPSocket;
					using HandshakeCallback_t = std::function<void(const ErrorCode&)>;

					// Constructor with a worker
					BLESocket(Context& context, Threads::Worker& worker) noexcept;

					// Returns whether or not we have successfully connected and completed the handshake
					bool IsConnected() const noexcept;

					// Blocking BLE handshake (server keypair is initialized if it wasn't already set). Throws ErrorCode on error
					void Handshake();
					// Blocking BLE handshake (server keypair is initialized if it wasn't already set). Returns ErrorCode in ec on error
					void Handshake(ErrorCode& ec) noexcept;
					// Asynchronous BLE handshake (server keypair is initialized if it wasn't already set, currently blocks worker). Returns ErrorCode in ec on error
					void AsyncHandshake(const HandshakeCallback_t& callback);
				private:
					static constexpr size_t RANDSIZE = 16;

					// buf must already have sizeof(uint32_t) * 2 bytes allocated
					void WriteMagicNumbers(std::vector<char>& buf) const noexcept;
					bool CheckMagicNumbers(const std::vector<char>& buf) const noexcept;

					void UpdateDisconnectStatus() noexcept;

					void AsyncHandshakeImpl(const HandshakeCallback_t& callback) noexcept;

					/*
					 *	BLE Handshake Process:
					 *	STAGE 1: The client sends the magic word header,
					 *	followed by the buffer "enc". The server responds
					 *	with the same magic word header, followed by their
					 *	RSA-4096 public exponent and modulus.
					 *	STAGE 2: The client generates an AES-256 key
					 *	and sends it to the server, encrypted by the RSA
					 *	key that it received from the server.
					 *	STAGE 3: The server sends an encrypted AES-256 packet
					 *	(using the new	key/random iv) with the magic word
					 *	header, and the client sends back an encrypted packet
					 *	(using the same key/new iv)	with the magic word header
					 *	and all future communications are using AES-256.
					 *	The handshake is now complete.
					 */

					 // Server handshake
					void SS1() noexcept;
					void AsyncSS1(const HandshakeCallback_t& callback) noexcept;
					void SS1R(std::vector<char>& buf);
					void AsyncSS1R(const HandshakeCallback_t& callback, std::vector<char>* buf, const ErrorCode& ec, const size_t bytesTransferred) noexcept;
					void SS2(std::vector<char>& buf);
					void AsyncSS2(const HandshakeCallback_t& callback, std::vector<char>* buf, const ErrorCode& ec, const size_t bytesTransferred) noexcept;
					void SS3(std::vector<char>& buf);
					void AsyncSS3(const HandshakeCallback_t& callback, std::vector<char>* buf, const ErrorCode& ec, const size_t bytesTransferred) noexcept;
					void AsyncSS3W(const HandshakeCallback_t& callback, std::vector<char>* buf, const ErrorCode& ec, const size_t bytesTransferred) noexcept;
					void SS3R(std::vector<char>& buf);
					void AsyncSS3R(const HandshakeCallback_t& callback, std::vector<char>* buf, const ErrorCode& ec, const size_t bytesTransferred) noexcept;
					// Client handshake
					void CS1(std::vector<char>& buf) noexcept;
					void AsyncCS1(const HandshakeCallback_t& callback) noexcept;
					void CS1W(std::vector<char>& buf) noexcept;
					void AsyncCS1W(const HandshakeCallback_t& callback, std::vector<char>* buf, const ErrorCode& ec, const size_t bytesTransferred) noexcept;
					void CS2(std::vector<char>& buf);
					void AsyncCS2(const HandshakeCallback_t& callback, std::vector<char>* buf, const ErrorCode& ec, const size_t bytesTransferred) noexcept;
					void CS3(std::vector<char>& buf) noexcept;
					void AsyncCS3(const HandshakeCallback_t& callback, std::vector<char>* buf, const ErrorCode& ec, const size_t bytesTransferred) noexcept;
					void CS3R(std::vector<char>& buf);
					void AsyncCS3R(const HandshakeCallback_t& callback, std::vector<char>* buf, const ErrorCode& ec, const size_t bytesTransferred) noexcept;
					void CS3W() noexcept;
					void AsyncCS3W(const HandshakeCallback_t& callback, std::vector<char>* buf, const ErrorCode& ec, const size_t bytesTransferred) noexcept;

					// call connect, and store that we are in client mode
#ifdef _WIN32
					int ConnectImpl(sockaddr* addr, int len) noexcept;
#elif __linux__
					int ConnectImpl(sockaddr* addr, socklen_t len) noexcept;
#endif

					// override send and recv with BLE variants
					int RecvImpl(void* buf, size_t len, int flags) noexcept;
					int SendImpl(const void* buf, size_t len, int flags) noexcept;

					Crypto::AES<256> m_aes;

					Context& m_context;

					CryptoPP::SecByteBlock m_key;
					CryptoPP::SecByteBlock m_iv;

					bool m_client;					// are we the client?
					bool m_handshake;				// have we completed the handshake?
					bool m_hsInProgress;			// are we currently working on the handshake?

					std::vector<char> m_overflow;	// overflow buffer
				};
			}
		}
	}
}

#endif