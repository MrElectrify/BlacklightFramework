#include <Memory/MemoryProtectionGuard.h>

using Blacklight::Memory::MemoryProtectionGuard;

#ifdef __linux__
#include <sys/mman.h>
#include <stdio.h>
#include <unistd.h>

// TODO: Actually parse /proc/self/maps
int ReadMProtection(void* addr)
{
	return PROT_READ | PROT_EXEC;
}
#endif

MemoryProtectionGuard::MemoryProtectionGuard(void* pLoc, size_t size, int protection) : m_pLoc(pLoc), m_size(size)
{
#ifdef _MSC_VER
	VirtualProtect(pLoc, size, protection, &m_oldProtection);
#elif __linux__
	auto pageSize = getpagesize();
	auto pageBegin = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(pLoc) & ~(pageSize - 1));

	m_oldProtection = ReadMProtection(pLoc);
	mprotect(pageBegin, pageSize, protection);
#endif
}

MemoryProtectionGuard::~MemoryProtectionGuard()
{
#ifdef _MSC_VER
	VirtualProtect(m_pLoc, m_size, m_oldProtection, &m_oldProtection);
#elif __linux__
	auto pageSize = getpagesize();
	auto pageBegin = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(m_pLoc) & ~(pageSize - 1));
	mprotect(pageBegin, pageSize, m_oldProtection);
#endif
}