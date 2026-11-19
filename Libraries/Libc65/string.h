//Memory and string functions.
#ifndef __LIBC65_STRING_H__
#define __LIBC65_STRING_H__
#include <stddef.h>


extern "C"
{
void*	memcpy(void* pvDest, const void* pvSource, size_t uiBytes);
void*	memmove(void* pvDest, const void* pvSource, size_t uiBytes);
void*	memset(void* pvDest, int iValue, size_t uiBytes);
int		memcmp(const void* pv1, const void* pv2, size_t uiBytes);
void*	memchr(const void* pv, int iValue, size_t uiBytes);

size_t	strlen(const char* sz);
char*	strcpy(char* szDest, const char* szSource);
char*	strncpy(char* szDest, const char* szSource, size_t uiChars);
char*	strcat(char* szDest, const char* szSource);
char*	strncat(char* szDest, const char* szSource, size_t uiChars);
int		strcmp(const char* sz1, const char* sz2);
int		strncmp(const char* sz1, const char* sz2, size_t uiChars);
char*	strchr(const char* sz, int c);
char*	strrchr(const char* sz, int c);
char*	strstr(const char* sz, const char* szFind);

#if defined(__W65C816__)
//size_t is 16 bits on the W65C816 (stringW65C816_16.asm has the functions above; strlen is 65535 when sz is that long
//or longer).  These take 32 bit counts (stringW65C816.asm), for what can pass 64KB.
void*	memcpy32(void* pvDest, const void* pvSource, unsigned long ulBytes);
void*	memmove32(void* pvDest, const void* pvSource, unsigned long ulBytes);
void*	memset32(void* pvDest, int iValue, unsigned long ulBytes);
int		memcmp32(const void* pv1, const void* pv2, unsigned long ulBytes);
void*	memchr32(const void* pv, int iValue, unsigned long ulBytes);
unsigned long	strlen32(const char* sz);
#endif // __W65C816__
}


#endif // __LIBC65_STRING_H__
