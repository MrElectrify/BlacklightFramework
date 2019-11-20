#ifndef BLACKLIGHT_MEMORY_MEMORYPROTECTIONGUARD_H_
#define BLACKLIGHT_MEMORY_MEMORYPROTECTIONGUARD_H_

#include <cstddef>
#include <cstdint>

#if _WIN32 || _WIN64
#include <Windows.h>
#endif

namespace Blacklight
{
	namespace Memory
	{
		class MemoryProtectionGuard
		{
		public:
			MemoryProtectionGuard(void* pLoc, size_t size, int protection);
			~MemoryProtectionGuard();
		private:
			void* m_pLoc;
			size_t m_size;
#if _WIN32 || _WIN64
			DWORD m_oldProtection;
#elif __linux__
			int m_oldProtection;
#endif
		};
	}
}

#endif