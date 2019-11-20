#include <Hooks/Detour.h>
#include <Hooks/VirtualModification.h>
#include <Hooks/VirtualReplacement.h>

#include <stdexcept>
#include <stdio.h>
#include <type_traits>

class A
{
public:
	A(int member) : m_member(member) {}

	virtual void vFunc0(/*__hidden thisptr*/)
	{
		printf("Virtual function 1 called: %d\n", m_member);
	}

	int GetMember() const { return m_member; };
private:
	int m_member;
};

// __thiscall (any nonstatic member function) is actually __fastcall in x64, and is similar to __fastcall in x86, because the instance pointer is passed in the ecx/rcx register
#if _WIN32 || _WIN64
#define FASTCALL __fastcall
#elif __linux__
#define FASTCALL __attribute__((fastcall))
#endif

typedef void (FASTCALL *vFunc0_t)(A*);
vFunc0_t vFunc0_o;

void FASTCALL hkvFunc0(A* thisptr)
{
	printf("Hooked vfunc0: %d\n", thisptr->GetMember());

	vFunc0_o(thisptr);
}

int AnotherFunc(int a, float b)
{
	printf("Another function called with args %d %.2f\n", a, b);

	// truncated to an int because we return an int
	return a + b;
}

using AnotherFunc_t = std::add_pointer_t<decltype(AnotherFunc)>;
AnotherFunc_t AnotherFunc_o;

int hkAnotherFunc(int a, float b)
{
	// remember, we can't actually *see* the function when we are hooking it. we call the trampoline
	auto res = AnotherFunc_o(a, b);

	printf("Hooked another function with args %d %.2f. Returning a - b instead of a + b, which would result in %d\n", a, b, res);

	return a - b;
}

using MessageBoxA_t = std::add_pointer_t<decltype(MessageBoxA)>;
MessageBoxA_t MessageBoxA_o;

int WINAPI hkMessageBoxA(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT type)
{
	return MessageBoxA_o(hWnd, lpText, lpCaption, type);
}

int main()
{
	// hook demonstration. the only portable way to hook a non-virtual function (VEH is only windows, which I'll add) is to detour
	auto res = AnotherFunc(5, 10.4f);
	// this should return 5 + 10.4 truncated to 15.

	printf("Result: %d\n", res);

	// now let's detour it

	{
		Blacklight::Hooks::Detour afd(AnotherFunc, hkAnotherFunc);
		AnotherFunc_o = afd.GetOriginal();

		afd.Enable();

		// this will pass through our hook, which returns a different result, and could even modify our input if it wanted to. It should normally be 15, but is -5 now
		res = AnotherFunc(5, 10.4f);

		printf("Result: %d\n", res);
	} // we leave the scope, afd is destructed, causing the function to be un-hooked

	// now it will not pass through the hook, and will go straight to the real function and return 15 again
	res = AnotherFunc(5, 10.4f);

	printf("Result: %d\n", res);

	// pool allocator of size 2
	Blacklight::Memory::PoolAllocator<A> alloc(2);

	// construct two instances
	auto a = alloc.allocate(127);
	auto b = alloc.allocate(112);

	a->vFunc0();
	b->vFunc0();

	{
		// virtual detour, detours the virtual function. not very hard to detect and very complex (some situations are 
		// not covered by my hook yet in 64-bit mode, specifically rip-relative instructions and any conditional jumps in the first MAX_TRAMPOLINE_LENGTH bytes)
		Blacklight::Hooks::VirtualDetour vd(a, 0, hkvFunc0);
		vFunc0_o = vd.GetOriginal();

		vd.Enable();

		// because we call a virtual function on a pointer, it is deduced runtime rather than compile time
		a->vFunc0();
		// notice how it affects both instances, it is a global hook
		b->vFunc0();
	} // we leave the scope, the hook is destructed so the function is unhooked

	a->vFunc0();
	b->vFunc0();

	{
		// replaces the function pointer in the vtable with our hook. the simplest, but also about as easy to detect.
		Blacklight::Hooks::VirtualModification vm(b, 0, hkvFunc0);
		
		// we can't use the saved original from the detour, because it is specially-allocated trampoline function stub, not the actual original
		vFunc0_o = vm.GetOriginal();

		vm.Enable();

		// this one is also global because it replaces all vtable entries
		a->vFunc0();
		b->vFunc0();
	} // hook is destructed yet again

	a->vFunc0();
	b->vFunc0();

	{
		// creates a copy of the vtable and replaces the function pointer with ours, then replaces the vtable pointer in the instance with out vtable pointer.
		// it is much harder to detect, but not impossible. also slightly more complex than modification
		Blacklight::Hooks::VirtualReplacement vr(b, 0, hkvFunc0, true /* enabled */);

		// we can use vFunc0_t now because it is the real function

		// this one replaces only one instance, so in this case, it'll only affect b
		a->vFunc0();
		b->vFunc0();
	} // destructed

	return 0;
}