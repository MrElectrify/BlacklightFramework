#ifndef BLACKLIGHT_HOOKS_VIRTUALREPLACEMENT_H_
#define BLACKLIGHT_HOOKS_VIRTUALREPLACEMENT_H_

/*
Virtual replacement hook
4/18/19 19:13
*/

#include <Hooks/Hook.h>

namespace Blacklight
{
	namespace Hooks
	{
		namespace impl
		{
			/*
			Implicit VMTReplacementHook
			*/
			class VirtualReplacementImpl : public Hook
			{
			public:
				using BaseClass_t = Hook;
				using ClassInstance_t = void*;
				using Index_t = unsigned int;
				using FunctionImpl_t = void*;
				using VTable_t = FunctionImpl_t * ;

				// constructor for VMT replacement hook
				VirtualReplacementImpl(ClassInstance_t pInstance, Index_t index, FunctionImpl_t pHookFn, bool enabled = false);

				// returns true if enabled successfully
				bool Enable();

				// returns true if disabled successfully, be sure the instance is still valid
				bool Disable();

				// returns a pointer to the trampoline
				FunctionImpl_t GetOriginal();

				// disables the hook if it is enabled
				~VirtualReplacementImpl();
			private:
				FunctionImpl_t m_pOrigFn;

				ClassInstance_t m_pInstance;

				VTable_t m_pOrigVtbl;
				VTable_t m_pHookVtbl;

				bool m_hooked;
			};
		}

		/*
		This is a hook for a certain class instance that copies its vtable, and replaces the
		vtable pointer with a hook at a certain index. It is relatively stealthy, but requires
		an instance of that class to hook, and therefore cannot be applied to every object
		*/
		template<typename Function_t>
		class VirtualReplacement
		{
		public:
			using impl_t = impl::VirtualReplacementImpl;

			using ClassInstance_t = impl_t::ClassInstance_t;
			using Index_t = impl_t::Index_t;
		private:
			impl_t m_impl;
		public:
			// constructor for a hook
			VirtualReplacement(ClassInstance_t pInstance, Index_t index, Function_t pHookFn, bool enable = false) :
				m_impl(pInstance, index, reinterpret_cast<impl_t::FunctionImpl_t>(pHookFn), enable) {}

			// returns true if enabled successfully
			bool Enable() { return m_impl.Enable(); }

			// returns true if disabled successfully
			bool Disable() { return m_impl.Disable(); }

			// returns a pointer to the original
			Function_t GetOriginal() { return reinterpret_cast<Function_t>(m_impl.GetOriginal()); }
		};

	}
}

#endif