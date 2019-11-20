#ifndef BLACKLIGHT_RANDOM_SSERAND_H_
#define BLACKLIGHT_RANDOM_SSERAND_H_

/*
 *	SSE Random
 *	9/13/19 23:08
 */

#include <cstdint>
#include <emmintrin.h>

namespace Blacklight
{
	namespace Random
	{
		/*
		 *	SSERandom is a fast SSE2-based implementation of rand. Not thread safe
		 */

		static __m128i s_seed;

		class SSERand
		{
		public:
			// self-explanatory
			static void Seed(uint32_t seed);
			// generates 16 random bytes
			static void GenerateBlock(uint8_t* output);
		};
	}
}

#endif