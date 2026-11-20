//The global allocation operators, in C++ for the 65C64 and W65C816 generators: new Type and new Type[count] get their
//memory here, delete and delete[] give it back.  A program can define its own instead.  [new.delete]
//
//When malloc has nothing, the new handler is called, and malloc asked again, until it gives memory or there is no
//handler: then operator new throws std::bad_alloc (in a program built with exceptions; NULL in one without).
//[new.delete.single] 4  The operators are weak: one the program defines is the one called, though this unit is linked
//for what else is in it.  [replacement.functions]
#include <new>
#include <stdlib.h>


//It has an initialiser so that it is in the data, NULL whether or not the start up clears the bss.
static std::new_handler	gpfNewHandler = NULL;


namespace std
{
	const nothrow_t	nothrow = nothrow_t();


	bad_alloc::bad_alloc(void) noexcept {}
	bad_alloc::bad_alloc(const bad_alloc& cOther) noexcept {}
	bad_alloc& bad_alloc::operator=(const bad_alloc& cOther) noexcept { return *this; }
	bad_alloc::~bad_alloc(void) {}
	const char* bad_alloc::what(void) const noexcept { return "std::bad_alloc"; }

	bad_array_new_length::bad_array_new_length(void) noexcept {}
	bad_array_new_length::~bad_array_new_length(void) {}
	const char* bad_array_new_length::what(void) const noexcept { return "std::bad_array_new_length"; }


	new_handler set_new_handler(new_handler pfHandler) noexcept
	{
		new_handler	pfOld;

		pfOld = gpfNewHandler;
		gpfNewHandler = pfHandler;
		return pfOld;
	}


	new_handler get_new_handler(void) noexcept
	{
		return gpfNewHandler;
	}
}


//malloc's memory, the new handler called while there is none.  NULL when there is no handler, without exceptions.
static void* Allocate(size_t uiBytes)
{
	void*				pv;
	std::new_handler	pfHandler;

	for (;;)
	{
		pv = malloc(uiBytes);
		if (pv)
		{
			return pv;
		}
		pfHandler = gpfNewHandler;
		if (!pfHandler)
		{
#ifdef __cpp_exceptions
			throw std::bad_alloc();
#else
			return NULL;
#endif // __cpp_exceptions
		}
		pfHandler();
	}
}


//The nothrow forms: NULL where the others throw.  [new.delete.single] 6
static void* AllocateOrNull(size_t uiBytes) noexcept
{
#ifdef __cpp_exceptions
	try
	{
		return Allocate(uiBytes);
	}
	catch (...)
	{
		return NULL;
	}
#else
	return Allocate(uiBytes);
#endif // __cpp_exceptions
}


__attribute__((weak)) void* operator new(size_t uiBytes)
{
	return Allocate(uiBytes);
}


__attribute__((weak)) void* operator new[](size_t uiBytes)
{
	return Allocate(uiBytes);
}


__attribute__((weak)) void* operator new(size_t uiBytes, const std::nothrow_t& cNothrow) noexcept
{
	return AllocateOrNull(uiBytes);
}


__attribute__((weak)) void* operator new[](size_t uiBytes, const std::nothrow_t& cNothrow) noexcept
{
	return AllocateOrNull(uiBytes);
}


__attribute__((weak)) void operator delete(void* pv) noexcept
{
	free(pv);
}


__attribute__((weak)) void operator delete[](void* pv) noexcept
{
	free(pv);
}


__attribute__((weak)) void operator delete(void* pv, size_t uiBytes) noexcept
{
	//As the standard's: a program that replaces only the one without the size has replaced both.  [new.delete.single] 16
	operator delete(pv);
}


__attribute__((weak)) void operator delete[](void* pv, size_t uiBytes) noexcept
{
	operator delete[](pv);
}


__attribute__((weak)) void operator delete(void* pv, const std::nothrow_t& cNothrow) noexcept
{
	free(pv);
}


__attribute__((weak)) void operator delete[](void* pv, const std::nothrow_t& cNothrow) noexcept
{
	free(pv);
}


#ifdef __cpp_exceptions
//new Type[count] whose count is more than the memory holds.  [expr.new] 7
extern "C" void __ThrowBadArrayNewLength(void)
{
	throw std::bad_array_new_length();
}
#endif // __cpp_exceptions
