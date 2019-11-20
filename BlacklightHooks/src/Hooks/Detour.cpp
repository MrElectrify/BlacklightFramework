#include <Hooks/Detour.h>
#include <Zydis/Zydis.h>

using Blacklight::Hooks::impl::DetourImpl;

#ifdef _MSC_VER
Blacklight::Memory::ProtectedPoolAllocator<Blacklight::Hooks::impl::DetourImpl::TrampolineFunc_t> Blacklight::Hooks::impl::g_hookBlock(
	MAX_CONCURRENT_HOOKS, PAGE_EXECUTE_READWRITE, Blacklight::Memory::PoolAllocatorFlags_NOCONSTRUCT);
#elif __linux__
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

Blacklight::Memory::ProtectedPoolAllocator<Blacklight::Hooks::impl::DetourImpl::TrampolineFunc_t> Blacklight::Hooks::impl::g_hookBlock(
	MAX_CONCURRENT_HOOKS, PROT_READ | PROT_WRITE | PROT_EXEC, Blacklight::Memory::PoolAllocatorFlags_NOCONSTRUCT);
#endif

// DetourImpl

DetourImpl::DetourImpl(FunctionImpl_t pOrigFn, FunctionImpl_t pHookFn, bool enable) :
	m_pHookFn(pHookFn), m_pOrigFn(pOrigFn), m_hooked(false)
{
	// first determine how many bytes we need to copy to our trampoline
	ZydisDecoder decoder;

#ifdef BLACKLIGHT_64BIT
	ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_ADDRESS_WIDTH_64);
#else
	ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LEGACY_32, ZYDIS_ADDRESS_WIDTH_32);
#endif

	size_t offset = 0;
	size_t sjOff = 0;

	ZydisDecodedInstruction inst;

	m_trampolineFunction = g_hookBlock.allocate();

	if (!m_trampolineFunction)
		throw std::runtime_error("Failed to allocate memory for trampoline");

	auto functionPart = reinterpret_cast<FunctionPart_t*>(pOrigFn);

	// save original function data
	memcpy(m_origData, functionPart, MAX_TRAMPOLINE_LENGTH);

	// initialize to breakpoint in case of issue
	memset(m_trampolineFunction->m_data, '\xCC', 16 + MAX_TRAMPOLINE_LENGTH * 2);

	auto CopyFunctionData = [this, functionPart, &offset, &inst, &sjOff]() -> bool
	{
		// fix relative pointers
		if (inst.operands[0].imm.isRelative)
		{
			auto newOffset = reinterpret_cast<ptrdiff_t>(functionPart + inst.operands[0].imm.value.u) - reinterpret_cast<ptrdiff_t>(m_trampolineFunction->m_data + sjOff);

#ifdef BLACKLIGHT_64BIT
			if (abs(newOffset) > 0x7FFFFFFF)
				throw std::runtime_error("Difference too large");
#endif

			// account for new bytes
			if (inst.opcode >= 0x70 &&
				inst.opcode <= 0x7F)
				newOffset -= 4;

			// change short jump to near jump
			*reinterpret_cast<FunctionPart_t*>(m_trampolineFunction->m_data + offset + sjOff) = 0x0F;
			*reinterpret_cast<FunctionPart_t*>(m_trampolineFunction->m_data + offset + sjOff + 1) = inst.opcode + 0x10;
			*reinterpret_cast<uintptr_t*>(m_trampolineFunction->m_data + offset + sjOff + 2) = newOffset;

			if (inst.opcode >= 0x70 &&
				inst.opcode <= 0x7F)
				sjOff += 4;
		}
		else
			memcpy(m_trampolineFunction->m_data + offset + sjOff, inst.data, inst.length);

		offset += inst.length;

		// break if ret
		if (inst.opcode == 0xC3)
			return false;

		return true;
	};

	while (ZydisDecoderDecodeBuffer(&decoder, functionPart + offset, -1, offset, &inst) == ZYDIS_STATUS_SUCCESS &&
		offset < 5 &&
		inst.opcode != 0xCC)		// padding, break
	{
		if (!CopyFunctionData())
			break;
	}

	if (offset < 5)
		throw std::runtime_error("Function is too small for detours");

	auto diff = reinterpret_cast<ptrdiff_t>(functionPart) - reinterpret_cast<ptrdiff_t>(m_trampolineFunction->m_data + sjOff + 5);

#ifdef BLACKLIGHT_64BIT
	// we need to jmp a different way - with push and ret
	if (abs(diff) > 0x7FFFFFFF)
	{
		while (ZydisDecoderDecodeBuffer(&decoder, functionPart + offset, -1, offset, &inst) == ZYDIS_STATUS_SUCCESS &&
			offset < MAX_TRAMPOLINE_LENGTH &&
			inst.opcode != 0xCC)		// padding, break
		{
			if (!CopyFunctionData())
				break;
		}

		if (offset < MAX_TRAMPOLINE_LENGTH)
			throw std::runtime_error("Function is too small for detours");

		// absolute jmp with ret
		uint32_t lo = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(functionPart + offset) & 0x000000000FFFFFFFF);
		uint32_t hi = static_cast<uint32_t>((reinterpret_cast<uintptr_t>(functionPart + offset) & 0xFFFFFFFF00000000) >> 32);

		auto base = m_trampolineFunction->m_data + offset + sjOff;

		// push
		base[0] = 0x68;
		*reinterpret_cast<uint32_t*>(base + 1) = lo;
		// we must mov the hiword onto the stack
		if (hi)
		{
			// mov DWORD PTR [rsp + 0x4]
			*reinterpret_cast<uint32_t*>(base + 5) = 0x042444c7;
			//, hi
			*reinterpret_cast<uint32_t*>(base + 9) = hi;
		}
		// ret
		*reinterpret_cast<FunctionPart_t*>(base + ((hi) ? 13 : 5)) = 0xc3;
	}
	else
	{
#endif
		// near jmp in our trampoline back to the original function
		m_trampolineFunction->m_data[offset + sjOff] = 0xE9;

		*reinterpret_cast<uintptr_t*>(&m_trampolineFunction->m_data[offset + sjOff + 1]) = diff;
#ifdef BLACKLIGHT_64BIT
	}
#endif

	m_functionLength = offset;

	if (enable)
		Enable();
}

bool DetourImpl::Enable()
{
	if (m_hooked)
		return false;

	auto jmpDiff = reinterpret_cast<ptrdiff_t>(m_pHookFn) - reinterpret_cast<ptrdiff_t>(m_pOrigFn) - 5;

#ifdef _MSC_VER
	DWORD dwOriginalProt;
	VirtualProtect(m_pOrigFn, MAX_TRAMPOLINE_LENGTH, PAGE_EXECUTE_READWRITE, &dwOriginalProt);
#elif __linux__
	auto baseAddr = reinterpret_cast<uintptr_t>(m_pOrigFn) & ~(getpagesize() - 1);

	if (mprotect(reinterpret_cast<void*>(baseAddr), getpagesize(), PROT_READ | PROT_WRITE | PROT_EXEC))
		throw std::runtime_error(strerror(errno));
#endif

#ifdef BLACKLIGHT_64BIT
	// relative jump only works 32-bit
	if (abs(jmpDiff) > 0x7FFFFFFF)
	{
		if (m_functionLength < MAX_TRAMPOLINE_LENGTH)
			throw std::runtime_error("Function too small");

		// absolute jmp
		*reinterpret_cast<uint16_t*>(m_pOrigFn) = 0xB848;
		*reinterpret_cast<FunctionImpl_t*>(reinterpret_cast<uintptr_t>(m_pOrigFn) + 2) = m_pHookFn;
		*reinterpret_cast<uint16_t*>(reinterpret_cast<uintptr_t>(m_pOrigFn) + 10) = 0xe0ff;
	}
	else
	{
#endif
		// near jmp in our trampoline back to the original function
		*reinterpret_cast<FunctionPart_t*>(m_pOrigFn) = 0xE9;

		*reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(m_pOrigFn) + 1) = jmpDiff;
#ifdef BLACKLIGHT_64BIT
	}
#endif

#ifdef _MSC_VER
	VirtualProtect(m_pOrigFn, MAX_TRAMPOLINE_LENGTH, dwOriginalProt, &dwOriginalProt);
#elif __linux__
	if (mprotect(reinterpret_cast<void*>(baseAddr), getpagesize(), PROT_READ | PROT_EXEC))
		throw std::runtime_error(strerror(errno));
#endif

	m_hooked = true;

	return true;
}

bool DetourImpl::Disable()
{
	if (!m_hooked)
		return false;

#ifdef _MSC_VER
	DWORD dwOriginalProt;
	VirtualProtect(m_pOrigFn, MAX_TRAMPOLINE_LENGTH, PAGE_EXECUTE_READWRITE, &dwOriginalProt);
#elif __linux__
	auto baseAddr = reinterpret_cast<uintptr_t>(m_pOrigFn) & ~(getpagesize() - 1);

	if (mprotect(reinterpret_cast<void*>(baseAddr), getpagesize(), PROT_READ | PROT_WRITE | PROT_EXEC))
		throw std::runtime_error(strerror(errno));
#endif

	memcpy(reinterpret_cast<FunctionPart_t*>(m_pOrigFn), m_origData, MAX_TRAMPOLINE_LENGTH);

#ifdef _MSC_VER
	VirtualProtect(m_pOrigFn, MAX_TRAMPOLINE_LENGTH, dwOriginalProt, &dwOriginalProt);
#elif __linux__
	if (mprotect(reinterpret_cast<void*>(baseAddr), getpagesize(), PROT_READ | PROT_EXEC))
		throw std::runtime_error(strerror(errno));
#endif

	m_hooked = false;

	return true;
}

DetourImpl::FunctionImpl_t DetourImpl::GetOriginal()
{
	return m_trampolineFunction;
}

DetourImpl::~DetourImpl()
{
	if (m_hooked)
		Disable();

	g_hookBlock.deallocate(m_trampolineFunction);
}