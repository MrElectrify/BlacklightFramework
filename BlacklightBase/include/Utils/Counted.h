#ifndef BLACKLIGHT_COUNTER_H_
#define BLACKLIGHT_COUNTER_H_

/*
Counter
5/26/19 22:34
*/

namespace Blacklight
{
	namespace Utils
	{
		using Index_t = unsigned int;
		static Index_t s_nextIndex = 0;

		// Counts the amount of counted derived children there are
		template<typename T>
		class TemplateCounted
		{
		public:
			static Index_t TypeIndex()
			{
				static Index_t index = s_nextIndex++;
				return index;
			}
		};
	}
}

#endif