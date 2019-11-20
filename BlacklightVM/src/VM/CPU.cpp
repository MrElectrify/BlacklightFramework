#include <VM/CPU.h>
#include <VM/InstructionGeneration/InstructionGeneration.h>

#include <array>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <streambuf>
#include <vector>

using Blacklight::VM::CPU;
using Blacklight::VM::MemoryController;
using Blacklight::VM::Register;

int CallFastcall(void* fn, uint32_t args[], size_t argc)
{
#if _WIN32 && !_WIN64
	// first 2 in ecx/edx and rest on stack in reverse order. callee cleanup
	int res;
	__asm
	{
		cmp argc, 0
		je END
		push esi
		push edi
		push ebx
		mov esi, argc
		mov edi, args
		mov ecx, [edi]
		dec esi
		cmp esi, 0
		je END
		add edi, 4
		mov edx, [edi]
		dec esi
		cmp esi, 0
		je END
		add edi, 4
	LOOP:
		dec esi
		mov ebx, [edi + esi * 4]
		push ebx
		cmp esi, 0
		jne LOOP
	END:
		call fn
		pop ebx
		pop edi
		pop esi
		mov res, eax
	}
	return res;
#else
	return 0;
#endif
}

int CallStdcall(void* fn, uint32_t args[], size_t argc)
{
#if _WIN32 && !_WIN64
	// args on stack in reverse order. callee cleanup
	int res;
	__asm
	{
		cmp argc, 0
		je END
		mov eax, argc
		mov ecx, args
	LOOP:
		dec eax
		mov edx, [ecx + eax * 4]
		push edx
		cmp eax, 0
		jne LOOP
	END:
		call fn
		mov res, eax
	}
	return res;
#else
	return 0;
#endif
}

int CallCdecl(void* fn, uint32_t args[], size_t argc)
{
#if _WIN32 && !_WIN64
	// args on stack in reverse order. caller cleanup
	int res;
	__asm
	{
		cmp argc, 0
		je END
		mov eax, argc
		mov ecx, args
	LOOP:
		dec eax
		mov edx, [ecx + eax * 4]
		push edx
		cmp eax, 0
		jne LOOP
	END:
		call fn
		mov ecx, argc
		imul ecx, 4
		add esp, ecx
		mov res, eax
	}
	return res;
#else
	return 0;
#endif
}

uint32_t SignExtend(uint32_t val, size_t bytes)
{
	if ((val >> (bytes - 1)) & 0x1) //negative
		return val | (0xFFFFFFFF << bytes);
	else
		return val;
}

CPU::CPU(const size_t blockSize) :
	m_finished(false),
	m_instructions{
		{[](CPU* pCPU, const uint8_t opcode)
{
	MemoryController& mc = pCPU->GetMemoryController();
	Register& prg = pCPU->GetRegister(R_PRG);

	// program counter must be updated by each instruction
	uint8_t inst = opcode & 0xF;
	uint8_t reg = *(mc.GetRawMemory() + prg + 1);

	bool imm = (inst >> 3) & 0x1;
	bool b = (inst >> 2) & 0x1;
	bool w = (inst >> 1) & 0x1;
	bool d = inst & 0x1;

	uint8_t dst = (reg >> 4) & 0xF;
	
	if (imm)
	{		
		int32_t offset = *reinterpret_cast<int32_t*>(mc.GetRawMemory() + prg + 2);

		prg += 6;

		if (b)
			pCPU->GetRegister(dst) = mc.Read8(prg + offset);
		else if (w)
			pCPU->GetRegister(dst) = mc.Read16(prg + offset);
		else if (d)
			pCPU->GetRegister(dst) = mc.Read32(prg + offset);
	}
	else
	{
		prg += 2;

		uint8_t src = reg & 0xF;

		if (b)
			pCPU->GetRegister(dst) = mc.Read8(pCPU->GetRegister(src));
		else if (w)
			pCPU->GetRegister(dst) = mc.Read16(pCPU->GetRegister(src));
		else if (d)
			pCPU->GetRegister(dst) = mc.Read32(pCPU->GetRegister(src));
	}
}},	// OP_LD
		{[](CPU* pCPU, const uint8_t opcode)
{
	MemoryController& mc = pCPU->GetMemoryController();
	Register& prg = pCPU->GetRegister(R_PRG);

	// program counter must be updated by each instruction
	uint8_t inst = opcode & 0xF;
	uint8_t reg = *(mc.GetRawMemory() + prg + 1);

	bool imm = (inst >> 3) & 0x1;
	bool b = (inst >> 2) & 0x1;
	bool w = (inst >> 1) & 0x1;

	uint8_t dst = (reg >> 4) & 0xF;

	if (imm)
	{
		if (b)
		{
			int8_t data = *reinterpret_cast<int8_t*>(mc.GetRawMemory() + prg + 2);
			prg += 3;
			pCPU->GetRegister(dst) = data;
		}
		else if (w)
		{
			int16_t data = *reinterpret_cast<int16_t*>(mc.GetRawMemory() + prg + 2);
			prg += 4;
			pCPU->GetRegister(dst) = data;
		}
		else
		{
			int32_t data = *reinterpret_cast<int32_t*>(mc.GetRawMemory() + prg + 2);
			prg += 6;
			pCPU->GetRegister(dst) = data;
		}
	}
	else
	{
		prg += 2;

		uint8_t src = reg & 0xF;

		if (b)
			pCPU->GetRegister(dst) = static_cast<uint8_t>(pCPU->GetRegister(src));
		else if (w)
			pCPU->GetRegister(dst) = static_cast<uint16_t>(pCPU->GetRegister(src));
		else
			pCPU->GetRegister(dst) = pCPU->GetRegister(src);
	}
}},	// OP_LDV
		{[](CPU* pCPU, const uint8_t opcode)
{
	MemoryController& mc = pCPU->GetMemoryController();
	Register& prg = pCPU->GetRegister(R_PRG);

	// program counter must be updated by each instruction
	uint8_t inst = opcode & 0xF;
	uint8_t reg = *(mc.GetRawMemory() + prg + 1);

	bool imm = (inst >> 3) & 0x1;
	bool b = (inst >> 2) & 0x1;
	bool w = (inst >> 1) & 0x1;

	uint8_t dst = (reg >> 4) & 0xF;
	uint8_t src = reg & 0xF;

	if (imm)
	{
		int32_t offset = *reinterpret_cast<int32_t*>(mc.GetRawMemory() + prg + 2);

		prg += 6;

		if (b)
			mc.Write8(prg + offset, pCPU->GetRegister(src));
		else if (w)
			mc.Write16(prg + offset, pCPU->GetRegister(src));
		else
			mc.Write32(prg + offset, pCPU->GetRegister(src));
	}
	else
	{
		prg += 2;

		if (b)
			mc.Write8(pCPU->GetRegister(dst), pCPU->GetRegister(src));
		else if (w)
			mc.Write16(pCPU->GetRegister(dst), pCPU->GetRegister(src));
		else
			mc.Write32(pCPU->GetRegister(dst), pCPU->GetRegister(src));
	}
}},	// OP_ST
		{[](CPU* pCPU, const uint8_t opcode)
{
	MemoryController& mc = pCPU->GetMemoryController();
	Register& prg = pCPU->GetRegister(R_PRG);
	Register& sf = pCPU->GetRegister(R_SF);

	sf -= sizeof(int);
	
	bool imm = (opcode >> 3) & 0x1;

	if (imm)
	{
		int32_t data = *reinterpret_cast<int32_t*>(mc.GetRawMemory() + prg + 1);
		
		prg += 5;
		
		pCPU->GetMemoryController().Write32(sf, data);
	}
	else
	{
		uint8_t src = *reinterpret_cast<int32_t*>(mc.GetRawMemory() + prg + 1) & 0xF;
		
		prg += 2;

		pCPU->GetMemoryController().Write32(sf, pCPU->GetRegister(src));
	}
}},	// OP_PUSH
		{[](CPU* pCPU, const uint8_t opcode)
{
	MemoryController& mc = pCPU->GetMemoryController();
	Register& prg = pCPU->GetRegister(R_PRG);
	Register& sf = pCPU->GetRegister(R_SF);

	uint8_t reg = *reinterpret_cast<int32_t*>(mc.GetRawMemory() + prg + 1);
	uint8_t dst = (reg >> 4) & 0xF;

	pCPU->GetRegister(dst) = mc.Read32(sf);
	sf += sizeof(int);

	prg += 2;
}},	// OP_POP
		{[](CPU* pCPU, const uint8_t opcode)
{
	MemoryController& mc = pCPU->GetMemoryController();
	Register& prg = pCPU->GetRegister(R_PRG);

	// program counter must be updated by each instruction
	uint8_t inst = opcode & 0xF;
	uint8_t reg = *(mc.GetRawMemory() + prg + 1);

	bool imm = (inst >> 3) & 0x1;
	bool b = (inst >> 2) & 0x1;
	bool w = (inst >> 1) & 0x1;

	uint8_t dst = (reg >> 4) & 0xF;

	if (imm)
	{
		if (b)
		{
			int8_t data = *reinterpret_cast<int8_t*>(mc.GetRawMemory() + prg + 2);
			prg += 3;
			pCPU->GetRegister(dst) += data;
		}
		else if (w)
		{
			int16_t data = *reinterpret_cast<int16_t*>(mc.GetRawMemory() + prg + 2);
			prg += 4;
			pCPU->GetRegister(dst) += data;
		}
		else
		{
			int32_t data = *reinterpret_cast<int32_t*>(mc.GetRawMemory() + prg + 2);
			prg += 6;
			pCPU->GetRegister(dst) += data;
		}
	}
	else
	{
		prg += 2;

		uint8_t src = reg & 0xF;

		if (b)
			pCPU->GetRegister(dst) += static_cast<int8_t>(pCPU->GetRegister(src));
		else if (w)
			pCPU->GetRegister(dst) += static_cast<int16_t>(pCPU->GetRegister(src));
		else
			pCPU->GetRegister(dst) += static_cast<int32_t>(pCPU->GetRegister(src));
	}
}},	// OP_ADD
		{[](CPU* pCPU, const uint8_t opcode)
{
	MemoryController& mc = pCPU->GetMemoryController();
	Register& prg = pCPU->GetRegister(R_PRG);

	// program counter must be updated by each instruction
	uint8_t inst = opcode & 0xF;
	uint8_t reg = *(mc.GetRawMemory() + prg + 1);

	bool imm = (inst >> 3) & 0x1;
	bool b = (inst >> 2) & 0x1;
	bool w = (inst >> 1) & 0x1;

	uint8_t dst = (reg >> 4) & 0xF;

	if (imm)
	{
		if (b)
		{
			int8_t data = *reinterpret_cast<int8_t*>(mc.GetRawMemory() + prg + 2);
			prg += 3;
			pCPU->GetRegister(dst) -= data;
		}
		else if (w)
		{
			int16_t data = *reinterpret_cast<int16_t*>(mc.GetRawMemory() + prg + 2);
			prg += 4;
			pCPU->GetRegister(dst) -= data;
		}
		else
		{
			int32_t data = *reinterpret_cast<int32_t*>(mc.GetRawMemory() + prg + 2);
			prg += 6;
			pCPU->GetRegister(dst) -= data;
		}
	}
	else
	{
		prg += 2;

		uint8_t src = reg & 0xF;

		if (b)
			pCPU->GetRegister(dst) -= static_cast<int8_t>(pCPU->GetRegister(src));
		else if (w)
			pCPU->GetRegister(dst) -= static_cast<int16_t>(pCPU->GetRegister(src));
		else
			pCPU->GetRegister(dst) -= static_cast<int32_t>(pCPU->GetRegister(src));
	}
}},	// OP_SUB
		{[](CPU* pCPU, const uint8_t opcode)
{
	MemoryController& mc = pCPU->GetMemoryController();
	Register& prg = pCPU->GetRegister(R_PRG);

	// program counter must be updated by each instruction
	uint8_t inst = opcode & 0xF;
	uint8_t reg = *(mc.GetRawMemory() + prg + 1);

	bool imm = (inst >> 3) & 0x1;
	bool b = (inst >> 2) & 0x1;
	bool w = (inst >> 1) & 0x1;

	uint8_t dst = (reg >> 4) & 0xF;

	if (imm)
	{
		if (b)
		{
			int8_t data = *reinterpret_cast<int8_t*>(mc.GetRawMemory() + prg + 2);
			prg += 3;
			pCPU->GetRegister(dst) &= data;
		}
		else if (w)
		{
			int16_t data = *reinterpret_cast<int16_t*>(mc.GetRawMemory() + prg + 2);
			prg += 4;
			pCPU->GetRegister(dst) &= data;
		}
		else
		{
			int32_t data = *reinterpret_cast<int32_t*>(mc.GetRawMemory() + prg + 2);
			prg += 6;
			pCPU->GetRegister(dst) &= data;
		}
	}
	else
	{
		prg += 2;

		uint8_t src = reg & 0xF;

		if (b)
			pCPU->GetRegister(dst) &= static_cast<uint8_t>(pCPU->GetRegister(src));
		else if (w)
			pCPU->GetRegister(dst) &= static_cast<uint16_t>(pCPU->GetRegister(src));
		else
			pCPU->GetRegister(dst) &= pCPU->GetRegister(src);
	}
}},	// OP_AND
		{[](CPU* pCPU, const uint8_t opcode)
{
	MemoryController& mc = pCPU->GetMemoryController();
	Register& prg = pCPU->GetRegister(R_PRG);

	prg += 2;

	uint8_t reg = *(mc.GetRawMemory() + prg + 1);
	uint8_t dst = (reg >> 4) & 0xF;

	pCPU->GetRegister(dst) = ~pCPU->GetRegister(dst);
}},	// OP_NOT
		{[](CPU* pCPU, const uint8_t opcode)
{
	MemoryController& mc = pCPU->GetMemoryController();
	Register& cnd = pCPU->GetRegister(R_CND);
	Register& prg = pCPU->GetRegister(R_PRG);

	cnd &= ~(F_P | F_E);

	// program counter must be updated by each instruction
	uint8_t inst = opcode & 0xF;
	uint8_t reg = *(mc.GetRawMemory() + prg + 1);

	bool imm = (inst >> 3) & 0x1;
	bool b = (inst >> 2) & 0x1;
	bool w = (inst >> 1) & 0x1;

	uint8_t dstr = (reg >> 4) & 0xF;
	uint32_t dst = pCPU->GetRegister(dstr);
	uint8_t srcr = reg & 0xF;
	uint32_t src;

	if (imm)
	{
		if (b)
		{
			src = *reinterpret_cast<int8_t*>(mc.GetRawMemory() + prg + 2);
			prg += 3;
		}
		else if (w)
		{
			src = *reinterpret_cast<int16_t*>(mc.GetRawMemory() + prg + 2);
			prg += 4;
		}
		else
		{
			src = *reinterpret_cast<int32_t*>(mc.GetRawMemory() + prg + 2);
			prg += 6;
		}
	}
	else
	{
		prg += 2;
		if (b)
			src = static_cast<uint8_t>(pCPU->GetRegister(srcr));
		else if (w)
			src = static_cast<uint16_t>(pCPU->GetRegister(srcr));
		else
			src = pCPU->GetRegister(srcr);
	}

	if (src > dst)
		cnd |= F_P;
	else if (src == dst)
		cnd |= F_E;
	else
		cnd |= F_N;
}},	// OP_CMP
		{[](CPU* pCPU, const uint8_t opcode)
{
	MemoryController& mc = pCPU->GetMemoryController();
	Register& cnd = pCPU->GetRegister(R_CND);
	Register& prg = pCPU->GetRegister(R_PRG);

	// program counter must be updated by each instruction
	uint8_t inst = opcode & 0xF;

	bool imm = (inst >> 3) & 0x1;
	bool P = (inst >> 2) & 0x1;
	bool E = (inst >> 1) & 0x1;
	bool N = inst & 0x1;

	if ((P && (cnd & F_P)) ||
		(E && (cnd & F_E)) ||
		(N && (cnd & F_N)))
	{
		if (imm)
		{
			int32_t offset = *reinterpret_cast<int32_t*>(mc.GetRawMemory() + prg + 1);

			prg += 5;
			prg += offset;
		}
		else
		{
			uint8_t reg = *(mc.GetRawMemory() + prg + 1);
			bool src = reg & 0xF;

			prg = pCPU->GetRegister(src);
		}
	}
	else
	{
		if (imm)
			prg += 5;
		else
			prg += 2;
	}
}},	// OP_BR
		{[](CPU* pCPU, const uint8_t opcode)
{
	MemoryController& mc = pCPU->GetMemoryController();
	Register& prg = pCPU->GetRegister(R_PRG);

	// program counter must be updated by each instruction
	uint8_t inst = opcode & 0xF;

	bool imm = (inst >> 3) & 0x1;
	bool b = (inst >> 2) & 0x1;
	bool w = (inst >> 1) & 0x1;

	if (imm)
	{
		int32_t offset;
		if (b)
		{
			offset = SignExtend(*reinterpret_cast<int8_t*>(mc.GetRawMemory() + prg + 1), 8);
			prg += 2;
		}
		else if (w)
		{
			offset = SignExtend(*reinterpret_cast<int16_t*>(mc.GetRawMemory() + prg + 1), 16);
			prg += 3;
		}
		else
		{
			offset = *reinterpret_cast<int32_t*>(mc.GetRawMemory() + prg + 1);
			prg += 5;
		}

		prg += offset;
	}
	else
	{
		uint8_t reg = *(mc.GetRawMemory() + prg + 1);
		bool src = reg & 0xF;

		prg = pCPU->GetRegister(src);
	}
}},	// OP_JMP
		{[](CPU* pCPU, const uint8_t opcode)
{
	MemoryController& mc = pCPU->GetMemoryController();
	Register& prg = pCPU->GetRegister(R_PRG);
	Register& sf = pCPU->GetRegister(R_SF);

	sf -= sizeof(int);

	// program counter must be updated by each instruction
	uint8_t inst = opcode & 0xF;

	bool imm = (inst >> 3) & 0x1;
	bool b = (inst >> 2) & 0x1;
	bool w = (inst >> 1) & 0x1;

	if (imm)
	{
		int32_t offset;
		if (b)
		{
			offset = SignExtend(*reinterpret_cast<int8_t*>(mc.GetRawMemory() + prg + 1), 8);
			prg += 2;
		}
		else if (w)
		{
			offset = SignExtend(*reinterpret_cast<int16_t*>(mc.GetRawMemory() + prg + 1), 16);
			prg += 3;
		}
		else
		{
			offset = *reinterpret_cast<int32_t*>(mc.GetRawMemory() + prg + 1);
			prg += 5;
		}

		mc.Write32(sf, prg);
		prg += offset;
	}
	else
	{
		uint8_t reg = *(mc.GetRawMemory() + prg + 1);
		bool src = reg & 0xF;

		prg += 2;

		mc.Write32(sf, prg);
		prg = pCPU->GetRegister(src);
	}
}},	// OP_CALL
		{[](CPU* pCPU, const uint8_t opcode)
{
	MemoryController& mc = pCPU->GetMemoryController();
	Register& prg = pCPU->GetRegister(R_PRG);
	Register& sf = pCPU->GetRegister(R_SF);

	prg = mc.Read32(sf);
	sf += sizeof(int);
}},	// OP_RET
		{[](CPU* pCPU, const uint8_t opcode)
{
	// no support for x64 calls
	if (sizeof(void*) == 8)
		throw std::runtime_error("64-bit function calls are not supported");

	MemoryController& mc = pCPU->GetMemoryController();
	Register& prg = pCPU->GetRegister(R_PRG);

	uint8_t inst = opcode & 0xF;

	bool F = inst & InstructionGeneration::CNV_FASTCALL;
	bool S = inst & InstructionGeneration::CNV_STDCALL;

	uint8_t index = *(mc.GetRawMemory() + prg + 1);

	uint32_t fn = mc.Read32(pCPU->GetRegister(R_SB) + (index * 4));

	uint32_t argc = pCPU->GetRegister(R_L);
	std::array<uint32_t, 11> args;
	for (size_t i = 0; i < argc; ++i)
		args[i] = pCPU->GetRegister(i);

	if (F)
	{
		pCPU->GetRegister(R_A) = CallFastcall(reinterpret_cast<void*>(fn), args.data(), argc);
	}
	else if (S)
	{
		pCPU->GetRegister(R_A) = CallStdcall(reinterpret_cast<void*>(fn), args.data(), argc);
	}
	else
	{
		pCPU->GetRegister(R_A) = CallCdecl(reinterpret_cast<void*>(fn), args.data(), argc);
	}

	prg += 2;
}},	// OP_CX
		{[](CPU* pCPU, const uint8_t opcode)
{
	MemoryController& mc = pCPU->GetMemoryController();
	Register& prg = pCPU->GetRegister(R_PRG);
	
	uint8_t tc = *(mc.GetRawMemory() + prg + 1);

	if (tc == TC_HALT)
		pCPU->NotifyFinished();
}}	// OP_TRAP
	},
	m_memory(blockSize)
{
	enum { PRG_START = 0x2000 };

	m_registers[R_PRG] = PRG_START;
	m_registers[R_SB] = 0x1000;
	m_registers[R_SF] = 0x1000;
	m_registers[R_CND] = 0;
}

MemoryController& CPU::GetMemoryController()
{
	return m_memory;
}

Register& CPU::GetRegister(const uint32_t reg)
{
	return m_registers[reg];
}

void CPU::NotifyFinished()
{
	m_finished = true;
}

void CPU::LoadImage(const char* path)
{
	std::ifstream in(path, std::ios_base::binary);

	std::vector<char> data(
		(std::istreambuf_iterator<char>(in)),
		std::istreambuf_iterator<char>());

	LoadImage(reinterpret_cast<uint8_t*>(data.data()), data.size());
}

void CPU::LoadImage(const uint8_t* buf, size_t size)
{
	uint32_t stackSize = reinterpret_cast<const uint32_t*>(buf)[0];
	uint32_t origin = reinterpret_cast<const uint32_t*>(buf)[1];

	// set stack base and program counter
	m_registers[R_SB] = stackSize;
	m_registers[R_SF] = stackSize;
	m_registers[R_PRG] = origin;

	// copy the memory
	memcpy(m_memory.GetRawMemory() + origin, buf + 8, size - 8);
}

void CPU::Run()
{
	uint8_t* rawMem = m_memory.GetRawMemory();

	while (m_finished == false)
	{
		uint8_t opcode = *(rawMem + m_registers[R_PRG]);
		unsigned char inst = (opcode >> 4) & 0xF;

		m_instructions[inst](this, opcode);
	}
}