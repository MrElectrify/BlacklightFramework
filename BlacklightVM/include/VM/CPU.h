#ifndef BLACKLIGHT_VM_CPU_H_
#define BLACKLIGHT_VM_CPU_H_

/*
VM CPU
4/30/19 16:51
*/

#include <VM/Arch.h>
#include <VM/MemoryController.h>
#include <VM/Instruction.h>

#include <cstddef>

namespace Blacklight
{
	namespace VM
	{		
		enum Flags
		{
			F_N = (1 << 0),
			F_E = (1 << 1),
			F_P = (1 << 2)
		};

		using Register = uint32_t;

		// Main BL CPU
		class CPU
		{
		public:
			CPU(const size_t blockSize);

			// Returns the CPU's MemoryController
			MemoryController& GetMemoryController();
			// Returns the CPU's registers
			Register& GetRegister(const uint32_t reg);

			// Notifies the CPU that it should finish executing
			void NotifyFinished();

			// Loads an file image into virtual memory
			void LoadImage(const char* path);

			// Loads an image from memory into virtual memory
			void LoadImage(const uint8_t* buf, size_t size);
			
#if !__WIN64 && !__x86_64__
			// Adds an x86 function to the call table, acting as a stack. Call after LoadImage and before Run
			template<typename T>
			void AddFunction(T fn)
			{
				Register& cnd = GetRegister(R_CND);

				uint8_t numFns = (cnd >> 16) & 0xF;

				m_memory.Write32(GetRegister(R_SB) + (numFns * 4), reinterpret_cast<uint32_t>(fn));

				cnd &= 0x0000FFFF;
				cnd |= ((++numFns) << 16);
			}
#endif

			// Runs the image that is loaded
			void Run();
		private:
			bool m_finished;

			Instruction m_instructions[OP_COUNT];
			MemoryController m_memory;
			Register m_registers[R_COUNT];
		};
	}
}

#endif