#include <VM/CPU.h>
#include <VM/InstructionGeneration/InstructionGeneration.h>

#include <assert.h>
#include <cmath>
#include <stdio.h>
#include <utility>

#if _WIN32 && !_WIN64
#define FASTCALL __fastcall
#define STDCALL __stdcall
#define CDECL __cdecl
#elif __linux__ && !__x86_64__
#define FASTCALL __attribute__((fastcall))
#define STDCALL __attribute__((stdcall))
#define CDECL __attribute__((cdecl))
#else
#define FASTCALL
#define STDCALL
#define CDECL
#endif

int FASTCALL FastcallTest(int a, int b, int c, int d)
{
	printf("FastcallTest: %d %d %d %d\n", a, b, c, d);
	return a + b + c + d;
}

int STDCALL StdcallTest(int a, int b, int c, int d)
{
	printf("StdcallTest: %d %d %d %d\n", a, b, c, d);
	return a + b + c + d;
}

int CDECL CdeclTest(int a, int b, int c, int d)
{
	printf("CdeclTest: %d %d %d %d\n", a, b, c, d);
	return a + b + c + d;
}

using namespace Blacklight::VM;
using namespace Blacklight::VM::InstructionGeneration;

class Test
{
public:
	void FN()
	{
		printf("Test::FN()\n");
	}
};

int main()
{
	constexpr auto testVal = 0x35465768;
	constexpr uint8_t buf[] =
	{													// R_PRG after execution
		0x10, 0, 0, 0,									// 10 stack size
		0x20, 0, 0, 0,									// 20 entry point
		LDV(true, SZ_BYTE), Reg(R_E), 5,				// 23 Load the byte 5 into the R_E register
		LDV(true, SZ_WORD), Reg(R_F), 0x98,	0x00,		// 26 Load the word 0x0098 (little endian) into the R_F register
		ST(false, SZ_BYTE), Reg(R_F, R_E),				// 29 Store the contents of R_E into the memory location held in R_F (0x0098)
		LD(false, SZ_BYTE), Reg(R_B, R_F),				// 2B Load the contents from the memory location held in R_F into R_B (RB should be 5)
		PUSH(true),										// 2C Push 0x35465768 onto the stack. It must be split up into 8 bit chunks to fit into our array, little endian
			static_cast<uint8_t>(testVal) & 0xFF,		// 2D
			static_cast<uint8_t>(testVal >> 8) & 0xFF,	// 2E
			static_cast<uint8_t>(testVal >> 16) & 0xFF, // 2F
			static_cast<uint8_t>(testVal >> 24) & 0xFF,	// 30
		POP(), Reg(R_A),								// 32 Pop the top of the stack into R_A. R_A will now contain 0x35465768
		ADD(true, SZ_BYTE), Reg(R_A), 217,				// 35 Add 217 to the R_A register
		SUB(true, SZ_BYTE), Reg(R_A), 222,				// 38 Subtract 222 from the R_A register. It should now contain 0x35465763
		AND(true, SZ_BYTE), Reg(R_A), 0x55,				// 3B AND the R_A register with the byte 0x55. I will now contain 0x41 (65)
		NOT(), Reg(R_A),								// 3D NOT the R_A register. It will now contain 0xBD (-66)
		CMP(true, SZ_BYTE), Reg(R_A), 0x65,				// 40 Compare 0x65 with R_A's value. It is smaller (0x41 vs 0xBD) so F_N (negative difference) is set in R_CND. We could perform a conditional branch if we wanted to
		CALL(true, SZ_BYTE), 4,							// 42 Pushes the current program counter (0x42) onto the stack, and jumps 4 bytes ahead (0x46)
		LDV(false, SZ_DWORD), Reg(R_A, R_G),			// 44 Loads the dword value stored in R_G (-13) into R_A
		TRAP(), TC_HALT,								// 46 Finish execution of the virtual machine
		ADD(true, SZ_BYTE), Reg(R_A), 53,				// 49 Add 53 to R_A (-13)
		LDV(false, SZ_DWORD), Reg(R_G, R_A),			// 4B Load the DWORD from R_A (-13) into R_G
#if _WIN32 && !_WIN64
		LDV(true, SZ_BYTE), Reg(R_A), 5,				// 4E Load 5 into R_A (x86 arg 0)
		LDV(true, SZ_BYTE), Reg(R_B), 6,				// 51 Load 6 into R_B (x86 arg 1)
		LDV(true, SZ_BYTE), Reg(R_C), 7,				// 54 Load 7 into R_C (x86 arg 2)
		LDV(true, SZ_BYTE), Reg(R_D), 3,				// 57 Load 3 into R_D (x86 arg 3)
		LDV(true, SZ_BYTE), Reg(R_L), 4,				// 5A Load 4 into R_L (x86 arg count)
		CX(CNV_STDCALL), 0x0,							// 5C call the STDCALL x86 function at index 0 in the x86 call table. In this case, `int __stdcall StdcallTest(int a, int b, int c, int d)`. Result is stored in R_A (21)
		CX(CNV_CDECL), 0x1,								// 5E call the CDECL x86 function at index 1 in the x86 call table. In this case, `int __cdecl CdeclTest(int a, int b, int c, int d)`. Result is stored in R_A (37)
		CX(CNV_FASTCALL), 0x2,							// 60 call the FASTCALL x86 function at index 2 in the x86 call table. In this case, `int __fastcall FastcallTest(int a, int b, int c, int d)`. Result is stored in R_A (53)
		LDV(false, SZ_BYTE), Reg(R_B, R_A),				// 62 Load the byte R_A (53) into R_B
#endif
		RET(),											// 63 Return to the caller (0x42)
	};

	CPU cpu(0x100);
	cpu.LoadImage(buf, sizeof(buf));
#if !_WIN64 && !__x86_64__
	cpu.AddFunction(StdcallTest);
	cpu.AddFunction(CdeclTest);
	cpu.AddFunction(FastcallTest);
#endif
	cpu.Run();

	// final result is stored in R_A
	printf("Final result of assembly (R_A): %d\n", cpu.GetRegister(R_A));

#if !_WIN64 && !__x86_64__
	printf("Final result of x86 calls (R_B): %d\n", cpu.GetRegister(R_B));
#endif

#if _WIN64 || __x86_64__
	printf("Size of bytecode buffer: %llu\n", sizeof(buf) - 8);
#else
	printf("Size of bytecode buffer: %u\n", sizeof(buf) - 8);
#endif

	for (size_t i = 8; i < sizeof(buf); ++i)
	{
		printf("%02X ", reinterpret_cast<const unsigned char*>(buf)[i]);
	}
	printf("\n");

	return 0;
}