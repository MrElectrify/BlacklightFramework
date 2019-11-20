#include "NetworkingTests.h"

constexpr size_t UDP_MAX = 0xFFE0;

int main()
{
	constexpr size_t PACKET_COUNT = 0x1000;
	constexpr size_t PACKET_SIZE = 0x1000;

	if (Networking::RunTCPTests(PACKET_COUNT, PACKET_SIZE) == false)
		return 1;

	// make sure we are not sending too large of datagrams
	if (PACKET_SIZE <= UDP_MAX &&
		Networking::RunUDPTests(PACKET_COUNT, PACKET_SIZE) == false)
		return 2;

	if (Networking::RunEncryptedTCPTests(PACKET_COUNT, PACKET_SIZE) == false)
		return 3;

	return 0;
}