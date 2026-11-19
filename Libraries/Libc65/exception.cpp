//The exceptions' runtime (CommonLibraries CppCompilerLib/ExceptionsPlan.txt).  The compiler writes the way out of
//each function (a function that throws returns with a flag set, and its caller tests it); what is here is the rest:
//where the thrown object lives, which handler takes it ([except.handle] 3), and what happens when none does.
//
//Nothing in this file throws or tests a call, so it is the same compiled with exceptions on or off.
#include <exception>
#include <typeinfo>
#include <stdlib.h>


//A type info as the compiler writes it (CppCompilerLib/CPPVtable.h): its name and a word whose low byte is its kind,
//then what the kind has.  Every word is 4 bytes on both processors.
#define TYPE_INFO_KIND_NULL_POINTER		2
#define TYPE_INFO_KIND_POINTER			3
#define TYPE_INFO_KIND_MEMBER_POINTER	4
#define TYPE_INFO_KIND_FUNCTION			6
#define TYPE_INFO_KIND_CLASS			8
#define TYPE_INFO_KIND_MASK				0xFF
#define TYPE_INFO_POINTEE_CONST			0x100
#define TYPE_INFO_POINTEE_VOLATILE		0x200
#define TYPE_INFO_POINTEE_CV			0x300

#define TYPE_INFO_PUBLIC				0x01
#define TYPE_INFO_VIRTUAL				0x02
#define TYPE_INFO_IN_VIRTUAL_SHIFT		8
#define TYPE_INFO_IN_VIRTUAL_MASK		0xFF
#define TYPE_INFO_VTABLE_SHIFT			16

#define VTABLE_ENTRY_BYTES				4


struct STypeInfo
{
	const char*		szName;
	unsigned long	ulKind;
};


//After a pointer's header: what it points to (without its const and volatile, which are in the pointer's kind word).
//After a member pointer's: the member's type, then the class.
struct SPointerTypeInfo
{
	STypeInfo			sHeader;
	const STypeInfo*	psPointee;
	const STypeInfo*	psClass;
};


//After a class's header: an entry for each subobject of an object of the class, the object itself first, ended by a
//NULL psClass.  lOffset is where the subobject is in a complete object.
struct SClassEntry
{
	const STypeInfo*	psClass;
	long				lOffset;
	unsigned long		ulFlags;
};


//In front of every exception object.  The same exception can be in flight and being handled at once (a handler that
//rethrows it), so each list has a link of its own.
struct SException
{
	const STypeInfo*	psType;
	void				(*pfDestroy)(void*);
	SException*			psNextInFlight;
	SException*			psNextHandled;
	void*				pvCaught;		//What a handler of a pointer type is given, when it isn't the thrown pointer as it is.
	unsigned int		uiHandlers;		//How many handlers are in it (a rethrown one can be caught again while its first handler is left).
	bool				bInFlight;
	bool				bPooled;
};


//All the state there is.  It has an initialiser so that it is in the data, zero whether or not the start up clears
//the bss, as the exit lists' heads are.
struct SExceptions
{
	SException*				psInFlight;		//The newest first.
	SException*				psHandled;		//The newest first.
	unsigned int			uiUncaught;
	std::terminate_handler	pfTerminate;
	std::unexpected_handler	pfUnexpected;
	unsigned int			uiPoolUsed;		//A bit for each of the pool's blocks.
};


static SExceptions	gsExceptions = { NULL, NULL, 0, NULL, NULL, 0 };


//When malloc has nothing: enough to throw bad_alloc and a few more.
#define EXCEPTION_POOL_BLOCKS		4
#define EXCEPTION_POOL_BYTES		64

struct SExceptionPoolBlock
{
	SException	sHeader;
	char		acObject[EXCEPTION_POOL_BYTES];
};

static SExceptionPoolBlock	gasExceptionPool[EXCEPTION_POOL_BLOCKS];


static SException* ExceptionOf(void* pvObject)
{
	return (SException*)pvObject - 1;
}


static void* ObjectOf(SException* psException)
{
	return psException + 1;
}


extern "C" void __Terminate(void) noexcept
{
	std::terminate_handler	pfHandler;

	pfHandler = gsExceptions.pfTerminate;
	if (pfHandler)
	{
		//Called once: a handler that calls terminate again stops.
		gsExceptions.pfTerminate = NULL;
		pfHandler();
	}
	abort();
}


extern "C" void __Unexpected(void)
{
	if (gsExceptions.pfUnexpected)
	{
		gsExceptions.pfUnexpected();
	}
	__Terminate();
}


extern "C" void* __AllocateException(size_t uiBytes) noexcept
{
	SException*		psException;
	unsigned int	ui;

	psException = (SException*)malloc(sizeof(SException) + uiBytes);
	if (psException)
	{
		psException->bPooled = false;
		return ObjectOf(psException);
	}
	if (uiBytes <= EXCEPTION_POOL_BYTES)
	{
		for (ui = 0; ui < EXCEPTION_POOL_BLOCKS; ui++)
		{
			if (!(gsExceptions.uiPoolUsed & (1u << ui)))
			{
				gsExceptions.uiPoolUsed |= 1u << ui;
				psException = &gasExceptionPool[ui].sHeader;
				psException->bPooled = true;
				return ObjectOf(psException);
			}
		}
	}
	__Terminate();
	return NULL;
}


static void FreeException(SException* psException)
{
	unsigned int	ui;

	if (!psException->bPooled)
	{
		free(psException);
		return;
	}
	for (ui = 0; ui < EXCEPTION_POOL_BLOCKS; ui++)
	{
		if (psException == &gasExceptionPool[ui].sHeader)
		{
			gsExceptions.uiPoolUsed &= ~(1u << ui);
		}
	}
}


//For an object that was never thrown: its constructor threw.
extern "C" void __FreeException(void* pvObject) noexcept
{
	FreeException(ExceptionOf(pvObject));
}


extern "C" void __Throw(void* pvObject, const void* pvTypeInfo, void (*pfDestroy)(void*)) noexcept
{
	SException*	psException;

	psException = ExceptionOf(pvObject);
	psException->psType = (const STypeInfo*)pvTypeInfo;
	psException->pfDestroy = pfDestroy;
	psException->psNextHandled = NULL;
	psException->pvCaught = NULL;
	psException->uiHandlers = 0;
	psException->bInFlight = true;
	psException->psNextInFlight = gsExceptions.psInFlight;
	gsExceptions.psInFlight = psException;
	gsExceptions.uiUncaught++;
}


//A type's type info: what typeid gives is its address.  (Through a reference: the compiler doesn't take typeid's own
//address yet.)
static const STypeInfo* TypeInfoOf(const std::type_info& cType)
{
	return (const STypeInfo*)&cType;
}


static unsigned int KindOf(const STypeInfo* psType)
{
	return (unsigned int)(psType->ulKind & TYPE_INFO_KIND_MASK);
}


//The class's subobject entries.
static const SClassEntry* EntriesOf(const STypeInfo* psClass)
{
	return (const SClassEntry*)(psClass + 1);
}


//The one psBase subobject of a psClass object, reached through public bases only; NULL if there is none, more than one,
//or a base on the way isn't public.  [except.handle] 3: "an unambiguous public base class".
static const SClassEntry* PublicBaseOf(const STypeInfo* psClass, const STypeInfo* psBase)
{
	const SClassEntry*	psEntry;
	const SClassEntry*	psFound;

	psFound = NULL;
	for (psEntry = EntriesOf(psClass); psEntry->psClass; psEntry++)
	{
		if (psEntry->psClass == psBase)
		{
			if (psFound)
			{
				return NULL;
			}
			psFound = psEntry;
		}
	}
	return (psFound && (psFound->ulFlags & TYPE_INFO_PUBLIC)) ? psFound : NULL;
}


//Where the subobject psEntry names is in the psClass object at pvObject, which may itself be a base of a bigger
//object: then a virtual base isn't where the entry's offset (a complete object's) says, and the object's own vtable
//gives the distance to it, as the compiler's own conversion to a virtual base reads it.
static void* SubobjectOf(void* pvObject, const STypeInfo* psClass, const SClassEntry* psEntry)
{
	const SClassEntry*	psFirst;
	const SClassEntry*	psVirtual;
	unsigned int		uiVirtual;
	unsigned long		ulVtable;
	char*				pcVtablePointer;
	long*				plVtable;
	long				lDistance;

	uiVirtual = (unsigned int)((psEntry->ulFlags >> TYPE_INFO_IN_VIRTUAL_SHIFT) & TYPE_INFO_IN_VIRTUAL_MASK);
	if (uiVirtual == 0)
	{
		return (char*)pvObject + psEntry->lOffset;
	}

	//The virtual base it is, or is in: that one's entry has the same index and the flag.
	psFirst = EntriesOf(psClass);
	psVirtual = NULL;
	for (psVirtual = psFirst; psVirtual->psClass; psVirtual++)
	{
		if ((psVirtual->ulFlags & TYPE_INFO_VIRTUAL) && (((psVirtual->ulFlags >> TYPE_INFO_IN_VIRTUAL_SHIFT) & TYPE_INFO_IN_VIRTUAL_MASK) == uiVirtual))
		{
			break;
		}
	}
	ulVtable = psFirst->ulFlags >> TYPE_INFO_VTABLE_SHIFT;
	if (!psVirtual->psClass || (ulVtable == 0))
	{
		return (char*)pvObject + psEntry->lOffset;
	}

	//Before a vtable part's header (two words), the distances to the class's virtual bases, the first nearest.
	pcVtablePointer = (char*)pvObject + (long)(ulVtable - 1);
	plVtable = *(long**)pcVtablePointer;
	lDistance = plVtable[-2 - (long)uiVirtual];
	return (char*)pvObject + lDistance + (psEntry->lOffset - psVirtual->lOffset);
}


//[conv.qual]: a pointer to psThrown (with ulThrown's const and volatile) can be given to a pointer to psHandler (with
//ulHandler's) when each level has at least the thrown one's, and where a level differs every level above it (bConst:
//so far) is const.  The types under the levels must be one type.
static bool Qualifies(const STypeInfo* psThrown, unsigned long ulThrown, const STypeInfo* psHandler, unsigned long ulHandler, bool bConst)
{
	ulThrown &= TYPE_INFO_POINTEE_CV;
	ulHandler &= TYPE_INFO_POINTEE_CV;
	if ((ulThrown & ~ulHandler) != 0)
	{
		return false;
	}
	if ((ulThrown != ulHandler) && !bConst)
	{
		return false;
	}
	if (psThrown == psHandler)
	{
		return true;
	}
	if ((KindOf(psThrown) == TYPE_INFO_KIND_POINTER) && (KindOf(psHandler) == TYPE_INFO_KIND_POINTER))
	{
		return Qualifies(((const SPointerTypeInfo*)psThrown)->psPointee, psThrown->ulKind, ((const SPointerTypeInfo*)psHandler)->psPointee, psHandler->ulKind, bConst && ((ulHandler & TYPE_INFO_POINTEE_CONST) != 0));
	}
	if ((KindOf(psThrown) == TYPE_INFO_KIND_MEMBER_POINTER) && (KindOf(psHandler) == TYPE_INFO_KIND_MEMBER_POINTER) &&
		(((const SPointerTypeInfo*)psThrown)->psClass == ((const SPointerTypeInfo*)psHandler)->psClass))
	{
		return Qualifies(((const SPointerTypeInfo*)psThrown)->psPointee, psThrown->ulKind, ((const SPointerTypeInfo*)psHandler)->psPointee, psHandler->ulKind, bConst && ((ulHandler & TYPE_INFO_POINTEE_CONST) != 0));
	}
	return false;
}


//Whether a handler of psHandler's type takes the exception, and what its parameter is then to be made from: the
//thrown object, a base of it, or a pointer worked out here (kept in the exception: psException->pvCaught).  NULL if
//the handler doesn't take it.  [except.handle] 3
static void* Match(SException* psException, const STypeInfo* psHandler)
{
	const STypeInfo*		psThrown;
	const SPointerTypeInfo*	psThrownPointer;
	const SPointerTypeInfo*	psHandlerPointer;
	const SClassEntry*		psEntry;
	void*					pvObject;
	void*					pvPointed;
	unsigned int			uiThrown;
	unsigned int			uiHandler;

	psThrown = psException->psType;
	pvObject = ObjectOf(psException);
	if (psThrown == psHandler)
	{
		return pvObject;
	}
	uiThrown = KindOf(psThrown);
	uiHandler = KindOf(psHandler);

	//A handler of a base class.  The thrown object is complete, so the entry's offset is where the base is.
	if ((uiThrown == TYPE_INFO_KIND_CLASS) && (uiHandler == TYPE_INFO_KIND_CLASS))
	{
		psEntry = PublicBaseOf(psThrown, psHandler);
		return psEntry ? (char*)pvObject + psEntry->lOffset : NULL;
	}

	//nullptr, for a handler of any pointer or pointer to member.
	if ((uiThrown == TYPE_INFO_KIND_NULL_POINTER) && ((uiHandler == TYPE_INFO_KIND_POINTER) || (uiHandler == TYPE_INFO_KIND_MEMBER_POINTER)))
	{
		psException->pvCaught = NULL;
		return &psException->pvCaught;
	}

	if ((uiThrown == TYPE_INFO_KIND_MEMBER_POINTER) && (uiHandler == TYPE_INFO_KIND_MEMBER_POINTER))
	{
		psThrownPointer = (const SPointerTypeInfo*)psThrown;
		psHandlerPointer = (const SPointerTypeInfo*)psHandler;
		if ((psThrownPointer->psClass == psHandlerPointer->psClass) && Qualifies(psThrownPointer->psPointee, psThrown->ulKind, psHandlerPointer->psPointee, psHandler->ulKind, true))
		{
			return pvObject;
		}
		return NULL;
	}

	if ((uiThrown != TYPE_INFO_KIND_POINTER) || (uiHandler != TYPE_INFO_KIND_POINTER))
	{
		return NULL;
	}
	psThrownPointer = (const SPointerTypeInfo*)psThrown;
	psHandlerPointer = (const SPointerTypeInfo*)psHandler;

	//The same thing pointed to, with const or volatile added.
	if (Qualifies(psThrownPointer->psPointee, psThrown->ulKind, psHandlerPointer->psPointee, psHandler->ulKind, true))
	{
		return pvObject;
	}
	if ((psThrown->ulKind & TYPE_INFO_POINTEE_CV & ~psHandler->ulKind) != 0)
	{
		return NULL;
	}

	//A pointer to an object, for a handler of a pointer to void.
	if ((psHandlerPointer->psPointee == TypeInfoOf(typeid(void))) && (KindOf(psThrownPointer->psPointee) != TYPE_INFO_KIND_FUNCTION))
	{
		return pvObject;
	}

	//A pointer to a class, for a handler of a pointer to a base of it: the pointer is moved to the base, as the
	//conversion in the program would move it.
	if ((KindOf(psThrownPointer->psPointee) == TYPE_INFO_KIND_CLASS) && (KindOf(psHandlerPointer->psPointee) == TYPE_INFO_KIND_CLASS))
	{
		psEntry = PublicBaseOf(psThrownPointer->psPointee, psHandlerPointer->psPointee);
		if (!psEntry)
		{
			return NULL;
		}
		pvPointed = *(void**)pvObject;
		psException->pvCaught = pvPointed ? SubobjectOf(pvPointed, psThrownPointer->psPointee, psEntry) : NULL;
		return &psException->pvCaught;
	}
	return NULL;
}


//Asked of the newest exception in flight.
extern "C" void* __Match(const void* pvHandlerTypeInfo) noexcept
{
	if (!gsExceptions.psInFlight)
	{
		return NULL;
	}
	return Match(gsExceptions.psInFlight, (const STypeInfo*)pvHandlerTypeInfo);
}


//A handler has taken the newest exception in flight: it is being handled, until __EndCatch.  Gives the thrown object
//(for catch (...), which asked __Match nothing).
extern "C" void* __BeginCatch(void) noexcept
{
	SException*	psException;

	psException = gsExceptions.psInFlight;
	if (!psException)
	{
		__Terminate();
		return NULL;
	}
	gsExceptions.psInFlight = psException->psNextInFlight;
	psException->bInFlight = false;
	gsExceptions.uiUncaught--;
	if (psException->uiHandlers == 0)
	{
		psException->psNextHandled = gsExceptions.psHandled;
		gsExceptions.psHandled = psException;
	}
	psException->uiHandlers++;
	return ObjectOf(psException);
}


//The newest handler is left (by its end, or by an exception on its way out, its own again if it rethrew).  Its
//exception is destroyed when no handler is in it and it isn't in flight again.
extern "C" void __EndCatch(void) noexcept
{
	SException*		psException;
	SException**	ppsLink;

	//A rethrown exception caught again in a handler nested in its first is entered twice: the list has it once.
	psException = gsExceptions.psHandled;
	if (!psException)
	{
		__Terminate();
		return;
	}
	psException->uiHandlers--;
	if (psException->uiHandlers != 0)
	{
		return;
	}
	for (ppsLink = &gsExceptions.psHandled; *ppsLink; ppsLink = &(*ppsLink)->psNextHandled)
	{
		if (*ppsLink == psException)
		{
			*ppsLink = psException->psNextHandled;
			break;
		}
	}
	if (!psException->bInFlight)
	{
		if (psException->pfDestroy)
		{
			psException->pfDestroy(ObjectOf(psException));
		}
		FreeException(psException);
	}
}


//throw; with no operand: the exception being handled is in flight again.  With none, terminate.  [except.throw] 9
extern "C" void __Rethrow(void) noexcept
{
	SException*	psException;

	psException = gsExceptions.psHandled;
	if (!psException || psException->bInFlight)
	{
		__Terminate();
		return;
	}
	psException->bInFlight = true;
	psException->psNextInFlight = gsExceptions.psInFlight;
	gsExceptions.psInFlight = psException;
	gsExceptions.uiUncaught++;
}


namespace std
{
	exception::exception(void) noexcept {}
	exception::exception(const exception& cOther) noexcept {}
	exception& exception::operator=(const exception& cOther) noexcept { return *this; }
	exception::~exception(void) {}
	const char* exception::what(void) const noexcept { return "std::exception"; }

	bad_exception::bad_exception(void) noexcept {}
	bad_exception::~bad_exception(void) {}
	const char* bad_exception::what(void) const noexcept { return "std::bad_exception"; }

	bad_cast::bad_cast(void) noexcept {}
	bad_cast::~bad_cast(void) {}
	const char* bad_cast::what(void) const noexcept { return "std::bad_cast"; }

	bad_typeid::bad_typeid(void) noexcept {}
	bad_typeid::~bad_typeid(void) {}
	const char* bad_typeid::what(void) const noexcept { return "std::bad_typeid"; }


	terminate_handler set_terminate(terminate_handler pfHandler) noexcept
	{
		terminate_handler	pfOld;

		pfOld = gsExceptions.pfTerminate;
		gsExceptions.pfTerminate = pfHandler;
		return pfOld;
	}


	terminate_handler get_terminate(void) noexcept
	{
		return gsExceptions.pfTerminate;
	}


	void terminate(void) noexcept
	{
		__Terminate();
		abort();
	}


	unexpected_handler set_unexpected(unexpected_handler pfHandler) noexcept
	{
		unexpected_handler	pfOld;

		pfOld = gsExceptions.pfUnexpected;
		gsExceptions.pfUnexpected = pfHandler;
		return pfOld;
	}


	unexpected_handler get_unexpected(void) noexcept
	{
		return gsExceptions.pfUnexpected;
	}


	void unexpected(void)
	{
		__Unexpected();
		abort();
	}


	bool uncaught_exception(void) noexcept
	{
		return gsExceptions.uiUncaught != 0;
	}
}
