//Memory allocation and general functions for the 65C64, in C++ for the 65C64 generator.
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include "BaseLib/TrueMallocator.h"


int		errno = 0;


//The heap is 0008:0000 to 000F:FFFF, or from 0020:0000 (or the end of a program that goes past it) to the top of
//memory when the memory size register (00FF:0300) says there is more than 2MB, or from 0100:0000 (past the devices'
//page) when there is more than 16MB.
//BaseLib's CTrueMallocator manages it: malloc and free take the same time however many blocks there are, and freed
//blocks are joined to the free blocks either side of them.  On the W65C816 it is by banks (below).
#define HEAP_START			0x80000
#define HEAP_END			0x100000
#define HIGH_HEAP_START		0x200000
#define DEVICES_START		0xFF0000		//The devices' page, 00FF:0000 to 00FF:FFFF, is never memory.
#define DEVICES_END			0x1000000
#define MEMORY_SIZE			(*(volatile uint32_t*)0x00FF0300)


CTrueMallocator		gcHeap;
bool				gbHeap = false;

//The linker's __end: the end of the program.
extern "C" char		_end;


//The start up's InitHeap and the kernal share one copy (the kernal is in ROM).
extern "C" void __HeapInit(void* pvStart, uint32_t uiBytes) __attribute__((noinline));


static void InitHeap(void)
{
	uint32_t	uiMemorySize;
	uint32_t	uiStart;
	uint32_t	uiEnd;

	uiMemorySize = MEMORY_SIZE;
	if (uiMemorySize > DEVICES_END)
	{
		uiStart = DEVICES_END;
		uiEnd = uiMemorySize;
	}
	else if (uiMemorySize > HIGH_HEAP_START)
	{
		//After the program if it goes past 0020:0000.
		uiStart = (uint32_t)&_end;
		uiStart = (uiStart > HIGH_HEAP_START) ? ((uiStart + 15) & ~(uint32_t)15) : HIGH_HEAP_START;
		uiEnd = (uiMemorySize < DEVICES_START) ? uiMemorySize : DEVICES_START;
	}
	else
	{
		uiStart = HEAP_START;
		uiEnd = HEAP_END;
	}
	__HeapInit((void*)uiStart, uiEnd - uiStart);
}


#if defined(__W65C816__)
//On the W65C816 the heap is by banks (CppCompilerLib/Size16Plan.txt), so no block crosses a 64KB bank: an index
//register reaches all of one from its start.  gcHeap has a region for each bank of the small heap, taken from the
//bottom as it fills and given back when nothing is left in it (but the first, which has the maps).  What doesn't fit a
//bank has whole adjacent banks of its own, taken from the top.  A byte for each bank says which it is, and a count for
//each of the small heap's banks how many blocks are in it, so free knows what a pointer is and when a bank is empty.
#define HEAP_BANK_SIZE		0x10000
#define HEAP_SMALL_MAX		(HEAP_BANK_SIZE - 64)	//The most an empty whole bank holds, less its region's and block's headers.
#define HEAP_BANK_FREE		0
#define HEAP_BANK_SMALL		1		//A region of gcHeap.
#define HEAP_BANK_KEPT		2		//The first region of gcHeap, where the maps are: never given back.
#define HEAP_BANK_LARGE		3		//The first of a large allocation's banks.
#define HEAP_BANK_MORE		4		//One of a large allocation's banks after its first.

//Large allocations are copied with the 32 bit memcpy.
#define HEAP_COPY(pvDest, pvSource, uiBytes)	memcpy32(pvDest, pvSource, uiBytes)

//malloc, calloc, realloc and free share one copy of each (the kernal is in ROM).
static void*		HeapMalloc(uint32_t uiBytes) __attribute__((noinline));
static void			HeapFree(void* pv) __attribute__((noinline));

static uint8_t*		gpuiHeapBanks;		//What each bank is.
static uint16_t*	gpuiHeapBlocks;		//How many of gcHeap's blocks each bank has.
static uint32_t		guiHeapStart;		//The heap's memory, after the maps.
static uint32_t		guiHeapEnd;
static uint16_t		guiHeapFirstBank;	//The bank of the heap's first byte (the maps').
static uint16_t		guiHeapBanks;


//The bank (from the heap's first) pv is in.
static uint16_t HeapBank(void* pv)
{
	return (uint16_t)(((uint32_t)pv >> 16) - guiHeapFirstBank);
}


//Where bank uiBank's memory starts.
static uint32_t HeapBankAddress(uint16_t uiBank)
{
	return (uint32_t)(guiHeapFirstBank + uiBank) << 16;
}


//The part of bank uiBank that is the heap's (all of it but in the first and last).
static uint32_t HeapBankStart(uint16_t uiBank)
{
	uint32_t	uiStart;

	uiStart = HeapBankAddress(uiBank);
	return (uiStart < guiHeapStart) ? guiHeapStart : uiStart;
}


static uint32_t HeapBankEnd(uint16_t uiBank)
{
	uint32_t	uiEnd;

	uiEnd = HeapBankAddress(uiBank) + HEAP_BANK_SIZE;
	return (uiEnd > guiHeapEnd) ? guiHeapEnd : uiEnd;
}


//Whether bank uiBank is free and all the heap's, as a large allocation's must be.
static bool HeapIsWholeFree(uint16_t uiBank)
{
	return (gpuiHeapBanks[uiBank] == HEAP_BANK_FREE) && (HeapBankAddress(uiBank) >= guiHeapStart) && (HeapBankAddress(uiBank) + HEAP_BANK_SIZE <= guiHeapEnd);
}


//Bank uiBank becomes a region of gcHeap.
static bool HeapAddBank(uint16_t uiBank, uint8_t uiKind)
{
	if (!gcHeap.AddRegion((void*)HeapBankStart(uiBank), HeapBankEnd(uiBank) - HeapBankStart(uiBank)))
	{
		return false;
	}
	gpuiHeapBanks[uiBank] = uiKind;
	return true;
}


//A block left bank uiBank: the bank goes back if it was the last.
static void HeapBlockGone(uint16_t uiBank)
{
	gpuiHeapBlocks[uiBank]--;
	if ((gpuiHeapBlocks[uiBank] == 0) && (gpuiHeapBanks[uiBank] == HEAP_BANK_SMALL) && gcHeap.RemoveRegion((void*)HeapBankStart(uiBank)))
	{
		gpuiHeapBanks[uiBank] = HEAP_BANK_FREE;
	}
}


//uiBytes in one bank: from the small heap's banks, or the lowest free bank given to it.
static void* HeapMallocSmall(uint32_t uiBytes)
{
	void*		pv;
	uint16_t	uiBank;

	pv = gcHeap.Malloc(uiBytes);
	for (uiBank = 0; !pv && (uiBank < guiHeapBanks); uiBank++)
	{
		if ((gpuiHeapBanks[uiBank] == HEAP_BANK_FREE) && HeapAddBank(uiBank, HEAP_BANK_SMALL))
		{
			pv = gcHeap.Malloc(uiBytes);
			if (!pv && gcHeap.RemoveRegion((void*)HeapBankStart(uiBank)))
			{
				//A part bank at the heap's end too small for it.
				gpuiHeapBanks[uiBank] = HEAP_BANK_FREE;
			}
		}
	}
	if (pv)
	{
		gpuiHeapBlocks[HeapBank(pv)]++;
	}
	return pv;
}


//uiBytes in the highest run of whole free banks that holds them.
static void* HeapMallocLarge(uint32_t uiBytes)
{
	uint32_t	uiBanks;
	uint16_t	uiCount;
	uint16_t	uiRun;
	uint16_t	uiBank;
	uint16_t	i;

	//Without the wrap of uiBytes + 0xFFFF near 4GB.
	uiBanks = (uiBytes >> 16) + ((uiBytes & (HEAP_BANK_SIZE - 1)) != 0);
	if (uiBanks > guiHeapBanks)
	{
		return NULL;
	}
	uiCount = (uint16_t)uiBanks;
	uiRun = 0;
	for (uiBank = guiHeapBanks; uiBank > 0; uiBank--)
	{
		uiRun = HeapIsWholeFree((uint16_t)(uiBank - 1)) ? uiRun + 1 : 0;
		if (uiRun == uiCount)
		{
			gpuiHeapBanks[uiBank - 1] = HEAP_BANK_LARGE;
			for (i = 1; i < uiCount; i++)
			{
				gpuiHeapBanks[uiBank - 1 + i] = HEAP_BANK_MORE;
			}
			return (void*)HeapBankAddress((uint16_t)(uiBank - 1));
		}
	}
	return NULL;
}


//The bytes a large allocation's banks hold.
static uint32_t HeapLargeBytes(uint16_t uiBank)
{
	uint32_t	uiBytes;

	for (uiBytes = HEAP_BANK_SIZE, uiBank++; (uiBank < guiHeapBanks) && (gpuiHeapBanks[uiBank] == HEAP_BANK_MORE); uiBank++)
	{
		uiBytes += HEAP_BANK_SIZE;
	}
	return uiBytes;
}


//The heap over uiBytes from pvStart (Libc65's start up, and the kernal's for MPU RAM): the maps first, then the
//first bank's room is the first region.
extern "C" void __HeapInit(void* pvStart, uint32_t uiBytes)
{
	uint32_t	uiStart;
	uint16_t	i;

	uiStart = (uint32_t)pvStart;
	guiHeapFirstBank = (uint16_t)(uiStart >> 16);
	guiHeapBanks = (uint16_t)(((uiStart + uiBytes - 1) >> 16) - guiHeapFirstBank + 1);
	gpuiHeapBanks = (uint8_t*)uiStart;
	gpuiHeapBlocks = (uint16_t*)((uiStart + guiHeapBanks + 1) & ~(uint32_t)1);
	guiHeapStart = ((uint32_t)(gpuiHeapBlocks + guiHeapBanks) + 15) & ~(uint32_t)15;
	guiHeapEnd = uiStart + uiBytes;
	for (i = 0; i < guiHeapBanks; i++)
	{
		gpuiHeapBanks[i] = HEAP_BANK_FREE;
		gpuiHeapBlocks[i] = 0;
	}
	gcHeap.Init();
	gbHeap = true;
	HeapAddBank(0, HEAP_BANK_KEPT);
}


static void* HeapMalloc(uint32_t uiBytes)
{
	if (!gbHeap)
	{
		InitHeap();
	}
	return (uiBytes <= HEAP_SMALL_MAX) ? HeapMallocSmall(uiBytes) : HeapMallocLarge(uiBytes);
}


static void HeapFree(void* pv)
{
	uint16_t	uiBank;

	if (!pv)
	{
		return;
	}
	uiBank = HeapBank(pv);
	if (gpuiHeapBanks[uiBank] == HEAP_BANK_LARGE)
	{
		for (gpuiHeapBanks[uiBank++] = HEAP_BANK_FREE; (uiBank < guiHeapBanks) && (gpuiHeapBanks[uiBank] == HEAP_BANK_MORE); uiBank++)
		{
			gpuiHeapBanks[uiBank] = HEAP_BANK_FREE;
		}
		return;
	}
	gcHeap.Free(pv);
	HeapBlockGone(uiBank);
}


//A small block grows or shrinks where it is or in the small heap if it can; anything else is copied to a new
//allocation.  A large one that still needs more than a bank and fits its banks stays.
static void* HeapRealloc(void* pv, uint32_t uiBytes)
{
	uint16_t	uiBank;
	uint32_t	uiOld;
	void*		pvNew;

	if (!pv)
	{
		return HeapMalloc(uiBytes);
	}
	uiBank = HeapBank(pv);
	if (gpuiHeapBanks[uiBank] == HEAP_BANK_LARGE)
	{
		uiOld = HeapLargeBytes(uiBank);
		if ((uiBytes > HEAP_SMALL_MAX) && (uiBytes <= uiOld))
		{
			return pv;
		}
	}
	else
	{
		uiOld = gcHeap.Size(pv);
		if (uiBytes <= HEAP_SMALL_MAX)
		{
			pvNew = gcHeap.Realloc(pv, uiBytes);
			if (pvNew && (pvNew != pv))
			{
				gpuiHeapBlocks[HeapBank(pvNew)]++;
				HeapBlockGone(uiBank);
			}
			if (pvNew)
			{
				return pvNew;
			}
		}
	}
	pvNew = HeapMalloc(uiBytes);
	if (pvNew)
	{
		HEAP_COPY(pvNew, pv, (uiOld < uiBytes) ? uiOld : uiBytes);
		HeapFree(pv);
	}
	return pvNew;
}


void* malloc32(unsigned long ulBytes) noexcept
{
	return HeapMalloc(ulBytes);
}


void* calloc32(unsigned long ulCount, unsigned long ulBytes) noexcept
{
	void*	pv;

	//ulCount * ulBytes must not wrap.
	if ((ulBytes != 0) && (ulCount > 0xFFFFFFFFUL / ulBytes))
	{
		return NULL;
	}
	pv = HeapMalloc(ulCount * ulBytes);
	if (pv)
	{
		memset32(pv, 0, ulCount * ulBytes);
	}
	return pv;
}


void* realloc32(void* pv, unsigned long ulBytes) noexcept
{
	return HeapRealloc(pv, ulBytes);
}


#else
extern "C" void __HeapInit(void* pvStart, uint32_t uiBytes)
{
	gcHeap.Init(pvStart, uiBytes);
	gbHeap = true;
}


static void* HeapMalloc(uint32_t uiBytes)
{
	if (!gbHeap)
	{
		InitHeap();
	}
	return gcHeap.Malloc(uiBytes);
}


static void HeapFree(void* pv)
{
	gcHeap.Free(pv);
}


static void* HeapRealloc(void* pv, uint32_t uiBytes)
{
	if (!gbHeap)
	{
		InitHeap();
	}
	return gcHeap.Realloc(pv, uiBytes);
}
#endif // __W65C816__


void* malloc(size_t uiBytes) noexcept
{
	return HeapMalloc(uiBytes);
}


void free(void* pv) noexcept
{
	HeapFree(pv);
}


void* calloc(size_t uiCount, size_t uiBytes) noexcept
{
	void*	pv;

	//uiCount * uiBytes must not wrap.
	if ((uiBytes != 0) && (uiCount > (size_t)-1 / uiBytes))
	{
		return NULL;
	}
	pv = malloc(uiCount * uiBytes);
	if (pv)
	{
		memset(pv, 0, uiCount * uiBytes);
	}
	return pv;
}


void* realloc(void* pv, size_t uiBytes) noexcept
{
	return HeapRealloc(pv, uiBytes);
}


int abs(int i) noexcept
{
	return (i < 0) ? -i : i;
}


long labs(long i) noexcept
{
	return (i < 0) ? -i : i;
}


//The value of c as a digit, or 36 if it isn't one in any base.
static int DigitValue(char c)
{
	if (isdigit(c))
	{
		return c - '0';
	}
	if (islower(c))
	{
		return c - 'a' + 10;
	}
	if (isupper(c))
	{
		return c - 'A' + 10;
	}
	return 36;
}


//Reads the sign and magnitude of a number for strtol and strtoul.  *pbOverflow is set if the magnitude doesn't
//fit in an unsigned long.  Without any digits *pszEnd is sz and false is returned.
//Parses an integer of at most ullMaximum (its sign is in *pbNegative).  *pbOverflow is set if it is bigger.
static bool ParseInteger(const char* sz, char** pszEnd, int iBase, unsigned long long ullMaximum, unsigned long long* pullValue, bool* pbNegative, bool* pbOverflow)
{
	const char*			szStart;
	unsigned long long	ullValue;
	unsigned long long	ullLimit;
	int					iLimitDigit;
	int					iDigit;
	bool				bDigits;

	szStart = sz;
	while (isspace(*sz))
	{
		sz++;
	}
	*pbNegative = false;
	if ((*sz == '-') || (*sz == '+'))
	{
		*pbNegative = *sz == '-';
		sz++;
	}

	//0x is only a prefix if a hex digit follows it; otherwise the 0 is the number.
	if (((iBase == 0) || (iBase == 16)) && (sz[0] == '0') && ((sz[1] == 'x') || (sz[1] == 'X')) && (DigitValue(sz[2]) < 16))
	{
		iBase = 16;
		sz += 2;
	}
	else if (iBase == 0)
	{
		iBase = (sz[0] == '0') ? 8 : 10;
	}

	ullLimit = ullMaximum / (unsigned)iBase;
	iLimitDigit = (int)(ullMaximum % (unsigned)iBase);
	ullValue = 0;
	bDigits = false;
	*pbOverflow = false;
	for (;;)
	{
		iDigit = DigitValue(*sz);
		if (iDigit >= iBase)
		{
			break;
		}
		if ((ullValue > ullLimit) || ((ullValue == ullLimit) && (iDigit > iLimitDigit)))
		{
			*pbOverflow = true;
		}
		else
		{
			ullValue = ullValue * (unsigned)iBase + (unsigned)iDigit;
		}
		bDigits = true;
		sz++;
	}

	if (pszEnd)
	{
		*pszEnd = (char*)(bDigits ? sz : szStart);
	}
	*pullValue = ullValue;
	return bDigits;
}


//Out of range values are LONG_MIN or LONG_MAX, and errno is ERANGE.
long strtol(const char* sz, char** pszEnd, int iBase) noexcept
{
	unsigned long long	ullValue;
	bool				bNegative;
	bool				bOverflow;

	if (!ParseInteger(sz, pszEnd, iBase, ULONG_MAX, &ullValue, &bNegative, &bOverflow))
	{
		return 0;
	}
	if (bNegative)
	{
		if (bOverflow || (ullValue > (unsigned long)LONG_MAX + 1))
		{
			errno = ERANGE;
			return LONG_MIN;
		}
		return (long)((unsigned long)0 - (unsigned long)ullValue);
	}
	if (bOverflow || (ullValue > (unsigned long)LONG_MAX))
	{
		errno = ERANGE;
		return LONG_MAX;
	}
	return (long)ullValue;
}


//Out of range values are ULONG_MAX, and errno is ERANGE.  A negative number is negated as an unsigned long.
unsigned long strtoul(const char* sz, char** pszEnd, int iBase) noexcept
{
	unsigned long long	ullValue;
	bool				bNegative;
	bool				bOverflow;

	if (!ParseInteger(sz, pszEnd, iBase, ULONG_MAX, &ullValue, &bNegative, &bOverflow))
	{
		return 0;
	}
	if (bOverflow)
	{
		errno = ERANGE;
		return ULONG_MAX;
	}
	return bNegative ? (unsigned long)0 - (unsigned long)ullValue : (unsigned long)ullValue;
}


//Out of range values are LLONG_MIN or LLONG_MAX, and errno is ERANGE.
long long strtoll(const char* sz, char** pszEnd, int iBase) noexcept
{
	unsigned long long	ullValue;
	bool				bNegative;
	bool				bOverflow;

	if (!ParseInteger(sz, pszEnd, iBase, ULLONG_MAX, &ullValue, &bNegative, &bOverflow))
	{
		return 0;
	}
	if (bNegative)
	{
		if (bOverflow || (ullValue > (unsigned long long)LLONG_MAX + 1))
		{
			errno = ERANGE;
			return LLONG_MIN;
		}
		return (long long)((unsigned long long)0 - ullValue);
	}
	if (bOverflow || (ullValue > (unsigned long long)LLONG_MAX))
	{
		errno = ERANGE;
		return LLONG_MAX;
	}
	return (long long)ullValue;
}


//Out of range values are ULLONG_MAX, and errno is ERANGE.  A negative number is negated as an unsigned long long.
unsigned long long strtoull(const char* sz, char** pszEnd, int iBase) noexcept
{
	unsigned long long	ullValue;
	bool				bNegative;
	bool				bOverflow;

	if (!ParseInteger(sz, pszEnd, iBase, ULLONG_MAX, &ullValue, &bNegative, &bOverflow))
	{
		return 0;
	}
	if (bOverflow)
	{
		errno = ERANGE;
		return ULLONG_MAX;
	}
	return bNegative ? (unsigned long long)0 - ullValue : ullValue;
}


static bool MatchIgnoreCase(const char* sz, const char* szWord)
{
	while (*szWord)
	{
		if (tolower((unsigned char)*sz) != *szWord)
		{
			return false;
		}
		sz++;
		szWord++;
	}
	return true;
}


//Decimal (with an exponent), inf, infinity and nan.  Up to 19 significant digits are kept and scaled by powers of
//ten, so the result is within an ulp or so but not always correctly rounded.  Out of range values are HUGE_VAL (or
//0) with errno ERANGE.
double strtod(const char* sz, char** pszEnd) noexcept
{
	const char*			szStart;
	const char*			szExponent;
	unsigned long long	ullDigits;
	int					iDigits;
	int					iExponent;
	int					iExponentValue;
	bool				bNegative;
	bool				bNegativeExponent;
	bool				bAny;
	double				dValue;
	double				dScale;
	double				dPower;
	int					iPower;

	szStart = sz;
	while (isspace((unsigned char)*sz))
	{
		sz++;
	}
	bNegative = false;
	if ((*sz == '-') || (*sz == '+'))
	{
		bNegative = *sz == '-';
		sz++;
	}

	if (MatchIgnoreCase(sz, "inf"))
	{
		sz += MatchIgnoreCase(sz, "infinity") ? 8 : 3;
		if (pszEnd)
		{
			*pszEnd = (char*)sz;
		}
		return bNegative ? -__builtin_huge_val() : __builtin_huge_val();
	}
	if (MatchIgnoreCase(sz, "nan"))
	{
		if (pszEnd)
		{
			*pszEnd = (char*)(sz + 3);
		}
		ullDigits = 0x7FF8000000000000ULL;
		memcpy(&dValue, &ullDigits, sizeof(double));
		return dValue;
	}

	ullDigits = 0;
	iDigits = 0;
	iExponent = 0;
	bAny = false;
	while ((*sz >= '0') && (*sz <= '9'))
	{
		if (iDigits < 19)
		{
			ullDigits = ullDigits * 10 + (unsigned)(*sz - '0');
			if (ullDigits != 0)
			{
				iDigits++;
			}
		}
		else
		{
			iExponent++;
		}
		bAny = true;
		sz++;
	}
	if (*sz == '.')
	{
		sz++;
		while ((*sz >= '0') && (*sz <= '9'))
		{
			if (iDigits < 19)
			{
				ullDigits = ullDigits * 10 + (unsigned)(*sz - '0');
				if (ullDigits != 0)
				{
					iDigits++;
				}
				iExponent--;
			}
			bAny = true;
			sz++;
		}
	}
	if (!bAny)
	{
		if (pszEnd)
		{
			*pszEnd = (char*)szStart;
		}
		return 0.0;
	}

	if ((*sz == 'e') || (*sz == 'E'))
	{
		szExponent = sz;
		sz++;
		bNegativeExponent = false;
		if ((*sz == '-') || (*sz == '+'))
		{
			bNegativeExponent = *sz == '-';
			sz++;
		}
		if ((*sz >= '0') && (*sz <= '9'))
		{
			iExponentValue = 0;
			while ((*sz >= '0') && (*sz <= '9'))
			{
				if (iExponentValue < 10000)
				{
					iExponentValue = iExponentValue * 10 + (*sz - '0');
				}
				sz++;
			}
			iExponent += bNegativeExponent ? -iExponentValue : iExponentValue;
		}
		else
		{
			sz = szExponent;
		}
	}
	if (pszEnd)
	{
		*pszEnd = (char*)sz;
	}

	dValue = (double)ullDigits;
	if (dValue != 0.0)
	{
		//10^|exponent| by squaring, then one multiply or divide.
		iPower = (iExponent < 0) ? -iExponent : iExponent;
		dScale = 1.0;
		dPower = 10.0;
		while (iPower > 0)
		{
			if (iPower & 1)
			{
				dScale *= dPower;
			}
			dPower *= dPower;
			iPower >>= 1;
		}
		if (iExponent < 0)
		{
			//Very small values are divided in two steps so the scale doesn't overflow first.
			if (iExponent < -300)
			{
				dValue /= 1e300;
				dScale /= 1e300;
			}
			dValue /= dScale;
		}
		else
		{
			dValue *= dScale;
		}
		if ((dValue == 0.0) || (dValue > 1.7976931348623157e308))
		{
			errno = ERANGE;
		}
	}
	return bNegative ? -dValue : dValue;
}


float strtof(const char* sz, char** pszEnd) noexcept
{
	return (float)strtod(sz, pszEnd);
}


long double strtold(const char* sz, char** pszEnd) noexcept
{
	return strtod(sz, pszEnd);
}


double atof(const char* sz) noexcept
{
	return strtod(sz, NULL);
}


int atoi(const char* sz) noexcept
{
	return (int)strtol(sz, NULL, 10);
}


long atol(const char* sz) noexcept
{
	return strtol(sz, NULL, 10);
}


//A linear congruential generator, as the C standard's example (with 32 bits of state where int is 16 bits).
uint32_t	guiRandom = 1;


int rand(void) noexcept
{
	guiRandom = guiRandom * 1103515245UL + 12345;
	return (int)((guiRandom / 65536) % ((uint32_t)RAND_MAX + 1));
}


void srand(unsigned int uiSeed) noexcept
{
	guiRandom = uiSeed;
}


//An insertion sort.
void qsort(void* pvBase, size_t uiCount, size_t uiBytes, int (*fCompare)(const void*, const void*))
{
	char*	pcBase;
	char*	pcKey;
	char*	pcBefore;
	size_t	i;
	size_t	j;
	size_t	k;
	char	c;

	//The element before is a pointer of its own: k - uiBytes, unsigned, would be k + 65536 - uiBytes on the W65C816
	//(a 16 bit size_t and 32 bit pointers).
	pcBase = (char*)pvBase;
	for (i = 1; i < uiCount; i++)
	{
		for (j = i; j > 0; j--)
		{
			pcKey = pcBase + j * uiBytes;
			pcBefore = pcKey - uiBytes;
			if (fCompare(pcBefore, pcKey) <= 0)
			{
				break;
			}
			for (k = 0; k < uiBytes; k++)
			{
				c = pcKey[k];
				pcKey[k] = pcBefore[k];
				pcBefore[k] = c;
			}
		}
	}
}


//Stops at once, unsuccessfully: neither the atexit functions nor the destructors of globals run.
void abort(void) noexcept
{
	_Exit(EXIT_FAILURE);
}


//The start up's exit lists: what is done when the program ends, last pushed first.  An object made at start up or a
//static local is pushed when it has been made (the compiler's CCPP65IRFrontEnd::PushExitNode), and a function atexit
//registers when it is registered, so they interleave; thread_local objects have their own list, run first.
//[basic.start.term] 1, 3
struct SExitNode
{
	SExitNode*	psNext;
	void		(*pfFunction)(void);
};
extern "C" SExitNode*	_ExitList;
extern "C" SExitNode*	_ThreadExitList;


//The nodes of the functions atexit registered.
#define ATEXIT_MAX		32
static SExitNode	gasAtExit[ATEXIT_MAX];
static int			giAtExits;


//Registers fFunction to be called by exit.  Returns -1 when ATEXIT_MAX are registered already.
int atexit(void (*fFunction)(void)) noexcept
{
	SExitNode*	psNode;

	if (giAtExits == ATEXIT_MAX)
	{
		return -1;
	}
	psNode = &gasAtExit[giAtExits];
	giAtExits++;
	psNode->pfFunction = fFunction;
	psNode->psNext = _ExitList;
	_ExitList = psNode;
	return 0;
}


//Each node on the list, last pushed first, taken off before its function is called (which may push more).
static void RunExitList(SExitNode** ppsList)
{
	SExitNode*	psNode;

	while (*ppsList)
	{
		psNode = *ppsList;
		*ppsList = psNode->psNext;
		psNode->pfFunction();
	}
}


//The thread_local objects' exit list, then the other: the destructors of what was made at start up and of static
//locals, and the atexit functions, last first.  The start up calls it (when stdlib.cpp is linked) after main returns,
//before the fini table.
extern "C" void __RunAtExit(void);
void __RunAtExit(void)
{
	RunExitList(&_ThreadExitList);
	RunExitList(&_ExitList);
}


//The linker's fini table: each global's R_Finalise, in link order.  _Exit is in the start up.
typedef void (*FFinalise)(void);
extern "C" char	_fini_start;
extern "C" char	_fini_end;


//Runs the exit lists (the atexit functions and the destructors of what was made at start up), then the fini table
//(the destructors of globals made in the data) backwards, as returning from main does, then stops.
void exit(int iStatus) noexcept
{
	FFinalise*	pfFinalise;

	__RunAtExit();
	pfFinalise = (FFinalise*)&_fini_end;
	while (pfFinalise != (FFinalise*)&_fini_start)
	{
		pfFinalise--;
		(*pfFinalise)();
	}
	_Exit(iStatus);
}
