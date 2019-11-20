#include <Memory/Allocators.h>

using Blacklight::Memory::impl::PoolAllocatorImpl;
using Blacklight::Memory::impl::ProtectedPoolAllocatorImpl;

// PoolAllocatorImpl
PoolAllocatorImpl::PoolAllocatorImpl(size_t count, size_t elementSize, int flags)
	: m_flags(flags)
{
	if (count == 0)
		throw std::runtime_error("Size must be positive");

	auto alignedSize = (elementSize % sizeof(void*) != 0) ? 
		elementSize + (sizeof(void*) - (elementSize % sizeof(void*))) : 
		elementSize;

	m_pBlock = malloc(count * alignedSize);

	if (!m_pBlock)
		throw std::bad_alloc();

	// initialize the free list
	for (size_t i = 0; i < count - 1; ++i)
	{
		reinterpret_cast<FreeNode*>(reinterpret_cast<uintptr_t>(m_pBlock) + alignedSize * i)->m_pNext = reinterpret_cast<FreeNode*>(reinterpret_cast<uintptr_t>(m_pBlock) + alignedSize * (i + 1));
	}

	// initialize the last node
	reinterpret_cast<FreeNode*>(reinterpret_cast<uintptr_t>(m_pBlock) + alignedSize * (count - 1))->m_pNext = nullptr;

	// set the head to the free list
	m_pHead = reinterpret_cast<FreeNode*>(m_pBlock);
}

void* PoolAllocatorImpl::allocate()
{
	if (!m_pHead)
		return nullptr;

	auto pBlock = reinterpret_cast<void*>(m_pHead);

	m_pHead = m_pHead->m_pNext;

	return pBlock;
}

void PoolAllocatorImpl::deallocate(void* ptr)
{
	reinterpret_cast<FreeNode*>(ptr)->m_pNext = m_pHead;

	m_pHead = reinterpret_cast<FreeNode*>(ptr);
}

PoolAllocatorImpl::~PoolAllocatorImpl()
{
	if (!m_pBlock)
		return;

	free(m_pBlock);
}

// ProtectedPoolAllocatorImpl

ProtectedPoolAllocatorImpl::ProtectedPoolAllocatorImpl(size_t count, size_t elementSize, int protection, int flags)
	: m_flags(flags), m_count(count)
{
	auto alignedSize = (elementSize % sizeof(void*) != 0) ?
		elementSize + (sizeof(void*) - (elementSize % sizeof(void*))) :
		elementSize;

	m_elementSize = alignedSize;

#if _WIN32 || _WIN64
	m_pBlock = VirtualAlloc(nullptr, count * alignedSize, MEM_COMMIT | MEM_RESERVE, protection);
#elif __linux__
	m_pBlock = mmap(nullptr, count * alignedSize, protection, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
#endif

	if (!m_pBlock)
		throw std::bad_alloc();

	// initialize the free list
	for (size_t i = 0; i < count - 1; ++i)
	{
		reinterpret_cast<FreeNode*>(reinterpret_cast<uintptr_t>(m_pBlock) + alignedSize * i)->m_pNext = reinterpret_cast<FreeNode*>(reinterpret_cast<uintptr_t>(m_pBlock) + alignedSize * (i + 1));
	}

	// initialize the last node
	reinterpret_cast<FreeNode*>(reinterpret_cast<uintptr_t>(m_pBlock) + alignedSize * (count - 1))->m_pNext = nullptr;

	// set the head to the free list
	m_pHead = reinterpret_cast<FreeNode*>(m_pBlock);
}

void* ProtectedPoolAllocatorImpl::allocate()
{
	if (!m_pHead)
		return nullptr;

	auto pBlock = reinterpret_cast<void*>(m_pHead);

	m_pHead = m_pHead->m_pNext;

	return pBlock;
}

void ProtectedPoolAllocatorImpl::deallocate(void* ptr)
{
	reinterpret_cast<FreeNode*>(ptr)->m_pNext = m_pHead;

	m_pHead = reinterpret_cast<FreeNode*>(ptr);
}

ProtectedPoolAllocatorImpl::~ProtectedPoolAllocatorImpl()
{
	if (!m_pBlock)
		return;

#if _WIN32 || _WIN64
	VirtualFree(m_pBlock, m_count * m_elementSize, MEM_DECOMMIT | MEM_FREE);
#elif __linux__
	munmap(m_pBlock, m_count * m_elementSize);
#endif
}