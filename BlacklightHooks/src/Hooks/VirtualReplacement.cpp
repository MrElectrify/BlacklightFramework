#include <Hooks/VirtualReplacement.h>

#include <cstddef>

using Blacklight::Hooks::impl::VirtualReplacementImpl;

// VirtualReplacementImpl

VirtualReplacementImpl::VirtualReplacementImpl(ClassInstance_t pInstance, Index_t index, FunctionImpl_t pHookFn, bool enabled)
	: m_pInstance(pInstance), m_hooked(false)
{
	m_pOrigVtbl = *reinterpret_cast<VTable_t*> (pInstance);
	m_pOrigFn = m_pOrigVtbl[index];

	// get size of the new vtable that we need to create
	size_t size = 0;
	while (m_pOrigVtbl[size])
		++size;

	// allocate space for the RTTI
	m_pHookVtbl = new FunctionImpl_t[size + 1];
	m_pHookVtbl[0] = m_pOrigVtbl[-1];

	++m_pHookVtbl;

	// copy contents of old vtable
	for (size_t i = 0; i < size; ++i)
		m_pHookVtbl[i] = m_pOrigVtbl[i];

	m_pHookVtbl[index] = pHookFn;

	if (enabled)
		Enable();
}

bool VirtualReplacementImpl::Enable()
{
	if (m_hooked)
		return false;

	*reinterpret_cast<VTable_t*>(m_pInstance) = m_pHookVtbl;

	m_hooked = true;

	return true;
}

bool VirtualReplacementImpl::Disable()
{
	if (!m_hooked)
		return false;

	*reinterpret_cast<VTable_t*>(m_pInstance) = m_pOrigVtbl;

	m_hooked = false;

	return true;
}

VirtualReplacementImpl::FunctionImpl_t VirtualReplacementImpl::GetOriginal()
{
	return m_pOrigFn;
}

VirtualReplacementImpl::~VirtualReplacementImpl()
{
	if (m_hooked)
		Disable();

	delete[](m_pHookVtbl - 1);
}