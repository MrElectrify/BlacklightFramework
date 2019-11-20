#ifndef BLACKLIGHT_INSTRUCTION_H_
#define BLACKLIGHT_INSTRUCTION_H_

/*
Instruction
5/27/19 22:55
*/

#include <cstdint>

namespace Blacklight
{
	namespace VM
	{
		class CPU;

		// Instruction wrapper
		class Instruction
		{
		public:
			using Handler_t = void(*)(CPU* pCPU, const uint8_t opcode);

			Instruction(Handler_t handler);

			void operator()(CPU* pCPU, const uint8_t opcode);
		private:
			Handler_t m_handler;
		};
	}
}

#endif