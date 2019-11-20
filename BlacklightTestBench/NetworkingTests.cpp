#include "NetworkingTests.h"

#include <Networking/Acceptor.h>
#include <Networking/Endpoint.h>
#include <Networking/BLE/BLESocket.h>

#include <future>
#include <iostream>
#include <mutex>

using Blacklight::Networking::Endpoint;
using Blacklight::Networking::ErrorCode;
using Blacklight::Networking::BLE::BLESocket;
using Blacklight::Networking::BLE::Context;

using boost::asio::ip::tcp;
using boost::asio::ip::udp;
using boost::asio::io_context;
using boost::system::error_code;

class SyncedStream
{
public:
	SyncedStream(std::ostream& os) : m_os(os) {}

	template<typename T>
	SyncedStream& operator<<(const T& data)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_os << data;
		return *this;
	}
private:
	std::mutex m_mutex;
	std::ostream& m_os;
};

bool Networking::RunTCPTests(const size_t count, const size_t size)
{
	SyncedStream ss(std::cout);

	ss << "Beginning TCP Networking Tests\n";

	float elapsed;

	auto serverFuture = std::async(std::launch::async, [&]()
	{
		io_context worker;
		tcp::acceptor acceptor(worker, tcp::endpoint(tcp::v4(), 1723));

		tcp::socket client(worker);

		error_code ec;
		acceptor.accept(client, ec);

		// we failed to accept the client
		if (ec != boost::system::errc::success)
		{
			ss << "Server failed at accept: " << ec.message() << '\n';
			return false;
		}

		std::vector<char> buf(size);

		auto begin = std::chrono::system_clock::now();

		for (size_t i = 0; i < count; ++i)
		{
			auto read = boost::asio::read(client, boost::asio::buffer(buf), ec);

			if (ec != boost::system::errc::success)
			{
				ss << "Server failed at read: " << ec.message() << '\n';
				return false;
			}

			if (read != size)
			{
				ss << "Server read size mismatch: " << size << '\n';
				return false;
			}

			auto sent = boost::asio::write(client, boost::asio::buffer(buf), ec);

			if (ec != boost::system::errc::success)
			{
				ss << "Server failed at write: " << ec.message() << '\n';
				return false;
			}

			if (sent != size)
			{
				ss << "Server send size mismatch\n";
				return false;
			}
		}

		auto end = std::chrono::system_clock::now();

		ss << "Server completed " << count << " packets\n";

		elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() / 1000.f;

		client.shutdown(client.shutdown_both);
		client.close();

		return true;
	});

	auto clientFuture = std::async(std::launch::async, [&]()
	{
		io_context worker;

		Context context;

		tcp::socket client(worker);

		tcp::resolver r(worker);
		tcp::endpoint e(boost::asio::ip::make_address_v4("127.0.0.1"), 1723);

		error_code ec;
		client.connect(e, ec);

		if (ec != boost::system::errc::success)
		{
			ss << "Test failed at connect: " << ec.message() << '\n';
			return false;
		}

		std::vector<char> buf(size);

		for (size_t i = 0; i < count; ++i)
		{
			CryptoPP::AutoSeededRandomPool rng;
			rng.GenerateBlock(reinterpret_cast<CryptoPP::byte*>(&buf[0]), size);

			auto sent = boost::asio::write(client, boost::asio::buffer(buf), ec);

			if (ec != boost::system::errc::success)
			{
				ss << "Client failed at write: " << ec.message() << '\n';
				return false;
			}

			if (sent != size)
			{
				ss << "Client send size mismatch\n";
				return false;
			}

			std::vector<char> tmp(size);

			auto read = boost::asio::read(client, boost::asio::buffer(tmp), ec);

			if (ec != boost::system::errc::success)
			{
				ss << "Server failed at read: " << ec.message() << '\n';
				return false;
			}

			if (read != size)
			{
				ss << "Server read size mismatch\n";
				return false;
			}

			if (tmp != buf)
			{
				ss << "Data mismatch\n";
				return false;
			}
		}

		ss << "Client completed " << count << " packets\n";

		client.shutdown(client.shutdown_both);
		client.close();

		return true;
	});

	while (true)
	{
		serverFuture.wait();
		if (serverFuture.get() == false)
		{
			clientFuture.get();
			return false;
		}

		clientFuture.wait();
		if (clientFuture.get() == false)
			return false;
		else
			break;
	}

	ss << "Operated on " << count * size * 2 / 1000000.f << " MB in " << elapsed << " seconds (" << count * size * 2 / elapsed / 1000000.f << " MB/s)\n";

	ss << "Completed TCP Networking Tests\n";

	return true;
}

bool Networking::RunUDPTests(const size_t count, const size_t size)
{
	SyncedStream ss(std::cout);

	ss << "Beginning UDP Networking Tests\n";

	float elapsed;

	auto serverFuture = std::async(std::launch::async, [&]()
	{
		io_context worker;

		udp::socket client(worker, udp::endpoint(udp::v4(), 1723));

		std::vector<char> buf(size);

		udp::endpoint endpoint;

		auto begin = std::chrono::system_clock::now();

		for (size_t i = 0; i < count; ++i)
		{
			auto read = client.receive_from(boost::asio::buffer(buf), endpoint);

			//ss << "Received " << read << " from " << endpoint.address() << ':' << endpoint.port() << '\n';

			if (read != size)
			{
				ss << "Server read size mismatch: " << size << '\n';
				return false;
			}

			auto sent = client.send_to(boost::asio::buffer(buf), endpoint);

			if (sent != size)
			{
				ss << "Server send size mismatch\n";
				return false;
			}
		}

		auto end = std::chrono::system_clock::now();

		ss << "Server completed " << count << " packets\n";

		elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() / 1000.f;

		client.shutdown(client.shutdown_both);
		client.close();

		return true;
	});

	auto client = [&]()
	{
		io_context worker;

		Context context;

		udp::socket client(worker);
		client.open(udp::v4());

		error_code ec;

		udp::endpoint e(boost::asio::ip::make_address_v4("127.0.0.1"), 1723);

		if (ec != boost::system::errc::success)
		{
			ss << "Test failed to resolve: " << ec.message() << '\n';
			return false;
		}

		std::vector<char> buf(size);

		auto sent = client.send_to(boost::asio::buffer(buf), e);

		ss << "Opened socket at " << client.local_endpoint() << '\n';

		for (size_t i = 0; i < count / 2; ++i)
		{
			CryptoPP::AutoSeededRandomPool rng;
			rng.GenerateBlock(reinterpret_cast<CryptoPP::byte*>(&buf[0]), size);

			sent = client.send_to(boost::asio::buffer(buf), e);

			if (sent != size)
			{
				ss << "Client send size mismatch\n";
				return false;
			}

			std::vector<char> tmp(size);

			auto read = client.receive_from(boost::asio::buffer(tmp), e);

			if (read != size)
			{
				ss << "Server read size mismatch\n";
				return false;
			}

			if (tmp != buf)
			{
				ss << "Data mismatch\n";
				return false;
			}
		}

		ss << "Client completed " << count << " packets\n";

		client.shutdown(client.shutdown_both);
		client.close();

		return true;
	};

	auto clientFuture = std::async(std::launch::async, client);
	auto clientFuture2 = std::async(std::launch::async, client);

	while (true)
	{
		serverFuture.wait();
		if (serverFuture.get() == false)
		{
			clientFuture.get();
			return false;
		}

		clientFuture.wait();
		if (clientFuture.get() == false)
			return false;
		else
			break;

		clientFuture2.wait();
	}

	ss << "Operated on " << count * size * 2 / 1000000.f << " MB in " << elapsed << " seconds (" << count * size * 2 / elapsed / 1000000.f << " MB/s)\n";

	ss << "Completed UDP Networking Tests\n";

	return true;
}

bool Networking::RunEncryptedTCPTests(const size_t count, const size_t size)
{
	SyncedStream ss(std::cout);

	ss << "Beginning Encrypted Networking Tests\n";

	float elapsed;

	auto serverFuture = std::async(std::launch::async, [&]()
	{
		io_context worker;
		tcp::acceptor acceptor(worker, tcp::endpoint(tcp::v4(), 1723));

		Context context;

		BLESocket client(context, worker);

		error_code ec;
		acceptor.accept(client.raw_socket(), ec);

		// we failed to accept the client
		if (ec != boost::system::errc::success)
		{
			ss << "Server failed at accept: " << ec.message() << '\n';
			return false;
		}

		client.handshake(ec);

		// handshake failed
		if (ec != boost::system::errc::success)
		{
			ss << "Server failed at handshake: " << ec.message() << '\n';
			return false;
		}

		std::vector<char> buf(size);

		auto begin = std::chrono::system_clock::now();

		for (size_t i = 0; i < count; ++i)
		{
			auto read = boost::asio::read(client, boost::asio::buffer(buf), ec);

			if (ec != boost::system::errc::success)
			{
				ss << "Server failed at read: " << ec.message() << '\n';
				return false;
			}

			if (read != size)
			{
				ss << "Server read size mismatch\n";
				return false;
			}

			auto sent = boost::asio::write(client, boost::asio::buffer(buf), ec);

			if (ec != boost::system::errc::success)
			{
				ss << "Server failed at write: " << ec.message() << '\n';
				return false;
			}

			if (sent != size)
			{
				ss << "Server send size mismatch\n";
				return false;
			}
		}

		auto end = std::chrono::system_clock::now();

		ss << "Server completed " << count << " packets\n";

		elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() / 1000.f;

		client.stop();

		return true;
	});

	auto clientFuture = std::async(std::launch::async, [&]()
	{
		io_context worker;
		
		Context context;

		BLESocket client(context, worker);

		tcp::resolver r(worker);
		tcp::endpoint e(boost::asio::ip::make_address_v4("127.0.0.1"), 1723);

		error_code ec;
		client.connect(e, ec);

		if (ec != boost::system::errc::success)
		{
			ss << "Test failed at connect: " << ec.message() << '\n';
			return false;
		}

		client.handshake(ec);

		if (ec != boost::system::errc::success)
		{
			ss << "Test failed at handshake: " << ec.message() << '\n';
			return false;
		}

		std::vector<char> buf(size);

		for (size_t i = 0; i < count; ++i)
		{
			CryptoPP::AutoSeededRandomPool rng;
			rng.GenerateBlock(reinterpret_cast<CryptoPP::byte*>(&buf[0]), size);

			auto sent = boost::asio::write(client, boost::asio::buffer(buf), ec);

			if (ec != boost::system::errc::success)
			{
				ss << "Client failed at write: " << ec.message() << '\n';
				return false;
			}

			if (sent != size)
			{
				ss << "Client send size mismatch\n";
				return false;
			}

			std::vector<char> tmp(size);

			auto read = boost::asio::read(client, boost::asio::buffer(tmp), ec);

			if (ec != boost::system::errc::success)
			{
				ss << "Server failed at read: " << ec.message() << '\n';
				return false;
			}

			if (read != size)
			{
				ss << "Server read size mismatch\n";
				return false;
			}

			if (tmp != buf)
			{
				ss << "Data mismatch\n";
				return false;
			}
		}

		ss << "Client completed " << count << " packets\n";

		client.stop();

		return true;
	});

	while (true)
	{
		serverFuture.wait();
		if (serverFuture.get() == false)
		{
			clientFuture.get();
			return false;
		}

		clientFuture.wait();
		if (clientFuture.get() == false)
			return false;
		else
			break;
	}

	ss << "Operated on " << count * size * 2 / 1000000.f << " MB in " << elapsed << " seconds (" << count * size * 2 / elapsed / 1000000.f << " MB/s)\n";

	ss << "Completed Encrypted Networking Tests\n";

	return true;
}