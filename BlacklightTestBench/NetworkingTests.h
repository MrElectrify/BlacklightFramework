#ifndef TESTBENCH_NETWORKINGTESTS_H_
#define TESTBENCH_NETWORKINGTESTS_H_

namespace Networking
{
	bool RunTCPTests(const size_t count, const size_t size);
	bool RunUDPTests(const size_t count, const size_t size);
	bool RunEncryptedTCPTests(const size_t count, const size_t size);
}

#endif