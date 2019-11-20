#ifndef BLACKLIGHT_MEMORYCONTROLLER_H_
#define BLACKLIGHT_MEMORYCONTROLLER_H_

/*
Memory Controller
5/27/19 22:53
*/

#include <cstddef>
#include <cstdint>

namespace Blacklight
{
	namespace VM
	{
		// MemoryController provides an interface to virtual memory
		class MemoryController
		{
		public:
			MemoryController(const size_t blockSize);

			// Returns the designated size data at an address
			const uint8_t Read8(const uintptr_t address) const;
			const uint16_t Read16(const uintptr_t address) const;
			const uint32_t Read32(const uintptr_t address) const;

			// Writes the designated size data to an address
			void Write8(const uintptr_t address, const uint8_t data);
			void Write16(const uintptr_t address, const uint16_t data);
			void Write32(const uintptr_t address, const uint32_t data);

			// Returns the raw memory buffer
			uint8_t* GetRawMemory();

			~MemoryController();
		private:
			size_t m_blockSize;
			uint8_t* m_memory;
		};
	}
}

#endif