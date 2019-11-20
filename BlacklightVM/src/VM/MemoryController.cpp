#include <VM/MemoryController.h>

#include <cassert>

using Blacklight::VM::MemoryController;

MemoryController::MemoryController(const size_t blockSize) :
	m_blockSize(blockSize)
{
	// ensure we don't make more memory than we can address
	assert(blockSize < UINT32_MAX);

	m_memory = new uint8_t[blockSize];
}

const uint8_t MemoryController::Read8(const uintptr_t address) const
{
	// ensure we are not trying to read memory that is not allocated to us
	assert(address < m_blockSize);

	return m_memory[address];
}
const uint16_t MemoryController::Read16(const uintptr_t address) const
{
	// ensure we are not trying to read memory that is not allocated to us
	assert(address < m_blockSize - 1);

	// faster than bitwise
	return *reinterpret_cast<uint16_t*>(&m_memory[address]);
}
const uint32_t MemoryController::Read32(const uintptr_t address) const
{
	// ensure we are not trying to read memory that is not allocated to us
	assert(address < m_blockSize - 3);

	return *reinterpret_cast<uint32_t*>(&m_memory[address]);
}

void MemoryController::Write8(const uintptr_t address, const uint8_t data)
{
	// ensure we are not trying to write to memory that is not allocated to us
	assert(address < m_blockSize);

	m_memory[address] = data;
}

void MemoryController::Write16(const uintptr_t address, const uint16_t data)
{
	// ensure we are not trying to write to memory that is not allocated to us
	assert(address < m_blockSize - 1);

	*reinterpret_cast<uint16_t*>(&m_memory[address]) = data;
}

void MemoryController::Write32(const uintptr_t address, const uint32_t data)
{
	// ensure we are not trying to write to memory that is not allocated to us
	assert(address < m_blockSize);

	*reinterpret_cast<uint32_t*>(&m_memory[address]) = data;
}

uint8_t* MemoryController::GetRawMemory()
{
	return m_memory;
}

MemoryController::~MemoryController()
{
	delete[]m_memory;
}