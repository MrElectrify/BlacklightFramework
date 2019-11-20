#include <Networking/Buffer.h>

using Blacklight::Networking::Buffer;

Buffer::Buffer(void* buf, size_t byteSize) : m_buf(buf), m_size(byteSize) {}

void* Buffer::GetData() const noexcept
{
	return m_buf;
}

size_t Buffer::GetSize() const noexcept
{
	return m_size;
}