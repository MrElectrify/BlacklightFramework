#ifndef BLACKLIGHT_MEMORY_ALLOCATORS_H_
#define BLACKLIGHT_MEMORY_ALLOCATORS_H_

#include <cstddef>
#include <stdexcept>
#include <utility>

#if _WIN32 || _WIN64
#include <Windows.h>
#elif __linux__
#include <sys/mman.h>
#endif

namespace Blacklight
{
	namespace Memory
	{
		namespace impl
		{
			struct FreeNode
			{
				FreeNode* m_pNext;
			};

			/*
			Implicit typeless allocator
			*/
			class PoolAllocatorImpl
			{
			public:
				PoolAllocatorImpl(size_t count, size_t elementSize, int flags);

				void* allocate();

				void deallocate(void* ptr);

				~PoolAllocatorImpl();

				int m_flags;
			private:
				void* m_pBlock;
				FreeNode* m_pHead;
			};

			/*
			Implicit typeless protected allocator
			*/
			class ProtectedPoolAllocatorImpl
			{
			public:
				ProtectedPoolAllocatorImpl(size_t count, size_t elementSize, int protection, int flags);

				void* allocate();

				void deallocate(void* ptr);

				~ProtectedPoolAllocatorImpl();

				int m_flags;
			private:
				void* m_pBlock;
				FreeNode* m_pHead;

				size_t m_count;
				size_t m_elementSize;
			};
		}

		// flags for a ProtectedPoolAllocator
		enum PoolAllocatorFlags
		{
			// if default construction is not an option or does not need to be done, this can help with performance
			PoolAllocatorFlags_NOCONSTRUCT = (1 << 0)
		};

		/*
		A simple memory pool
		*/
		template<typename T>
		class PoolAllocator
		{
		public:
			using impl_t = impl::PoolAllocatorImpl;
		private:
			impl_t m_impl;
		public:
			// allocates a block of a number of items with the specified protection
			PoolAllocator(size_t count, int flags = 0) :
				m_impl(count, sizeof(T), flags)
			{
				static_assert(sizeof(T) >= sizeof(void*), "sizeof(T) must be at least as big as void*");
			}

			// allocate a block from the pool, returns nullptr on failure
			template<typename... Args>
			T* allocate(Args&&... args)
			{
				auto pBlock = m_impl.allocate();

				if (!(m_impl.m_flags & PoolAllocatorFlags_NOCONSTRUCT))
					new(pBlock)T(std::forward<Args>(args)...);

				return reinterpret_cast<T*>(pBlock);
			}

			// return the block to the pool, assume from the same pool
			void deallocate(T* ptr)
			{
				m_impl.deallocate(ptr);
			}
		};

		/*
		A simple protected block pool
		*/
		template<typename T>
		class ProtectedPoolAllocator
		{
		public:
			using impl_t = impl::ProtectedPoolAllocatorImpl;
		private:
			impl_t m_impl;
		public:
			// allocates a block of a number of items with the specified protection
			ProtectedPoolAllocator(size_t count, int protection, int flags = 0) :
				m_impl(count, sizeof(T), protection, flags)
			{
				static_assert(sizeof(T) >= sizeof(void*), "sizeof(T) must be at least as big as void*");
			}

			// allocate a block from the pool, returns nullptr on failure
			template<typename... Args>
			T* allocate(Args&&... args)
			{
				auto pBlock = m_impl.allocate();

				if (!(m_impl.m_flags & PoolAllocatorFlags_NOCONSTRUCT))
					new(pBlock)T(std::forward<Args>(args)...);

				return reinterpret_cast<T*>(pBlock);
			}

			// return the block to the pool, assume from the same pool
			void deallocate(T* ptr)
			{
				m_impl.deallocate(ptr);
			}
		};
	}
}

#endif