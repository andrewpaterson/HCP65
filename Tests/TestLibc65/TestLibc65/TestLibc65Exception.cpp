#include "TestLib/Assert.h"
#include "TestLibc65Context.h"


//What every program here starts with: the runtime's routines called as the compiler will call them (it can't compile a
//throw yet: CommonLibraries CppCompilerLib/ExceptionsPlan.txt stage 4), with typeid for the type infos.
#define TEST_LIBC65_EXCEPTION_PRELUDE \
	"#include <exception>\n" \
	"#include <typeinfo>\n" \
	"#include <stdlib.h>\n" \
	"#include <new>\n" \
	"long* gp;\n" \
	"int giDestroyed;\n" \
	"void R(long l) { *gp++ = l; }\n" \
	"struct A { int a; virtual ~A() {} };\n" \
	"struct B : A { int b; ~B() { giDestroyed += 10; } };\n" \
	"struct P : private A { int p; };\n" \
	"struct L : A { int l; };\n" \
	"struct N : A { int n; };\n" \
	"struct M : L, N { int m; };\n" \
	"struct V { int v; virtual ~V() {} };\n" \
	"struct W : virtual V { int w; };\n" \
	"struct X : W { int x; };\n" \
	"struct Y { int y; virtual ~Y() {} };\n" \
	"struct Z : Y, W { int z; };\n" \
	"const void* Info(const std::type_info& c) { return &c; }\n" \
	"template<class T> void Destroy(void* pv) { giDestroyed++; ((T*)pv)->~T(); }\n" \
	"void Counted(void* pv) { giDestroyed++; }\n" \
	"template<class T> void* Throw(const T& t) { void* pv = __AllocateException(sizeof(T)); new (pv) T(t); __Throw(pv, Info(typeid(T)), &Destroy<T>); return pv; }\n" \
	"template<class T> void* ThrowPlain(T t) { void* pv = __AllocateException(sizeof(T)); *(T*)pv = t; __Throw(pv, Info(typeid(T)), &Counted); return pv; }\n" \
	"template<class H> void* Match() { return __Match(Info(typeid(H))); }\n" \
	"void Catch() { __BeginCatch(); __EndCatch(); }\n"


//////////////////////////////////////////////////////////////////////////
//
// __Match, [except.handle] 3: the same type; an unambiguous public base
// class; a pointer with const or volatile added, to void, or to an
// unambiguous public base class, moved to it (through the object's vtable
// for a virtual base of an object that is itself a base); nullptr for any
// pointer.  And what doesn't match.
//
//////////////////////////////////////////////////////////////////////////
void TestLibc65ExceptionMatch(void)
{
	static const char*	aszUnits[] = { "exception.cpp", "stdlib.cpp", "string.cpp", "string.asm", "ctype.cpp", "new.cpp", NULL };
	static const int32	aiExpected[] =
	{
		1, 0, 0, 0, 0,						//an int: int; not long, unsigned, short or const int*
		1, 1, 0, 1, 0,						//a B: B, its base A; not P; uncaught before the handler, not after
		1, 0, 1,							//a P: P; not its private base A.  Destroyed once caught: 1
		1, 0, 1, 1,							//an M: M; not A (two of them); L and N, each where it is
		1, 1, 1, 1, 1,						//a B*: B*, A* (moved), const A*, void*, const void*
		0, 1, 0, 1, 0,						//a const B*: not B*; const A*; not void*; const void*; not A*
		0, 1, 0, 1,							//a B**: not A**; B* const*; not const B**; const B* const*
		1, 1,								//a null B*: A*, null
		1, 1, 1,							//a W* that is part of an X, and of a Z: V* where V is in that object; an X: V
		1, 1, 0,							//nullptr: int* and A*, null; not int
		0,									//nothing left in flight
	};
	STestLibc65Run		sRun;
	size				i;

	AssertTrue(TestLibc65Begin(&sRun, aszUnits, "",
		TEST_LIBC65_EXCEPTION_PRELUDE
		"int main()\n"
		"{\n"
		"	void*	pv;\n"
		"	B		b;\n"
		"	B*		pb;\n"
		"	const B*	pcb;\n"
		"	B**		ppb;\n"
		"	X		x;\n"
		"	Z		z;\n"
		"	W*		pw;\n"
		"	gp = (long*)0x100000;\n"
		"	pv = ThrowPlain<int>(5);\n"
		"	R(Match<int>() == pv); R(Match<long>() != 0); R(Match<unsigned int>() != 0); R(Match<short>() != 0); R(Match<const int*>() != 0);\n"
		"	Catch();\n"
		"	pv = Throw<B>(b);\n"
		"	R(Match<B>() == pv); R(Match<A>() == (A*)(B*)pv); R(Match<P>() != 0); R(std::uncaught_exception());\n"
		"	Catch();\n"
		"	R(std::uncaught_exception());\n"
		"	giDestroyed = 0;\n"
		"	pv = Throw<P>(P());\n"
		"	R(Match<P>() == pv); R(Match<A>() != 0);\n"
		"	Catch();\n"
		"	R(giDestroyed);\n"
		"	pv = Throw<M>(M());\n"
		"	R(Match<M>() == pv); R(Match<A>() != 0); R(Match<L>() == (L*)(M*)pv); R(Match<N>() == (N*)(M*)pv);\n"
		"	Catch();\n"
		"	pb = &b;\n"
		"	pv = ThrowPlain<B*>(pb);\n"
		"	R(Match<B*>() == pv); R(*(A**)Match<A*>() == (A*)pb); R(*(const A**)Match<const A*>() == (A*)pb); R(*(void**)Match<void*>() == pb); R(*(const void**)Match<const void*>() == pb);\n"
		"	Catch();\n"
		"	pcb = &b;\n"
		"	ThrowPlain<const B*>(pcb);\n"
		"	R(Match<B*>() != 0); R(*(const A**)Match<const A*>() == (const A*)pcb); R(Match<void*>() != 0); R(*(const void**)Match<const void*>() == pcb); R(Match<A*>() != 0);\n"
		"	Catch();\n"
		"	ppb = &pb;\n"
		"	ThrowPlain<B**>(ppb);\n"
		"	R(Match<A**>() != 0); R(*(B* const**)Match<B* const*>() == ppb); R(Match<const B**>() != 0); R(*(const B* const**)Match<const B* const*>() == (const B* const*)ppb);\n"
		"	Catch();\n"
		"	pb = 0;\n"
		"	ThrowPlain<B*>(pb);\n"
		"	R(Match<A*>() != 0); R(*(A**)Match<A*>() == 0);\n"
		"	Catch();\n"
		"	pw = &x;\n"
		"	ThrowPlain<W*>(pw);\n"
		"	R(*(V**)Match<V*>() == (V*)pw);\n"
		"	Catch();\n"
		"	pw = &z;\n"
		"	ThrowPlain<W*>(pw);\n"
		"	R(*(V**)Match<V*>() == (V*)pw);\n"
		"	Catch();\n"
		"	pv = Throw<X>(x);\n"
		"	R(Match<V>() == (V*)(X*)pv);\n"
		"	Catch();\n"
		"	ThrowPlain<decltype(nullptr)>(nullptr);\n"
		"	R((Match<int*>() != 0) && (*(int**)Match<int*>() == 0)); R((Match<A*>() != 0) && (*(A**)Match<A*>() == 0)); R(Match<int>() != 0);\n"
		"	Catch();\n"
		"	R(Match<int>() != 0);\n"
		"	return 0;\n"
		"}\n"));
	AssertTrue(TestLibc65Run(&sRun, 4000000));

	for (i = 0; i < sizeof(aiExpected) / sizeof(int32); i++)
	{
		AssertInt(aiExpected[i], TestLibc65Result(&sRun, i));
	}

	TestLibc65End(&sRun);
}


//////////////////////////////////////////////////////////////////////////
//
// The exceptions in flight and the ones being handled are stacks: a second
// thrown and caught while the first is being handled, the first rethrown
// from its handler and caught again.  An exception is destroyed when its
// last handler is left and it isn't in flight, and not before.  The pool
// gives memory when malloc has none.
//
//////////////////////////////////////////////////////////////////////////
void TestLibc65ExceptionStacks(void)
{
	static const char*	aszUnits[] = { "exception.cpp", "stdlib.cpp", "string.cpp", "string.asm", "ctype.cpp", "new.cpp", NULL };
	static const int32	aiExpected[] =
	{
		1, 0,								//thrown: in flight; in its handler: not
		1, 1, 0, 1,							//a second one in the first's handler: in flight, matched, caught; destroyed (1), the first not
		1, 1,								//the first rethrown: in flight again, and still an A
		1,									//its first handler left by the rethrow: it isn't destroyed
		11,									//caught again and that handler left: destroyed now (the B's own 10 and Destroy's 1)
		0,									//nothing in flight
		1, 1, 1, 1,							//with no memory: an exception still gets some, is thrown, matched and caught
		1,									//and with memory again, from malloc
	};
	STestLibc65Run		sRun;
	size				i;

	AssertTrue(TestLibc65Begin(&sRun, aszUnits, "",
		TEST_LIBC65_EXCEPTION_PRELUDE
		"void* gapv[64];\n"
		"int main()\n"
		"{\n"
		"	void*	pv;\n"
		"	void*	pvPooled;\n"
		"	B		b;\n"
		"	int		i;\n"
		"	int		iBlocks;\n"
		"	gp = (long*)0x100000;\n"
		"	Throw<B>(b);\n"
		"	R(std::uncaught_exception());\n"
		"	__BeginCatch();\n"
		"	R(std::uncaught_exception());\n"
		"	giDestroyed = 0;\n"
		"	pv = ThrowPlain<int>(7);\n"
		"	R(std::uncaught_exception()); R(Match<int>() == pv);\n"
		"	Catch();\n"
		"	R(std::uncaught_exception()); R(giDestroyed);\n"
		"	giDestroyed = 0;\n"
		"	__Rethrow();\n"
		"	R(std::uncaught_exception()); R(Match<A>() != 0);\n"
		"	__EndCatch();\n"
		"	R(giDestroyed == 0);\n"
		"	Catch();\n"
		"	R(giDestroyed);\n"
		"	R(Match<A>() != 0);\n"
		"	iBlocks = 0;\n"
		"	while ((iBlocks < 64) && ((gapv[iBlocks] = malloc(30000)) != 0)) { iBlocks++; }\n"
		"	while (malloc(16) != 0) {}\n"
		"	pvPooled = __AllocateException(sizeof(long));\n"
		"	R(pvPooled != 0);\n"
		"	*(long*)pvPooled = 99;\n"
		"	__Throw(pvPooled, Info(typeid(long)), 0);\n"
		"	R(Match<long>() == pvPooled); R(*(long*)pvPooled == 99);\n"
		"	Catch();\n"
		"	R(!std::uncaught_exception());\n"
		"	for (i = 0; i < iBlocks; i++) { free(gapv[i]); }\n"
		"	pv = ThrowPlain<int>(1);\n"
		"	R(pv != pvPooled);\n"
		"	Catch();\n"
		"	return 0;\n"
		"}\n"));
	AssertTrue(TestLibc65Run(&sRun, 40000000));

	for (i = 0; i < sizeof(aiExpected) / sizeof(int32); i++)
	{
		AssertInt(aiExpected[i], TestLibc65Result(&sRun, i));
	}

	TestLibc65End(&sRun);
}


//////////////////////////////////////////////////////////////////////////
//
// std::terminate calls the handler that was set, once, and stops; throw;
// with nothing being handled is terminate.  The handler here ends the
// program with _Exit, whose status the test reads.
//
//////////////////////////////////////////////////////////////////////////
void TestLibc65ExceptionTerminate(void)
{
	static const char*	aszUnits[] = { "exception.cpp", "stdlib.cpp", "string.cpp", "string.asm", "ctype.cpp", "new.cpp", NULL };
	STestLibc65Run		sRun;

	AssertTrue(TestLibc65Begin(&sRun, aszUnits, "",
		TEST_LIBC65_EXCEPTION_PRELUDE
		"void Ended() { _Exit(77); }\n"
		"int main()\n"
		"{\n"
		"	std::terminate_handler	pf;\n"
		"	gp = (long*)0x100000;\n"
		"	pf = std::set_terminate(&Ended);\n"
		"	R(pf == 0); R(std::get_terminate() == &Ended);\n"
		"	__Rethrow();\n"
		"	return 3;\n"
		"}\n"));
	AssertTrue(TestLibc65RunToStop(&sRun, 4000000));
	AssertInt(77, TestLibc65Return(&sRun));
	AssertInt(1, TestLibc65Result(&sRun, 0));
	AssertInt(1, TestLibc65Result(&sRun, 1));
	TestLibc65End(&sRun);

	//Without a handler it stops in abort.
	AssertTrue(TestLibc65Begin(&sRun, aszUnits, "",
		TEST_LIBC65_EXCEPTION_PRELUDE
		"int main()\n"
		"{\n"
		"	gp = (long*)0x100000;\n"
		"	R(5);\n"
		"	std::terminate();\n"
		"	R(6);\n"
		"	return 3;\n"
		"}\n"));
	AssertTrue(TestLibc65RunToStop(&sRun, 4000000));
	AssertInt(5, TestLibc65Result(&sRun, 0));
	AssertInt(0, TestLibc65Result(&sRun, 1));
	TestLibc65End(&sRun);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void TestLibc65Exception(void)
{
	BeginTests();

	TestLibc65ExceptionMatch();
	TestLibc65ExceptionStacks();
	TestLibc65ExceptionTerminate();

	TestStatistics();
}
