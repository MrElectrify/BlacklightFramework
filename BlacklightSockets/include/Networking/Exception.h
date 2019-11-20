#ifndef BLACKLIGHT_NETWORKING_EXCEPTION_H_
#define BLACKLIGHT_NETWORKING_EXCEPTION_H_

/*
Defines network exception, derived from std::runtime_error
4/18/19 23:48
*/

#include <stdexcept>

namespace Blacklight
{
	namespace Networking
	{
		/*
		This exception is thrown by anything networking-related
		*/
		class Exception : public std::runtime_error
		{
		public:
			// constructor for exception
			Exception(const char* what) : std::runtime_error(what) {}
		};
	}
}

#endif