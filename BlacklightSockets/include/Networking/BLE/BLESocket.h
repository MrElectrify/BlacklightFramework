#ifndef BLACKLIGHT_NETWORKING_BLE_BLESOCKET_H_
#define BLACKLIGHT_NETWORKING_BLE_BLESOCKET_H_

/*
BLESocket
5/27/19 10:21
*/

// BLE
#include <Networking/BLE/Context.h>

// Crypto
#include <Crypto/AES.h>
#include <Crypto/RSA.h>

// STL
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#ifdef _WIN32
#define _WINSOCK_DEPRECATED_NO_WARNINGS 1
#define BOOST_ERROR_CODE_HEADER_ONLY 1
#endif

// boost
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/system/error_code.hpp>

namespace Blacklight
{
	namespace Networking
	{
		namespace BLE
		{
			/*
			 *	Socket that implements Blacklight Encryption (BLE)
			 *	BLE is similar to SSL in that the handshake is
			 *	implemented using RSA and AES, specifically RSA-4096
			 *	for the key transfer and AES-256-GCM for the data
			 *	transmission. This implementation uses boost::asio
			 *	and follows the same snake_case, contrasting the rest
			 *	of the Blacklight framework
			 */
			class BLESocket
			{
			public:
				using Buffer_t = boost::asio::const_buffer;
				using Endpoint_t = boost::asio::ip::tcp::endpoint;
				using ErrorCode_t = boost::system::error_code;
				using IOCallback_t = std::function<void(const ErrorCode_t& ec, size_t bytesTransferred)>;
				using HandshakeCallback_t = std::function<void(const ErrorCode_t&)>;
				using Socket_t = boost::asio::ip::tcp::socket;
				using Worker_t = boost::asio::io_context;
			private:
				// async read
				template<typename MutableBufferSequence,
					typename ReadHandler>
					void AsyncR1(const MutableBufferSequence& buf, const ReadHandler& callback) noexcept
				{
					// do not attempt to read if we aren't connected
					if (is_connected() == false)
						return callback(boost::system::errc::make_error_code(boost::system::errc::not_connected), 0);

					// send raw data during handshake
					if (m_hsInProgress == true)
						return m_socket.async_read_some(buf, callback);

					size_t overflowBytes = std::min(m_overflow.size(), buf.size());

					if (m_overflow.size() > 0)
					{
						// return the overflow
						memcpy(buf.data(), m_overflow.data(), overflowBytes);

						m_overflow.erase(m_overflow.begin(), m_overflow.begin() + overflowBytes);

						if (overflowBytes == buf.size())
							return callback(boost::system::errc::make_error_code(boost::system::errc::success), overflowBytes);
					}

					auto recvBuf = std::make_shared<std::vector<char>>(sizeof(uint64_t) + sizeof(uint32_t) * 2);

					// initially receive the header and size
					boost::asio::async_read(
						m_socket,
						boost::asio::buffer(*recvBuf),
						boost::bind(
							&BLESocket::AsyncR2<MutableBufferSequence, ReadHandler>, this, buf, callback, recvBuf, 
							overflowBytes, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
				}

				template<typename MutableBufferSequence,
					typename ReadHandler>
					void AsyncR2(const MutableBufferSequence& buf, const ReadHandler& callback, std::shared_ptr<std::vector<char>> recvBuf, size_t overflowBytes, const ErrorCode_t& ec, const size_t bytesTransferred) noexcept
				{
					if (ec == boost::asio::error::operation_aborted)
						return callback(ec, 0);

					if (bytesTransferred < static_cast<int>(sizeof(uint64_t) + sizeof(uint32_t) * 2))
						return callback(boost::system::errc::make_error_code(boost::system::errc::bad_message), 0);

					if (CheckMagicNumbers(*recvBuf) == false)
						return callback(boost::system::errc::make_error_code(boost::system::errc::illegal_byte_sequence), 0);

					auto size = *reinterpret_cast<uint64_t*>(&(*recvBuf)[sizeof(uint32_t) * 2]);

					recvBuf->resize(size);

					// ensure we get the whole block so we don't get a decryption issue
					boost::asio::async_read(
						m_socket,
						boost::asio::buffer(*recvBuf),
						boost::bind(
							&BLESocket::AsyncR3<MutableBufferSequence, ReadHandler>, this, buf, callback, recvBuf,
							overflowBytes, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
				}

				template<typename MutableBufferSequence,
					typename ReadHandler>
					void AsyncR3(const MutableBufferSequence& buf, const ReadHandler& callback, std::shared_ptr<std::vector<char>> recvBuf, size_t overflowBytes, const ErrorCode_t& ec, const size_t bytesTransferred) noexcept
				{
					// we got data, let's decrypt it
					try
					{
						auto dec = m_aes.Decrypt(m_key, *recvBuf);

						memcpy(buf.data(), dec.data() + overflowBytes, std::min(dec.size(), buf.size()) - overflowBytes);

						if (buf.size() < dec.size() + overflowBytes)
						{
							size_t diff = dec.size() + overflowBytes - buf.size();

							m_overflow.resize(diff);

							memcpy(&m_overflow[0], dec.data() + overflowBytes + buf.size(), diff);

							return callback(boost::system::errc::make_error_code(boost::system::errc::success), buf.size());
						}

						return callback(boost::system::errc::make_error_code(boost::system::errc::success), dec.size() + overflowBytes);
					}
					catch (...)
					{
						return callback(boost::system::errc::make_error_code(boost::system::errc::invalid_argument), 0);
					}
				}
			public:
				// Constructor with a worker
				BLESocket(Context& context, Worker_t& worker) noexcept;
				BLESocket(BLESocket&& other);

				// Returns the raw underlying socket
				Socket_t& raw_socket() noexcept;

				// Returns the remote endpoint for a connection. Throws on error (not connected)
				Endpoint_t remote_endpoint() const;
				// Returns the remote endpoint for a connection. Returns ErrorCode in ec on error
				Endpoint_t remote_endpoint(ErrorCode_t& ec) const noexcept;

				std::result_of<decltype(&boost::asio::ip::tcp::socket::get_executor)(boost::asio::ip::tcp::socket)>::type get_executor() noexcept
				{
					return m_socket.get_executor();
				}

				// Blocking connect to the specified endpoint. Throws ErrorCode on error
				void connect(const Endpoint_t& endpoint);
				// Blocking connect to the specified endpoint. Returns ErrorCode in ec on error
				void connect(const Endpoint_t& endpoint, ErrorCode_t& ec) noexcept;
				// Asynchronous connect to the specified endpoint. Calls callback with ErrorCode in ec on error. Blocks worker thread while connecting
				template <typename ConnectHandler>
				void async_connect(const Endpoint_t& endpoint, ConnectHandler&& callback) noexcept
				{
					m_client = true;
					m_socket.async_connect(endpoint, callback);
				}

				// I/O Operations:
				// Blocking read from a socket, returns when any data is received with the number of bytes read, limited by the size of buf. Throws ErrorCode on error
				template<typename MutableBufferSequence>
				size_t read_some(const MutableBufferSequence& buf)
				{
					// do not attempt to read if we aren't connected
					if (is_connected() == false)
						throw boost::system::errc::not_connected;

					// send raw data during handshake
					if (m_hsInProgress == true)
						return m_socket.read_some(buf);

					size_t overflowBytes = std::min(m_overflow.size(), buf.size());

					if (m_overflow.size() > 0)
					{
						// return the overflow
						memcpy(buf.data(), m_overflow.data(), overflowBytes);

						m_overflow.erase(m_overflow.begin(), m_overflow.begin() + overflowBytes);

						if (overflowBytes == buf.size())
							return overflowBytes;
					}

					std::vector<char> recvBuf(sizeof(uint64_t) + sizeof(uint32_t) * 2);

					boost::system::error_code ec;

					// initially receive the header and size
					size_t r = boost::asio::read(m_socket, boost::asio::buffer(recvBuf), ec);
					if (ec != boost::system::errc::success)
						throw ec;

					if (r < static_cast<int>(sizeof(uint64_t) + sizeof(uint32_t) * 2))
						return 0;

					if (CheckMagicNumbers(recvBuf) == false)
						return 0;

					auto size = *reinterpret_cast<uint64_t*>(&recvBuf[sizeof(uint32_t) * 2]);

					recvBuf.resize(size);

					// ensure we get the whole block so we don't get a decryption issue
					r = boost::asio::read(m_socket, boost::asio::buffer(recvBuf), ec);
					if (ec != boost::system::errc::success)
						throw ec;

					// we got data, let's decrypt it
					try
					{
						auto dec = m_aes.Decrypt(m_key, recvBuf);

						memcpy(buf.data(), dec.data() + overflowBytes, std::min(dec.size(), buf.size()) - overflowBytes);

						if (buf.size() < dec.size() + overflowBytes)
						{
							size_t diff = dec.size() + overflowBytes - buf.size();

							m_overflow.resize(diff);

							memcpy(&m_overflow[0], dec.data() + overflowBytes + buf.size(), diff);

							return buf.size();
						}

						return dec.size() + overflowBytes;
					}
					catch (std::runtime_error&)
					{
						return -1;
					}
					catch (CryptoPP::Exception&)
					{
						return -1;
					}
				}
				// Blocking read from a socket, returns when any data is received with the number of bytes read, limited by the size of buf. Returns ErrorCode in ec on error
				template<typename MutableBufferSequence>
				size_t read_some(const MutableBufferSequence& buf, ErrorCode_t& ec) noexcept
				{
					try
					{
						ec = boost::system::errc::make_error_code(boost::system::errc::success);
						return read_some(buf);
					}
					catch (const ErrorCode_t& e)
					{
						ec = e;
						return 0;
					}
				}
				// Asynchronous read from a socket, calls the callback when any data is received with the number of bytes read, limited by the size of buf, returns ErrorCode in ec
				template<typename MutableBufferSequence,
					typename ReadHandler>
				void async_read_some(const MutableBufferSequence& buf, ReadHandler&& callback) noexcept
				{
					// we need to treat this as a lambda to avoid weird bind-a-bind fuckery
					AsyncR1(buf, boost::function<void(ErrorCode_t, size_t)>(callback));
				}

				// Blocking write to a socket, returns when some data has been written with the amount written. Throws ErrorCode on error
				template<typename ConstBufferSequence>
				size_t write_some(const ConstBufferSequence& buf)
				{
					boost::system::error_code ec;

					// do not attempt to read if we aren't connected
					if (is_connected() == false)
						throw boost::system::errc::not_connected;

					// send raw data during handshake
					if (m_hsInProgress == true)
					{
						auto sz = m_socket.write_some(buf, ec);

						if (ec != boost::system::errc::success)
							throw ec;

						return sz;
					}

					try
					{
						std::string b(reinterpret_cast<const char*>(buf.data()), buf.size());

						m_iv = m_aes.GenerateIV();
						auto enc = m_aes.Encrypt(m_key, m_iv, b);

						// make space for the uint64_t size and magic number header
						enc.resize(enc.size() + sizeof(uint64_t) + sizeof(uint32_t) * 2);

						std::rotate(enc.begin(), enc.end() - sizeof(uint64_t) - sizeof(uint32_t) * 2, enc.end());

						WriteMagicNumbers(enc);
						// write the size
						*reinterpret_cast<uint64_t*>(&enc[sizeof(uint32_t) * 2]) = enc.size() - sizeof(uint64_t) - sizeof(uint32_t) * 2;

						auto s = boost::asio::write(m_socket, boost::asio::buffer(enc), ec);

						if (ec != boost::system::errc::success)
							throw ec;

						return buf.size();
					}
					catch (std::runtime_error&)
					{
						return -1;
					}
					catch (CryptoPP::Exception&)
					{
						return -1;
					}
				}
				// Blocking write to a socket, returns when some data has been written with the amount written. Returns ErrorCode in ec on error
				template<typename ConstBufferSequence>
				size_t write_some(const ConstBufferSequence& buf, ErrorCode_t& ec) noexcept
				{
					try
					{
						ec = boost::system::errc::make_error_code(boost::system::errc::success);
						return write_some(buf);
					}
					catch (const ErrorCode_t& e)
					{
						ec = e;
						return 0;
					}
				}
				// Asynchronous write to a socket, calls the callback when any data is written to the socket. Returns ErrorCode in ec on error
				template<typename ConstBufferSequence,
					typename WriteHandler>
					void async_write_some(const ConstBufferSequence& buf, WriteHandler&& callback) noexcept
				{
					// do not attempt to read if we aren't connected
					if (is_connected() == false)
						return callback(boost::system::errc::make_error_code(boost::system::errc::not_connected), 0);

					// send raw data during handshake
					if (m_hsInProgress == true)
						return m_socket.async_write_some(buf, callback);

					try
					{
						std::string b(reinterpret_cast<const char*>(buf.data()), buf.size());

						m_iv = m_aes.GenerateIV();
						auto enc = m_aes.Encrypt(m_key, m_iv, b);

						// make space for the uint64_t size and magic number header
						enc.resize(enc.size() + sizeof(uint64_t) + sizeof(uint32_t) * 2);

						std::rotate(enc.begin(), enc.end() - sizeof(uint64_t) - sizeof(uint32_t) * 2, enc.end());

						WriteMagicNumbers(enc);
						// write the size
						*reinterpret_cast<uint64_t*>(&enc[sizeof(uint32_t) * 2]) = enc.size() - sizeof(uint64_t) - sizeof(uint32_t) * 2;

						//callback(boost::system::errc::make_error_code(boost::system::errc::success), 10);
						ErrorCode_t ec;
						auto size = boost::asio::write(m_socket, boost::asio::buffer(enc), ec);
						callback(ec, buf.size());
					}
					catch (const CryptoPP::Exception&)
					{
						callback(boost::system::errc::make_error_code(boost::system::errc::invalid_argument), 0);
					}
				}

				// Shuts down the socket, and calls UpdateDisconnectStatus
				void stop() noexcept;

				// Returns whether or not the socket is connected (may return true if the other end does not gracefull close the connection until a read or write call is made)
				bool is_connected() const noexcept;

				// Blocking BLE handshake (server keypair is initialized if it wasn't already set). Throws ErrorCode on error
				void handshake();
				// Blocking BLE handshake (server keypair is initialized if it wasn't already set). Returns ErrorCode in ec on error
				void handshake(ErrorCode_t& ec) noexcept;
				// Asynchronous BLE handshake (server keypair is initialized if it wasn't already set, currently blocks worker). Returns ErrorCode in ec on error
				void async_handshake(const HandshakeCallback_t& callback) noexcept;
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
				 * 	STAGE 2: The client generates an AES-256 key
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
				void AsyncSS1R(const HandshakeCallback_t& callback, std::vector<char>* buf, const ErrorCode_t& ec, const size_t bytesTransferred) noexcept;
				void SS2(std::vector<char>& buf);
				void AsyncSS2(const HandshakeCallback_t& callback, std::vector<char>* buf, const ErrorCode_t& ec, const size_t bytesTransferred) noexcept;
				void SS3(std::vector<char>& buf);
				void AsyncSS3(const HandshakeCallback_t& callback, std::vector<char>* buf, const ErrorCode_t& ec, const size_t bytesTransferred) noexcept;
				void AsyncSS3W(const HandshakeCallback_t& callback, std::vector<char>* buf, const ErrorCode_t& ec, const size_t bytesTransferred) noexcept;
				void SS3R(std::vector<char>& buf);
				void AsyncSS3R(const HandshakeCallback_t& callback, std::vector<char>* buf, const ErrorCode_t& ec, const size_t bytesTransferred) noexcept;
				// Client handshake
				void CS1(std::vector<char>& buf) noexcept;
				void AsyncCS1(const HandshakeCallback_t& callback) noexcept;
				void CS1W(std::vector<char>& buf) noexcept;
				void AsyncCS1W(const HandshakeCallback_t& callback, std::vector<char>* buf, const ErrorCode_t& ec, const size_t bytesTransferred) noexcept;
				void CS2(std::vector<char>& buf);
				void AsyncCS2(const HandshakeCallback_t& callback, std::vector<char>* buf, const ErrorCode_t& ec, const size_t bytesTransferred) noexcept;
				void CS3(std::vector<char>& buf) noexcept;
				void AsyncCS3(const HandshakeCallback_t& callback, std::vector<char>* buf, const ErrorCode_t& ec, const size_t bytesTransferred) noexcept;
				void CS3R(std::vector<char>& buf);
				void AsyncCS3R(const HandshakeCallback_t& callback, std::vector<char>* buf, const ErrorCode_t& ec, const size_t bytesTransferred) noexcept;
				void CS3W() noexcept;
				void AsyncCS3W(const HandshakeCallback_t& callback, std::vector<char>* buf, const ErrorCode_t& ec, const size_t bytesTransferred) noexcept;

				Crypto::AES<256> m_aes;

				Context& m_context;

				Socket_t m_socket;

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

#endif