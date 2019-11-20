#ifndef BLACKLIGHT_INSTRUCTIONGENERATION_H_
#define BLACKLIGHT_INSTRUCTIONGENERATION_H_

/*
Instruction Generation
5/27/19 22:57
*/

#include <VM/Arch.h>
#include <VM/Instruction.h>

namespace Blacklight
{
	namespace VM
	{
		namespace InstructionGeneration
		{
			template<uint8_t instr>
			class InstructionFactory
			{
			protected:
				uint8_t INS = instr;
			};

			class RegisterFactory
			{
			public:
				constexpr const uint8_t operator()(const RegisterE dst = R_A, const RegisterE src = R_A) const
				{
					return src | (dst << 4);
				}
			} constexpr Reg;

			enum Size
			{
				SZ_DWORD = 1 << 0,
				SZ_WORD = 1 << 1,
				SZ_BYTE = 1 << 2
			};

			class LDFactory : public InstructionFactory<0b0000>
			{
			public:
				constexpr LDFactory() {}

				constexpr const uint8_t operator()(const bool imm, const int sz) const
				{
					return (INS << 4) | (imm << 3) | sz;
				}
			} constexpr LD;

			class LDVFactory : public InstructionFactory<0b0001>
			{
			public:
				constexpr LDVFactory() {}

				constexpr const uint8_t operator()(const bool imm, const Size sz = SZ_DWORD) const
				{
					return (INS << 4) | (imm << 3) | sz;
				}
			} constexpr LDV;

			class STFactory : public InstructionFactory<0b0010>
			{
			public:
				constexpr STFactory() {}

				constexpr const uint8_t operator()(const bool imm, const Size sz = SZ_DWORD) const
				{
					return (INS << 4) | (imm << 3) | sz;
				}
			} constexpr ST;

			class PUSHFactory : public InstructionFactory<0b0011>
			{
			public:
				constexpr PUSHFactory() {}

				constexpr const uint8_t operator()(const bool imm) const
				{
					return (INS << 4) | (imm << 3);
				}
			} constexpr PUSH;

			class POPFactory : public InstructionFactory<0b0100>
			{
			public:
				constexpr POPFactory() {}

				constexpr const uint8_t operator()() const
				{
					return INS << 4;
				}
			} constexpr POP;

			class ADDFactory : public InstructionFactory<0b0101>
			{
			public:
				constexpr ADDFactory() {}

				constexpr const uint8_t operator()(const bool imm, const Size sz = SZ_DWORD) const
				{
					return (INS << 4) | (imm << 3) | sz;
				}
			} constexpr ADD;

			class SUBFactory : public InstructionFactory<0b0110>
			{
			public:
				constexpr SUBFactory() {}

				constexpr const uint8_t operator()(const bool imm, const Size sz = SZ_DWORD) const
				{
					return (INS << 4) | (imm << 3) | sz;
				}
			} constexpr SUB;

			class ANDFactory : public InstructionFactory<0b0111>
			{
			public:
				constexpr ANDFactory() {}

				constexpr const uint8_t operator()(const bool imm, const Size sz = SZ_DWORD) const
				{
					return (INS << 4) | (imm << 3) | sz;
				}
			} constexpr AND;

			class NOTFactory : public InstructionFactory<0b1000>
			{
			public:
				constexpr NOTFactory() {}

				constexpr const uint8_t operator()() const
				{
					return INS << 4;
				}
			} constexpr NOT;

			class CMPFactory : public InstructionFactory<0b1001>
			{
			public:
				constexpr CMPFactory() {}

				constexpr const uint8_t operator()(const bool imm, const Size sz = SZ_DWORD) const
				{
					return (INS << 4) | (imm << 3) | sz;
				}
			} constexpr CMP;

			class BRFactory : public InstructionFactory<0b1010>
			{
			public:
				constexpr BRFactory() {}

				constexpr const uint8_t operator()(const bool imm, const int flags) const
				{
					return (INS << 4) | (imm << 3) | flags;
				}
			} constexpr BR;

			class JMPFactory : public InstructionFactory<0b1011>
			{
			public:
				constexpr JMPFactory() {}

				constexpr const uint8_t operator()(const bool imm, const Size sz = SZ_DWORD) const
				{
					return (INS << 4) | (imm << 3) | sz;
				}
			} constexpr JMP;

			class CALLFactory : public InstructionFactory<0b1100>
			{
			public:
				constexpr CALLFactory() {}

				constexpr const uint8_t operator()(const bool imm, const Size sz = SZ_DWORD) const
				{
					return (INS << 4) | (imm << 3) | sz;
				}
			} constexpr CALL;

			class RETFactory : public InstructionFactory<0b1101>
			{
			public:
				constexpr RETFactory() {}

				constexpr const uint8_t operator()() const
				{
					return (INS << 4);
				}
			} constexpr RET;

			enum Convention
			{
				CNV_CDECL = (1 << 0),
				CNV_STDCALL = (1 << 1),
				CNV_FASTCALL = (1 << 2)
			};

			class CXFactory : public InstructionFactory<0b1110>
			{
			public:
				constexpr CXFactory() {}

				constexpr const uint8_t operator()(const Convention cnv = CNV_CDECL) const
				{
					return (INS << 4) | cnv;
				}
			} constexpr CX;

			class TRAPFactory : public InstructionFactory<0b1111>
			{
			public:
				constexpr TRAPFactory() {}

				constexpr const uint8_t operator()() const
				{
					return (INS << 4);
				}
			} constexpr TRAP;
		}
	}
}

#endif