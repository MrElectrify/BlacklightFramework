#ifndef BLACKLIGHT_HOOKS_DETOUR_H_
#define BLACKLIGHT_HOOKS_DETOUR_H_

/*
Includes the basic detour as well as virtual detour
4/18/19 19:12
*/

#include <Memory/Allocators.h>
#include <Hooks/Hook.h>

#ifdef BLACKLIGHT_64BIT
#define MAX_TRAMPOLINE_LENGTH 12
#elif BLACKLIGHT_32BIT
#define MAX_TRAMPOLINE_LENGTH 5
#endif

#define MAX_CONCURRENT_HOOKS 64

namespace Blacklight
{
	namespace Hooks
	{
		namespace impl
		{
			/*
			Implicit Detours
			*/
			class DetourImpl : public Hook
			{
			public:
				using BaseClass_t = Hook;
				using FunctionPart_t = typename BaseClass_t::FunctionPart_t;
				using FunctionImpl_t = void*;

				struct TrampolineFunc_t
				{
					FunctionPart_t m_data[16 + MAX_TRAMPOLINE_LENGTH * 2];
				};

				// constructor for a hook
				DetourImpl(FunctionImpl_t pOrigFn, FunctionImpl_t pHookFn, bool enable = false);

				// returns true if enabled successfully
				bool Enable();

				// returns true if disabled successfully
				bool Disable();

				// returns a pointer to the trampoline
				FunctionImpl_t GetOriginal();

				// disables the hook if it is enabled
				~DetourImpl();
			private:
				FunctionImpl_t m_pHookFn;
				FunctionImpl_t m_pOrigFn;
				TrampolineFunc_t* m_trampolineFunction;
				FunctionPart_t m_origData[MAX_TRAMPOLINE_LENGTH];

				size_t m_functionLength;

				bool m_hooked;
			};

			extern Memory::ProtectedPoolAllocator<DetourImpl::TrampolineFunc_t> g_hookBlock;
		}

		/*
		This is a form of hook that re-routes execution to a hook
		by placing a near jmp in that function. This form of detour
		is detectable by searching for the jmp opcode at the beginning
		of the function
		*/
		template<typename Function_t>
		class Detour
		{
		public:
			using impl_t = impl::DetourImpl;
		private:
			impl_t m_impl;
		public:
			// constructor for a hook
			Detour(Function_t pOrigFn, Function_t pHookFn, bool enable = false) :
				m_impl(reinterpret_cast<impl_t::FunctionImpl_t>(pOrigFn), reinterpret_cast<impl_t::FunctionImpl_t>(pHookFn), enable) {}

			// returns true if enabled successfully
			bool Enable() { return m_impl.Enable(); }

			// returns true if disabled successfully
			bool Disable() { return m_impl.Disable(); }

			// returns a pointer to the trampoline
			Function_t GetOriginal() { return reinterpret_cast<Function_t>(m_impl.GetOriginal()); }
		};

		/*
		A detour that hooks a virtual function by an instance of an object
		and the virtual function index. For more about virtuals, see the base
		Detour
		*/
		template<typename Function_t>
		class VirtualDetour
		{
		public:
			using ClassInstance_t = void*;
			using impl_t = impl::DetourImpl;
			using Index_t = unsigned int;
			using VTable_t = impl_t::FunctionImpl_t*;
		private:
			impl_t m_impl;
		public:
			// constructor for a hook
			VirtualDetour(ClassInstance_t pInstance, Index_t index, Function_t pHookFn, bool enable = false) :
				m_impl(*reinterpret_cast<VTable_t*>(pInstance)[index], reinterpret_cast<impl_t::FunctionImpl_t>(pHookFn), enable) {}

			// returns true if enabled successfully
			bool Enable() { return m_impl.Enable(); }

			// returns true if disabled successfully
			bool Disable() { return m_impl.Disable(); }

			// returns a pointer to the trampoline
			Function_t GetOriginal() { return reinterpret_cast<Function_t>(m_impl.GetOriginal()); }
		};

	}
}

#endif