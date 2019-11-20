#ifndef BLACKLIGHT_HOOKS_VIRTUALMODIFICATION_H_
#define BLACKLIGHT_HOOKS_VIRTUALMODIFICATION_H_

/*
Virtual modification hook
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
			Implicit modification hook
			*/
			class VirtualModificationImpl : public Hook
			{
			public:
				using BaseClass_t = Hook;
				using ClassInstance_t = void*;
				using Index_t = unsigned int;
				using FunctionImpl_t = void*;
				using VTable_t = FunctionImpl_t * ;

				// constructor for VMT replacement hook
				VirtualModificationImpl(ClassInstance_t pInstance, Index_t index, FunctionImpl_t pHookFn, bool enabled = false);

				// returns true if enabled successfully
				bool Enable();

				// returns true if disabled successfully
				bool Disable();

				// returns a pointer to the trampoline
				FunctionImpl_t GetOriginal();

				// disables the hook if it is enabled
				~VirtualModificationImpl();
			private:
				FunctionImpl_t m_pHookFn;
				FunctionImpl_t m_pOrigFn;

				Index_t m_index;

				VTable_t m_pOrigVtbl;

				bool m_hooked;
			};
		}

		/*
		This replaces the function in the original vtable with a hook function,
		therefore hooking every instance of a class. As it modifies the data of
		another module, it is more easily detectable but easier to manage.
		*/
		template<typename Function_t>
		class VirtualModification
		{
		public:
			using impl_t = impl::VirtualModificationImpl;

			using ClassInstance_t = impl_t::ClassInstance_t;
			using Index_t = impl_t::Index_t;
		private:
			impl_t m_impl;
		public:
			// constructor for a hook
			VirtualModification(ClassInstance_t pInstance, Index_t index, Function_t pHookFn, bool enable = false) :
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