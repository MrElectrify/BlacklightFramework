#ifndef BLACKLIGHT_NETWORKING_BUFFER_H_
#define BLACKLIGHT_NETWORKING_BUFFER_H_

/*
Buffer wrapper implementation
4/19/19 11:06
*/

#include <algorithm>
#include <array>
#include <cstddef>
#include <vector>

namespace Blacklight
{
	namespace Networking
	{
		/*
		A mutable buffer wrapper
		*/
		class Buffer
		{
		public:
			using Buffer_t = void*;

			// initializes with a buffer and the size of the buffer in bytes
			Buffer(Buffer_t buf, size_t byteSize);

			// Initializes a constexpr-sized buffer
			template<typename Data_t, size_t N>
			Buffer(Data_t(&buf)[N])
			{
				m_buf = buf;
				m_size = N * sizeof(Data_t);
			}

			// a buffer with a designated max size
			template<typename Data_t, size_t N>
			Buffer(Data_t(&buf)[N], size_t maxSize)
			{
				m_buf = buf;
				m_size = std::min(N * sizeof(Data_t), maxSize);
			}

			// a buffer initialized by std::array
			template<typename Data_t, size_t N>
			Buffer(std::array<Data_t, N>& arr)
			{
				m_buf = arr.data();
				m_size = N * sizeof(Data_t);
			}

			// a buffer initialized by std::array with a max size
			template<typename Data_t, size_t N>
			Buffer(std::array<Data_t, N>& arr, size_t maxSize)
			{
				m_buf = arr.data();
				m_size = std::min(N * sizeof(Data_t), maxSize);
			}

			// a buffer initialized by std::vector
			template<typename Data_t>
			Buffer(std::vector<Data_t>& vec)
			{
				m_buf = vec.data();
				m_size = vec.size() * sizeof(Data_t);
			}

			// a buffer initialized by std::vector with a max size
			template<typename Data_t>
			Buffer(std::vector<Data_t>& vec, size_t maxSize)
			{
				m_buf = vec.data();
				m_size = std::min(vec.size() * sizeof(Data_t), maxSize);
			}

			// returns a buffer pointer
			Buffer_t GetData() const noexcept;
			// returns the size of the buffer
			size_t GetSize() const noexcept;
		private:
			Buffer_t m_buf;
			size_t m_size;
		};
	}
}

#endif