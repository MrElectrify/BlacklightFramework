#ifndef BLACKLIGHT_SINGLETON_H_
#define BLACKLIGHT_SINGLETON_H_

/*
Singleton
5/26/19 18:28
*/

namespace Blacklight
{
	namespace Utils
	{
		template<typename T>
		class Singleton
		{
		public:
			static T& GetInstance()
			{
				static T inst;
				return inst;
			}
		};
	}
}

#endif