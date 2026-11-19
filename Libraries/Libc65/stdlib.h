//Memory allocation and general functions.
#ifndef __LIBC65_STDLIB_H__
#define __LIBC65_STDLIB_H__
#include <stddef.h>


#define EXIT_SUCCESS	0
#define EXIT_FAILURE	1
#define RAND_MAX		0x7fff


extern "C"
{
void*	malloc(size_t uiBytes) noexcept;
void*	calloc(size_t uiCount, size_t uiBytes) noexcept;
void*	realloc(void* pv, size_t uiBytes) noexcept;
void	free(void* pv) noexcept;
#if defined(__W65C816__)
//32 bit sizes, for blocks of more than 64KB: those have whole banks of their own (see stdlib.cpp).  free and realloc
//take either kind of block.
void*	malloc32(unsigned long ulBytes) noexcept;
void*	calloc32(unsigned long ulCount, unsigned long ulBytes) noexcept;
void*	realloc32(void* pv, unsigned long ulBytes) noexcept;
#endif // __W65C816__

int		abs(int i) noexcept;
long	labs(long i) noexcept;
int		atoi(const char* sz) noexcept;
long	atol(const char* sz) noexcept;
long	strtol(const char* sz, char** pszEnd, int iBase) noexcept;
unsigned long	strtoul(const char* sz, char** pszEnd, int iBase) noexcept;
long long	strtoll(const char* sz, char** pszEnd, int iBase) noexcept;
unsigned long long	strtoull(const char* sz, char** pszEnd, int iBase) noexcept;
double	strtod(const char* sz, char** pszEnd) noexcept;
float	strtof(const char* sz, char** pszEnd) noexcept;
long double	strtold(const char* sz, char** pszEnd) noexcept;
double	atof(const char* sz) noexcept;

int		rand(void) noexcept;
void	srand(unsigned int uiSeed) noexcept;
void	qsort(void* pvBase, size_t uiCount, size_t uiBytes, int (*fCompare)(const void*, const void*));

void	abort(void) noexcept;
int		atexit(void (*fFunction)(void)) noexcept;
void	exit(int iStatus) noexcept;
void	_Exit(int iStatus) noexcept;
}


#endif // __LIBC65_STDLIB_H__
