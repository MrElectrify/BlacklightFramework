#include <Networking/BLE/BLESocket.h>

#define _MAGIC1 0x1173
#define _MAGIC2 0x0235

#define UNUSED(x) (void)x

using Blacklight::Networking::BLE::BLESocket;

BLESocket::BLESocket(Context& context, Worker_t& worker) noexcept
	: m_context(context), m_socket(worker), m_key(m_aes.KEYSIZE), m_iv(m_aes.IVSIZE), m_client(false), m_handshake(false), m_hsInProgress(false) {}

BLESocket::BLESocket(BLESocket&& other) 
	: m_aes(std::move(other.m_aes)), m_context(other.m_context), m_socket(std::move(other.m_socket)), 
	m_key(std::move(other.m_key)), m_iv(std::move(other.m_iv)), m_overflow(std::move(other.m_overflow))
{
	m_client = other.m_client;
	m_handshake = other.m_handshake;
	m_hsInProgress = other.m_hsInProgress;
}

BLESocket::Socket_t& BLESocket::raw_socket() noexcept
{
	return m_socket;
}

BLESocket::Endpoint_t BLESocket::remote_endpoint() const
{
	return m_socket.remote_endpoint();
}

BLESocket::Endpoint_t BLESocket::remote_endpoint(ErrorCode_t& ec) const noexcept
{
	return m_socket.remote_endpoint(ec);
}

void BLESocket::connect(const Endpoint_t& endpoint)
{
	m_client = true;

	m_socket.connect(endpoint);
}

void BLESocket::connect(const Endpoint_t& endpoint, ErrorCode_t& ec) noexcept
{
	m_client = true;

	m_socket.connect(endpoint, ec);
}

void BLESocket::stop() noexcept
{
	ErrorCode_t ec;

	m_socket.shutdown(m_socket.shutdown_both, ec);
	m_socket.close(ec);
}

bool BLESocket::is_connected() const noexcept
{
	return m_socket.is_open() == true &&
		(m_handshake == true || m_hsInProgress == true);
}

void BLESocket::handshake()
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

	if (m_socket.is_open() == false)
		throw boost::system::errc::not_connected;

	try
	{
		if (m_client == true)
		{
			// client handshake

			// write magic header	magic words				enc
			std::vector<char> buf(sizeof(uint32_t) * 2 + sizeof(char) * 3);

			CS1(buf);

			boost::asio::write(m_socket, boost::asio::buffer(buf));

			CS1W(buf);

			boost::asio::read(m_socket, boost::asio::buffer(buf));

			CS2(buf);

			boost::asio::write(m_socket, boost::asio::buffer(buf));

			CS3(buf);

			boost::asio::read(m_socket, boost::asio::buffer(buf));

			CS3R(buf);

			boost::asio::write(m_socket, boost::asio::buffer(buf));

			CS3W();
		}
		else
		{
			// server handshake
			std::vector<char> buf(sizeof(uint32_t) * 2 + sizeof(char) * 3);

			SS1();

			boost::asio::read(m_socket, boost::asio::buffer(buf));

			SS1R(buf);

			boost::asio::write(m_socket, boost::asio::buffer(buf));

			SS2(buf);

			boost::asio::read(m_socket, boost::asio::buffer(buf));

			SS3(buf);

			boost::asio::write(m_socket, boost::asio::buffer(buf));

			boost::asio::read(m_socket, boost::asio::buffer(buf));

			SS3R(buf);
		}
	}
	catch (...)
	{
		// mark ourselves as disconnected
		m_hsInProgress = false;

		throw;
	}
}

void BLESocket::handshake(ErrorCode_t& ec) noexcept
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

	if (m_socket.is_open() == false)
	{
		ec = boost::system::errc::make_error_code(boost::system::errc::not_connected);
		return;
	}

	try
	{
		if (m_client == true)
		{
			// client handshake

			// write magic header	magic words				enc
			std::vector<char> buf(sizeof(uint32_t) * 2 + sizeof(char) * 3);

			CS1(buf);

			boost::asio::write(m_socket, boost::asio::buffer(buf), ec);

			if (ec != boost::system::errc::success)
				return;

			CS1W(buf);

			boost::asio::read(m_socket, boost::asio::buffer(buf), ec);

			if (ec != boost::system::errc::success)
				return;

			CS2(buf);

			boost::asio::write(m_socket, boost::asio::buffer(buf), ec);

			if (ec != boost::system::errc::success)
				return;

			CS3(buf);

			boost::asio::read(m_socket, boost::asio::buffer(buf), ec);

			if (ec != boost::system::errc::success)
				return;

			CS3R(buf);

			boost::asio::write(m_socket, boost::asio::buffer(buf), ec);

			if (ec != boost::system::errc::success)
				return;

			CS3W();
		}
		else
		{
			// server handshake
			std::vector<char> buf(sizeof(uint32_t) * 2 + sizeof(char) * 3);

			SS1();

			boost::asio::read(m_socket, boost::asio::buffer(buf), ec);

			if (ec != boost::system::errc::success)
				return;

			SS1R(buf);

			boost::asio::write(m_socket, boost::asio::buffer(buf), ec);

			if (ec != boost::system::errc::success)
				return;

			SS2(buf);

			boost::asio::read(m_socket, boost::asio::buffer(buf), ec);

			if (ec != boost::system::errc::success)
				return;

			SS3(buf);

			boost::asio::write(m_socket, boost::asio::buffer(buf), ec);

			if (ec != boost::system::errc::success)
				return;

			boost::asio::read(m_socket, boost::asio::buffer(buf), ec);

			if (ec != boost::system::errc::success)
				return;

			SS3R(buf);
		}
	}
	catch (...)
	{
		// mark ourselves as disconnected
		m_hsInProgress = false;

		std::abort();
	}
}

void BLESocket::async_handshake(const HandshakeCallback_t& callback) noexcept
{
	boost::asio::post(m_socket.get_io_context(), boost::bind(&BLESocket::AsyncSS1, this, callback));
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
	// reset the handshake status
	m_handshake = false;
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

	boost::asio::async_read(m_socket, 
		boost::asio::buffer(*buf), 
		boost::bind(
			&BLESocket::AsyncSS1R, this, callback, buf, 
			boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
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
		throw boost::system::errc::illegal_byte_sequence;

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

void BLESocket::AsyncSS1R(const HandshakeCallback_t& callback, std::vector<char>* buf, const ErrorCode_t& ec, const size_t bytesTransferred) noexcept
{
	UNUSED(bytesTransferred);

	// check for errors
	if (ec != boost::system::errc::success)
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
	catch (const ErrorCode_t& e)
	{
		delete buf;
		m_hsInProgress = false;
		callback(e);
		return;
	}

	boost::asio::async_write(m_socket,
		boost::asio::buffer(*buf), 
		boost::bind(
			&BLESocket::AsyncSS2, this, callback, buf, 
			boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
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

void BLESocket::AsyncSS2(const HandshakeCallback_t& callback, std::vector<char>* buf, const ErrorCode_t& ec, const size_t bytesTransferred) noexcept
{
	UNUSED(bytesTransferred);

	if (ec != boost::system::errc::success)
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
	catch (const ErrorCode_t& e)
	{
		delete buf;
		m_hsInProgress = false;
		callback(e);
		return;
	}

	boost::asio::async_read(m_socket,
		boost::asio::buffer(*buf),
		boost::bind(
			&BLESocket::AsyncSS3, this, callback, buf,
			boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
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
		throw boost::system::errc::bad_message;

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

void BLESocket::AsyncSS3(const HandshakeCallback_t& callback, std::vector<char>* buf, const ErrorCode_t& ec, const size_t bytesTransferred) noexcept
{
	UNUSED(bytesTransferred);

	if (ec != boost::system::errc::success)
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
	catch (const ErrorCode_t& e)
	{
		delete buf;
		m_hsInProgress = false;
		callback(e);
		return;
	}

	boost::asio::async_write(m_socket,
		boost::asio::buffer(*buf), 
		boost::bind(
			&BLESocket::AsyncSS3W, this, callback, buf, 
			boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
}

void BLESocket::AsyncSS3W(const HandshakeCallback_t& callback, std::vector<char>* buf, const ErrorCode_t& ec, const size_t bytesTransferred) noexcept
{
	UNUSED(bytesTransferred);

	if (ec != boost::system::errc::success)
	{
		delete buf;
		m_hsInProgress = false;
		callback(ec);
		return;
	}

	boost::asio::async_read(m_socket,
		boost::asio::buffer(*buf),
		boost::bind(
			&BLESocket::AsyncSS3R, this, callback, buf,
			boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
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
		throw boost::system::errc::bad_message;

	// handshake is complete
	m_handshake = true;
	m_hsInProgress = false;
}

void BLESocket::AsyncSS3R(const HandshakeCallback_t& callback, std::vector<char>* buf, const ErrorCode_t& ec, const size_t bytesTransferred) noexcept
{
	UNUSED(bytesTransferred);

	if (ec != boost::system::errc::success)
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
	catch (const ErrorCode_t& e)
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

	boost::asio::async_write(m_socket,
		boost::asio::buffer(*buf),
		boost::bind(
			&BLESocket::AsyncCS1W, this, callback, buf,
			boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
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

void BLESocket::AsyncCS1W(const HandshakeCallback_t& callback, std::vector<char>* buf, const ErrorCode_t& ec, const size_t bytesTransferred) noexcept
{
	UNUSED(bytesTransferred);

	if (ec != boost::system::errc::success)
	{
		delete buf;
		callback(ec);
		return;
	}

	CS1W(*buf);

	boost::asio::async_read(m_socket,
		boost::asio::buffer(*buf),
		boost::bind(
			&BLESocket::AsyncCS2, this, callback, buf,
			boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
}

void BLESocket::CS2(std::vector<char>& buf)
{
	// bad response
	if (CheckMagicNumbers(buf) == false)
		throw boost::system::errc::bad_message;

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
			throw boost::system::errc::identifier_removed;
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

void BLESocket::AsyncCS2(const HandshakeCallback_t& callback, std::vector<char>* buf, const ErrorCode_t& ec, const size_t bytesTransferred) noexcept
{
	UNUSED(bytesTransferred);

	if (ec != boost::system::errc::success)
	{
		delete buf;
		callback(ec);
		return;
	}

	try
	{
		CS2(*buf);
	}
	catch (const ErrorCode_t& e)
	{
		delete buf;
		m_hsInProgress = false;
		callback(e);
		return;
	}

	boost::asio::async_write(m_socket,
		boost::asio::buffer(*buf),
		boost::bind(
			&BLESocket::AsyncCS3, this, callback, buf,
			boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
}

void BLESocket::CS3(std::vector<char>& buf) noexcept
{
	buf.resize(m_aes.IVSIZE + m_aes.TAGSIZE + sizeof(uint32_t) * 2 + 16);
}

void BLESocket::AsyncCS3(const HandshakeCallback_t& callback, std::vector<char>* buf, const ErrorCode_t& ec, const size_t bytesTransferred) noexcept
{
	UNUSED(bytesTransferred);

	if (ec != boost::system::errc::success)
	{
		delete buf;
		callback(ec);
		return;
	}

	CS3(*buf);

	boost::asio::async_read(m_socket,
		boost::asio::buffer(*buf),
		boost::bind(
			&BLESocket::AsyncCS3R, this, callback, buf,
			boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
}

void BLESocket::CS3R(std::vector<char>& buf)
{
	buf = m_aes.Decrypt(m_key, buf);

	// bad response
	if (CheckMagicNumbers(buf) == false)
		throw boost::system::errc::bad_message;

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

void BLESocket::AsyncCS3R(const HandshakeCallback_t& callback, std::vector<char>* buf, const ErrorCode_t& ec, const size_t bytesTransferred) noexcept
{
	UNUSED(bytesTransferred);

	if (ec != boost::system::errc::success)
	{
		delete buf;
		callback(ec);
		return;
	}

	try
	{
		CS3R(*buf);
	}
	catch (const ErrorCode_t& e)
	{
		delete buf;
		m_hsInProgress = false;
		callback(e);
		return;
	}

	boost::asio::async_write(m_socket,
		boost::asio::buffer(*buf),
		boost::bind(
			&BLESocket::AsyncCS3W, this, callback, buf,
			boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
}

void BLESocket::CS3W() noexcept
{
	// the handshake is complete
	m_handshake = true;
	m_hsInProgress = false;
}

void BLESocket::AsyncCS3W(const HandshakeCallback_t& callback, std::vector<char>* buf, const ErrorCode_t& ec, const size_t bytesTransferred) noexcept
{
	UNUSED(bytesTransferred);

	if (ec != boost::system::errc::success)
	{
		delete buf;
		callback(ec);
		return;
	}

	CS3W();

	delete buf;
	callback(ec);
}