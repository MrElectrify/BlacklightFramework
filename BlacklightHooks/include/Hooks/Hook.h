#ifndef BLACKLIGHT_HOOKS_HOOK_H_
#define BLACKLIGHT_HOOKS_HOOK_H_

#ifdef _MSC_VER
#define BLACKLIGHT_WINDOWS 1
#if _WIN64
#define BLACKLIGHT_64BIT 1
#else
#define BLACKLIGHT_32BIT 1
#endif
#elif __linux__
#define BLACKLIGHT_LINUX 1
#if __x86_64__ || __ppc64__
#define BLACKLIGHT_64BIT
#else
#define BLACKLIGHT_32BIT
#endif
#endif

namespace Blacklight
{
	namespace Hooks
	{
		class Hook
		{
		public:
			using FunctionPart_t = unsigned char;
		public:
			// returns true if enabled successfully
			virtual bool Enable() = 0;

			// returns true if disabled successfully
			virtual bool Disable() = 0;
		};
	}
}

#endif