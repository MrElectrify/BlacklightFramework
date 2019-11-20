#ifndef BLACKLIGHT_VM_ARCH_H_
#define BLACKLIGHT_VM_ARCH_H_

/*
Architecture Enums
5/27/19 23:06
*/

namespace Blacklight
{
	namespace VM
	{
		enum RegisterE
		{
			R_A,
			R_B,
			R_C,
			R_D,
			R_E,
			R_F,
			R_G,
			R_H,
			R_I,
			R_J,
			R_K,
			R_L,
			R_SF,
			R_SB,
			R_PRG,
			R_CND,
			R_COUNT
		};

		enum OpcodeE
		{
			OP_LD,
			OP_LDV,
			OP_ST,
			OP_PUSH,
			OP_POP,
			OP_ADD,
			OP_SUB,
			OP_AND,
			OP_NOT,
			OP_CMP,
			OP_BR,
			OP_JMP,
			OP_CALL,
			OP_RET,
			OP_CX,
			OP_TRAP,
			OP_COUNT
		};

		enum TrapCode
		{
			TC_PUTC,
			TC_PUTS,
			TC_GETCHAR,
			TC_SOCKET,
			TC_LISTEN,
			TC_BIND,
			TC_ACCEPT,
			TC_SELECT,
			TC_RECV,
			TC_SEND,
			TC_SHUTDOWN,
			TC_HALT
		};

	}
}

#endif