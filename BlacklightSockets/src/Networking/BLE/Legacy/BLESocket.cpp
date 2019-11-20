#include <Networking/BLE/Legacy/BLESocket.h>

#include <mutex>

#ifdef __linux__
#include <sys/ioctl.h>
#endif

#define _MAGIC1 0x1173
#define _MAGIC2 0x0235

#define UNUSED(x) (void)x

using Blacklight::Crypto::RSA;
using Blacklight::Networking::BLE::Legacy::BLESocket;
using Blacklight::Networking::BLE::Context;
using Blacklight::Networking::ErrorCode;
using Blacklight::Networking::TCPSocket;
using Blacklight::Threads::Worker;

BLESocket::BLESocket(Context& context, Worker& worker) noexcept : TCPSocket(worker), m_context(context), m_key(m_aes.KEYSIZE), m_iv(m_aes.IVSIZE), m_client(false), m_handshake(false), m_hsInProgress(false) {}

bool BLESocket::IsConnected() const noexcept
{
	return TCPSocket::IsConnected() == true && 
		(m_handshake == true || m_hsInProgress == true);
}

void BLESocket::Handshake()
{
	/*
	 *	BLE Handshake Process:
	 *	STAGE 1: The client sends the magic word header,
	 *	followed by the buffer "enc". The server responds
	 *	with the same magic word header, followed by their
	 *	RSA-2048 public exponent and modulus.
	 *	STAGE 2: The client generates an AES-256 key
	 *	and sends it to the server, encrypted by the RSA
	 *	key that it received from the server.
	 *	STAGE 3: The server sends an encrypted AES-256 packet
	 *	(using the new	key/random iv) with the magic word
	 *	header and a 16 byte random string, and the client 
	 *	sends back an encrypted packet with the magic word header
	 *	and 16 byte random string, and all future communications 
	 *	are encrypted using AES-256. The handshake is now complete.
	 */

	if (TCPSocket::IsConnected() == false)
		throw ErrorCode(ErrorCode_DISCONNECTED);
	
	try
	{
		if (m_client == true)
		{
			// client handshake

			// write magic header	magic words				enc
			std::vector<char> buf(sizeof(uint32_t) * 2 + sizeof(char) * 3);

			CS1(buf);

			Write(buf);

			CS1W(buf);

			Read(buf);

			CS2(buf);

			Write(buf);

			CS3(buf);

			Read(buf);

			CS3R(buf);

			Write(buf);

			CS3W();
		}
		else
		{
			// server handshake
			std::vector<char> buf(sizeof(uint32_t) * 2 + sizeof(char) * 3);

			SS1();

			Read(buf);

			SS1R(buf);

			Write(buf);

			SS2(buf);

			Read(buf);

			SS3(buf);

			Write(buf);

			Read(buf);

			SS3R(buf);
		}
	}
	catch (const ErrorCode& ec)
	{
		// mark ourselves as disconnected
		m_hsInProgress = false;

		throw ec;
	}
}

void BLESocket::Handshake(ErrorCode& ec) noexcept
{
	try
	{
		ec = ErrorCode_SUCCESS;

		Handshake();
	}
	catch (ErrorCode& e)
	{
		ec = e;
	}
}

void BLESocket::AsyncHandshake(const HandshakeCallback_t& callback)
{
	m_worker.QueueJob(std::bind(&BLESocket::AsyncHandshakeImpl, this, callback));
}

// assume space has been allocated
void BLESocket::WriteMagicNumbers(std::vector<char>& buf) const noexcept
{
	*reinterpret_cast<uint32_t*>(&buf[0]) = _MAGIC1;
	*reinterpret_cast<uint32_t*>(&buf[sizeof(uint32_t)]) = _MAGIC2;
}

bool BLESocket::CheckMagicNumbers(const std::vector<char>& buf) const noexcept
{
	const auto magic1 = *reinterpret_cast<const uint32_t*>(&buf[0]);
	const auto magic2 = *reinterpret_cast<const uint32_t*>(&buf[sizeof(uint32_t)]);

	return (magic1 == _MAGIC1 &&
		magic2 == _MAGIC2);
}

void BLESocket::UpdateDisconnectStatus() noexcept
{
	TCPSocket::UpdateDisconnectStatus();

	// reset the handshake status
	m_handshake = false;
}

void BLESocket::AsyncHandshakeImpl(const HandshakeCallback_t& callback) noexcept
{
	ErrorCode ec;

	Handshake(ec);

	callback(ec);
}

void BLESocket::SS1() noexcept
{
	/*
	 *	STAGE 1: The client sends the magic word header,
	 *	followed by the buffer "enc".
	 */

	m_hsInProgress = true;
}

void BLESocket::AsyncSS1(const HandshakeCallback_t& callback) noexcept
{
	// create a buffer and receive with it
	auto buf = new std::vector<char>(sizeof(uint32_t) * 2 + sizeof(char) * 3);

	SS1();

	AsyncRead(*buf, std::bind(&BLESocket::AsyncSS1R, this, callback, buf, std::placeholders::_1, std::placeholders::_2));
}

void BLESocket::SS1R(std::vector<char>& buf)
{
	/*
	 *	The server responds
	 *	with the same magic word header, followed by their
	 *	RSA-2048 public exponent and modulus.
	 */

	// resize the buffer
	if (CheckMagicNumbers(buf) == false ||
		strncmp(buf.data() + sizeof(uint32_t) * 2, "enc", 3) != 0)
		throw ErrorCode(ErrorCode_HANDSHAKE);

	// export the key
	buf.clear();
	buf.resize(sizeof(uint32_t) * 2 + Context::RSAHandle_t::KEYSIZE);

	WriteMagicNumbers(buf);

	// make a keypair if there isn't already one or if it is corrupted
	if (m_context.GetPublicKey().e == 0 ||
		m_context.GetPublicKey().n == 0 ||
		m_context.GetPrivateKey().d == 0)
	{
		static std::mutex keyGenMutex;
		std::lock_guard<std::mutex> keyGenGuard(keyGenMutex);

		auto[priv, pub] = m_context.RSAHandle().GenerateKeys();

		m_context.UseKeyPair(priv, pub);
	}

	// properly align integer for sending
	size_t eZeros = Context::RSAHandle_t::OCTETCOUNT - std::ceil(mpz_sizeinbase(m_context.GetPublicKey().e.get_mpz_t(), 16) / 2.f);
	size_t nZeros = Context::RSAHandle_t::OCTETCOUNT - std::ceil(mpz_sizeinbase(m_context.GetPublicKey().n.get_mpz_t(), 16) / 2.f);

	mpz_export(&buf[sizeof(uint32_t) * 2 + eZeros], nullptr, 1, 1, 1, 0, m_context.GetPublicKey().e.get_mpz_t());
	mpz_export(&buf[sizeof(uint32_t) * 2 + Context::RSAHandle_t::EXPSIZE + nZeros], nullptr, 1, 1, 1, 0, m_context.GetPublicKey().n.get_mpz_t());
}

void BLESocket::AsyncSS1R(const HandshakeCallback_t& callback, std::vector<char>* buf, const ErrorCode& ec, const size_t bytesTransferred) noexcept
{
	UNUSED(bytesTransferred);

	// check for errors
	if (ec != ErrorCode_SUCCESS)
	{
		delete buf;
		m_hsInProgress = false;
		callback(ec);
		return;
	}

	try
	{
		SS1R(*buf);
	}
	catch (const ErrorCode& e)
	{
		delete buf;
		m_hsInProgress = false;
		callback(e);
		return;
	}

	AsyncWrite(*buf, std::bind(&BLESocket::AsyncSS2, this, callback, buf, std::placeholders::_1, std::placeholders::_2));
}

void BLESocket::SS2(std::vector<char>& buf)
{
	/*
	 *	STAGE 2: The client generates an AES-256 key
	 *	and sends it to the server, encrypted by the RSA
	 *	key that it received from the server.
	 */

	buf.resize(Context::RSAHandle_t::OCTETCOUNT);
}

void BLESocket::AsyncSS2(const HandshakeCallback_t& callback, std::vector<char>* buf, const ErrorCode& ec, const size_t bytesTransferred) noexcept
{
	UNUSED(bytesTransferred);

	if (ec != ErrorCode_SUCCESS)
	{
		delete buf;
		m_hsInProgress = false;
		callback(ec);
		return;
	}

	try
	{
		SS2(*buf);
	}
	catch (const ErrorCode& e)
	{
		delete buf;
		m_hsInProgress = false;
		callback(e);
		return;
	}

	AsyncRead(*buf, std::bind(&BLESocket::AsyncSS3, this, callback, buf, std::placeholders::_1, std::placeholders::_2));
}

void BLESocket::SS3(std::vector<char>& buf)
{
	/*
	 *	STAGE 3: The server sends an encrypted AES-256 packet
	 *	(using the new	key/random iv) with the magic word
	 *	header and a 16 byte random string
	 */

	buf = m_context.RSAHandle().Decrypt(m_context.GetPrivateKey(), buf);

	if (CheckMagicNumbers(buf) == false)
		throw ErrorCode(ErrorCode_HANDSHAKE);

	m_key.Assign(reinterpret_cast<CryptoPP::byte*>(&buf[sizeof(uint32_t) * 2]), m_aes.KEYSIZE);

	// STEP 3: send random data

	buf.clear();
	buf.resize(sizeof(uint32_t) * 2 + RANDSIZE);

	WriteMagicNumbers(buf);

	// generate 16 random bytes (to randomize encrypted result)
	CryptoPP::AutoSeededRandomPool rng;
	rng.GenerateBlock(reinterpret_cast<CryptoPP::byte*>(&buf[sizeof(uint32_t) * 2]), RANDSIZE);

	// generate iv
	m_iv = m_aes.GenerateIV();

	buf = m_aes.Encrypt(m_key, m_iv, buf);
}

void BLESocket::AsyncSS3(const HandshakeCallback_t& callback, std::vector<char>* buf, const ErrorCode& ec, const size_t bytesTransferred) noexcept
{
	UNUSED(bytesTransferred);

	if (ec != ErrorCode_SUCCESS)
	{
		delete buf;
		m_hsInProgress = false;
		callback(ec);
		return;
	}

	try
	{
		SS3(*buf);
	}
	catch (const ErrorCode& e)
	{
		delete buf;
		m_hsInProgress = false;
		callback(e);
		return;
	}

	AsyncWrite(*buf, std::bind(&BLESocket::AsyncSS3W, this, callback, buf, std::placeholders::_1, std::placeholders::_2));
}

void BLESocket::AsyncSS3W(const HandshakeCallback_t& callback, std::vector<char>* buf, const ErrorCode& ec, const size_t bytesTransferred) noexcept
{
	UNUSED(bytesTransferred);

	if (ec != ErrorCode_SUCCESS)
	{
		delete buf;
		m_hsInProgress = false;
		callback(ec);
		return;
	}

	AsyncRead(*buf, std::bind(&BLESocket::AsyncSS3R, this, callback, buf, std::placeholders::_1, std::placeholders::_2));
}

void BLESocket::SS3R(std::vector<char>& buf)
{
	/*
	 *	and the client
	 *	sends back an encrypted packet with the magic word header
	 *	and 16 byte random string, and all future communications
	 *	are encrypted using AES-256. The handshake is now complete.
	 */

	// we just need to successfully decrypt their message and we're good

	buf = m_aes.Decrypt(m_key, buf);

	if (CheckMagicNumbers(buf) == false)
		throw ErrorCode(ErrorCode_HANDSHAKE);

	// handshake is complete
	m_handshake = true;
	m_hsInProgress = false;
}

void BLESocket::AsyncSS3R(const HandshakeCallback_t& callback, std::vector<char>* buf, const ErrorCode& ec, const size_t bytesTransferred) noexcept
{
	UNUSED(bytesTransferred);

	if (ec != ErrorCode_SUCCESS)
	{
		delete buf;
		m_hsInProgress = false;
		callback(ec);
		return;
	}

	try
	{
		SS3R(*buf);
	}
	catch (const ErrorCode& e)
	{
		delete buf;
		m_hsInProgress = false;
		callback(e);
		return;
	}

	delete buf;
	callback(ec);
}

void BLESocket::CS1(std::vector<char>& buf) noexcept
{
	/*
	 *	STAGE 1: The client sends the magic word header,
	 *	followed by the buffer "enc". 
	 */
	
	// we allocate on the heap here to pass it to our future callbacks
	WriteMagicNumbers(buf);

	memcpy(&buf[sizeof(uint32_t) * 2], "enc", 3);

	// hacky solution to allow write to work
	m_hsInProgress = true;
}

void BLESocket::AsyncCS1(const HandshakeCallback_t& callback) noexcept
{
	// write magic header				magic words				enc
	auto buf = new std::vector<char>(sizeof(uint32_t) * 2 + sizeof(char) * 3);

	CS1(*buf);

	AsyncWrite(*buf, std::bind(&BLESocket::AsyncCS1W, this, callback, buf, std::placeholders::_1, std::placeholders::_2));
}

void BLESocket::CS1W(std::vector<char>& buf) noexcept
{
	/*
	 *	The server responds
	 *	with the same magic word header, followed by their
	 *	RSA-2048 public exponent and modulus.
	 */

	buf.resize(sizeof(uint32_t) * 2 + Context::RSAHandle_t::KEYSIZE);
}

void BLESocket::AsyncCS1W(const HandshakeCallback_t& callback, std::vector<char>* buf, const ErrorCode& ec, const size_t bytesTransferred) noexcept
{
	UNUSED(bytesTransferred);

	if (ec != ErrorCode_SUCCESS)
	{
		delete buf;
		callback(ec);
		return;
	}

	CS1W(*buf);

	AsyncRead(*buf, std::bind(&BLESocket::AsyncCS2, this, callback, buf, std::placeholders::_1, std::placeholders::_2));
}

void BLESocket::CS2(std::vector<char>& buf)
{
	// bad response
	if (CheckMagicNumbers(buf) == false)
		throw ErrorCode(ErrorCode_HANDSHAKE);

	std::vector<char> exponent(buf.begin() + sizeof(uint32_t) * 2, buf.begin() + sizeof(uint32_t) * 2 + Context::RSAHandle_t::EXPSIZE);
	std::vector<char> modulus(buf.begin() + sizeof(uint32_t) * 2 + Context::RSAHandle_t::EXPSIZE, buf.end());

	Context::RSAHandle_t::PublicKey pub;

	// import the key
	mpz_import(pub.e.get_mpz_t(), Context::RSAHandle_t::EXPSIZE, 1, 1, 1, 0, exponent.data());
	mpz_import(pub.n.get_mpz_t(), Context::RSAHandle_t::MODSIZE, 1, 1, 1, 0, modulus.data());

	m_context.UseKeyPair({}, pub);

	if (m_context.GetPinnedKey().e != 0)
	{
		// we need to verify the key

		if (m_context.GetPublicKey().e != m_context.GetPinnedKey().e ||
			m_context.GetPublicKey().n != m_context.GetPinnedKey().n)
			throw ErrorCode(ErrorCode_HANDSHAKE);
	}

	// STAGE 2: generate AES key
	m_key = m_aes.GenerateKey();

	// encrypt with the public key

	buf.clear();
	buf.resize(sizeof(uint32_t) * 2);

	WriteMagicNumbers(buf);

	std::copy(m_key.begin(), m_key.end(), std::back_inserter(buf));

	buf = m_context.RSAHandle().Encrypt(m_context.GetPublicKey(), buf);
}

void BLESocket::AsyncCS2(const HandshakeCallback_t& callback, std::vector<char>* buf, const ErrorCode& ec, const size_t bytesTransferred) noexcept
{
	UNUSED(bytesTransferred);

	if (ec != ErrorCode_SUCCESS)
	{
		delete buf;
		callback(ec);
		return;
	}
	
	try
	{
		CS2(*buf);
	}
	catch (const ErrorCode& e)
	{
		delete buf;
		m_hsInProgress = false;
		callback(e);
		return;
	}

	AsyncWrite(*buf, std::bind(&BLESocket::AsyncCS3, this, callback, buf, std::placeholders::_1, std::placeholders::_2));
}

void BLESocket::CS3(std::vector<char>& buf) noexcept
{
	buf.resize(m_aes.IVSIZE + m_aes.TAGSIZE + sizeof(uint32_t) * 2 + 16);
}

void BLESocket::AsyncCS3(const HandshakeCallback_t& callback, std::vector<char>* buf, const ErrorCode& ec, const size_t bytesTransferred) noexcept
{
	UNUSED(bytesTransferred);

	if (ec != ErrorCode_SUCCESS)
	{
		delete buf;
		callback(ec);
		return;
	}

	CS3(*buf);

	AsyncRead(*buf, std::bind(&BLESocket::AsyncCS3R, this, callback, buf, std::placeholders::_1, std::placeholders::_2));
}

void BLESocket::CS3R(std::vector<char>& buf)
{
	buf = m_aes.Decrypt(m_key, buf);

	// bad response
	if (CheckMagicNumbers(buf) == false)
		throw ErrorCode(ErrorCode_HANDSHAKE);

	buf.clear();
	buf.resize(sizeof(uint32_t) * 2 + RANDSIZE);

	WriteMagicNumbers(buf);

	// generate 16 random bytes (to randomize encrypted result)
	CryptoPP::AutoSeededRandomPool rng;
	rng.GenerateBlock(reinterpret_cast<CryptoPP::byte*>(&buf[sizeof(uint32_t) * 2]), RANDSIZE);

	// generate a new IV
	m_iv = m_aes.GenerateIV();

	buf = m_aes.Encrypt(m_key, m_iv, buf);
}

void BLESocket::AsyncCS3R(const HandshakeCallback_t& callback, std::vector<char>* buf, const ErrorCode& ec, const size_t bytesTransferred) noexcept
{
	UNUSED(bytesTransferred);

	if (ec != ErrorCode_SUCCESS)
	{
		delete buf;
		callback(ec);
		return;
	}

	try
	{
		CS3R(*buf);
	}
	catch (const ErrorCode& e)
	{
		delete buf;
		m_hsInProgress = false;
		callback(e);
		return;
	}

	AsyncWrite(*buf, std::bind(&BLESocket::AsyncCS3W, this, callback, buf, std::placeholders::_1, std::placeholders::_2));
}

void BLESocket::CS3W() noexcept
{
	// the handshake is complete
	m_handshake = true;
	m_hsInProgress = false;
}

void BLESocket::AsyncCS3W(const HandshakeCallback_t& callback, std::vector<char>* buf, const ErrorCode& ec, const size_t bytesTransferred) noexcept
{
	UNUSED(bytesTransferred);

	if (ec != ErrorCode_SUCCESS)
	{
		delete buf;
		callback(ec);
		return;
	}

	CS3W();

	delete buf;
	callback(ec);
}

#ifdef _WIN32
int BLESocket::ConnectImpl(sockaddr* addr, int len) noexcept
#elif __linux__
int BLESocket::ConnectImpl(sockaddr* addr, socklen_t len) noexcept
#endif
{
	m_client = true;

	return connect(m_socket, addr, len);
}

// override send and recv with BLE variants
int BLESocket::RecvImpl(void* buf, size_t len, int flags) noexcept
{
	// send raw data during handshake
	if (m_hsInProgress == true)
		return recv(m_socket, reinterpret_cast<char*>(buf), len, flags);

	size_t overflowBytes = std::min(m_overflow.size(), len);

	if (m_overflow.size() > 0)
	{
		// return the overflow
		memcpy(buf, m_overflow.data(), overflowBytes);

		m_overflow.erase(m_overflow.begin(), m_overflow.begin() + overflowBytes);

		if (overflowBytes == len)
			return overflowBytes;
	}

	std::vector<char> recvBuf(sizeof(uint64_t) + sizeof(uint32_t) * 2);

	// initially receive the header and size
	int r = recv(m_socket, &recvBuf[0], recvBuf.size(), flags);

	// don't continue unless we have data
	if (r <= 0 &&
		overflowBytes == 0)
		return r;

	if (r <= 0)
		return overflowBytes;

	if (r < static_cast<int>(sizeof(uint64_t) + sizeof(uint32_t) * 2))
		return -1;

	if (CheckMagicNumbers(recvBuf) == false)
		return -1;

	auto size = *reinterpret_cast<uint64_t*>(&recvBuf[sizeof(uint32_t) * 2]);
	
	recvBuf.resize(size);

	// ensure we get the whole block so we don't get a decryption issue
	size_t tr = 0;
	while (tr < size)
	{
		r = recv(m_socket, &recvBuf[tr], size - tr, flags);

		if (r == -1)
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
				return r;
			}

		}
		else if (r == 0)
		{
			return r;
		}

		tr += r;
	}

	// we got data, let's decrypt it
	try
	{
		auto dec = m_aes.Decrypt(m_key, recvBuf);

		memcpy(buf, dec.data() + overflowBytes, len - overflowBytes);

		if (len < dec.size() + overflowBytes)
		{
			size_t diff = dec.size() + overflowBytes - len;

			m_overflow.resize(diff);

			memcpy(&m_overflow[0], dec.data() + overflowBytes + len, diff);

			return len;
		}

		return dec.size() + overflowBytes;
	}
	catch (std::runtime_error& e)
	{
		return -1;
	}
	catch (CryptoPP::Exception& e)
	{
		return -1;
	}
}

int BLESocket::SendImpl(const void* buf, size_t len, int flags) noexcept
{
	// send raw data during handshake
	if (m_hsInProgress == true)
		return send(m_socket, reinterpret_cast<const char*>(buf), len, flags);

	try
	{
		std::string b(reinterpret_cast<const char*>(buf), len);

		m_iv = m_aes.GenerateIV();
		auto enc = m_aes.Encrypt(m_key, m_iv, b);

		// make space for the uint64_t size and magic number header
		enc.resize(enc.size() + sizeof(uint64_t) + sizeof(uint32_t) * 2);

		std::rotate(enc.begin(), enc.end() - sizeof(uint64_t) - sizeof(uint32_t) * 2, enc.end());

		WriteMagicNumbers(enc);
		// write the size
		*reinterpret_cast<uint64_t*>(&enc[sizeof(uint32_t) * 2]) = enc.size() - sizeof(uint64_t) - sizeof(uint32_t) * 2;

		auto s = send(m_socket, enc.data(), enc.size(), flags);

		if (s <= 0)
			return s;

		return len;
	}
	catch (std::runtime_error& e)
	{
		return -1;
	}
	catch (CryptoPP::Exception& e)
	{
		return -1;
	}
}