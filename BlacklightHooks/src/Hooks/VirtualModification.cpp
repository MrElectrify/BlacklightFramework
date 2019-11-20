#include <Hooks/VirtualModification.h>
#include <Memory/MemoryProtectionGuard.h>

#ifdef BLACKLIGHT_LINUX
#include <sys/mman.h>
#endif

using Blacklight::Hooks::impl::VirtualModificationImpl;

// VirtualModificationImpl

VirtualModificationImpl::VirtualModificationImpl(ClassInstance_t pInstance, Index_t index, FunctionImpl_t pHookFn, bool enabled)
	: m_pHookFn(pHookFn), m_index(index), m_hooked(false)
{
	m_pOrigVtbl = *reinterpret_cast<VTable_t*>(pInstance);
	m_pOrigFn = m_pOrigVtbl[index];

	if (enabled)
		Enable();
}

bool VirtualModificationImpl::Enable()
{
	if (m_hooked)
		return false;

#if _WIN32 || _WIN64
	Memory::MemoryProtectionGuard guard(reinterpret_cast<void*>(m_pOrigVtbl + m_index), sizeof(FunctionImpl_t), PAGE_READWRITE);
#elif __linux__
	Memory::MemoryProtectionGuard guard(reinterpret_cast<void*>(m_pOrigVtbl + m_index), sizeof(FunctionImpl_t), PROT_READ | PROT_WRITE);
#endif

	m_pOrigVtbl[m_index] = m_pHookFn;

	m_hooked = true;

	return true;
}

bool VirtualModificationImpl::Disable()
{
	if (!m_hooked)
		return false;

#if _WIN32 || _WIN64
	Memory::MemoryProtectionGuard guard(reinterpret_cast<void*>(m_pOrigVtbl + m_index), sizeof(FunctionImpl_t), PAGE_READWRITE);
#elif __linux__
	Memory::MemoryProtectionGuard guard(reinterpret_cast<void*>(m_pOrigVtbl + m_index), sizeof(FunctionImpl_t), PROT_READ | PROT_WRITE);
#endif

	m_pOrigVtbl[m_index] = m_pOrigFn;

	m_hooked = false;

	return true;
}

VirtualModificationImpl::FunctionImpl_t VirtualModificationImpl::GetOriginal()
{
	return m_pOrigFn;
}

VirtualModificationImpl::~VirtualModificationImpl()
{
	if (m_hooked)
		Disable();
}