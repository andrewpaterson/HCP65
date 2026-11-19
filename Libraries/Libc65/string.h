//Memory and string functions.
#ifndef __LIBC65_STRING_H__
#define __LIBC65_STRING_H__
#include <stddef.h>


extern "C"
{
void*	memcpy(void* pvDest, const void* pvSource, size_t uiBytes) noexcept;
void*	memmove(void* pvDest, const void* pvSource, size_t uiBytes) noexcept;
void*	memset(void* pvDest, int iValue, size_t uiBytes) noexcept;
int		memcmp(const void* pv1, const void* pv2, size_t uiBytes) noexcept;
void*	memchr(const void* pv, int iValue, size_t uiBytes) noexcept;

size_t	strlen(const char* sz) noexcept;
char*	strcpy(char* szDest, const char* szSource) noexcept;
char*	strncpy(char* szDest, const char* szSource, size_t uiChars) noexcept;
char*	strcat(char* szDest, const char* szSource) noexcept;
char*	strncat(char* szDest, const char* szSource, size_t uiChars) noexcept;
int		strcmp(const char* sz1, const char* sz2) noexcept;
int		strncmp(const char* sz1, const char* sz2, size_t uiChars) noexcept;
char*	strchr(const char* sz, int c) noexcept;
char*	strrchr(const char* sz, int c) noexcept;
char*	strstr(const char* sz, const char* szFind) noexcept;

#if defined(__W65C816__)
//size_t is 16 bits on the W65C816 (stringW65C816_16.asm has the functions above; strlen is 65535 when sz is that long
//or longer).  These take 32 bit counts (stringW65C816.asm), for what can pass 64KB.
void*	memcpy32(void* pvDest, const void* pvSource, unsigned long ulBytes) noexcept;
void*	memmove32(void* pvDest, const void* pvSource, unsigned long ulBytes) noexcept;
void*	memset32(void* pvDest, int iValue, unsigned long ulBytes) noexcept;
int		memcmp32(const void* pv1, const void* pv2, unsigned long ulBytes) noexcept;
void*	memchr32(const void* pv, int iValue, unsigned long ulBytes) noexcept;
unsigned long	strlen32(const char* sz) noexcept;
#endif // __W65C816__
}


#endif // __LIBC65_STRING_H__
