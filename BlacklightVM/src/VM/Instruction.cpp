#include <VM/Instruction.h>

using Blacklight::VM::Instruction;

Instruction::Instruction(Handler_t handler) : m_handler(handler) {}

void Instruction::operator()(CPU* pCPU, const uint8_t opcode)
{
	m_handler(pCPU, opcode);
}