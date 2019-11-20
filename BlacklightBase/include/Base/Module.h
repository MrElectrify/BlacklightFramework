#ifndef BLACKLIGHT_MODULE_H_
#define BLACKLIGHT_MODULE_H_

/*
Base module
5/26/19 16:04
*/

#include <Utils/Counted.h>

namespace Blacklight
{
	namespace Base
	{
		/*
		A module is a non-singleton 
		*/
		class Module
		{
		public:
			// All hooks will be created here
			virtual void CreateHooks() = 0;
			// All hooks will be destroyed here
			virtual void DestroyHooks() = 0;

			// A module will update as necessary every 5 milliseconds, and its update function will be called
			virtual void Update() = 0;

			virtual ~Module();
		};
	}
}

#endif