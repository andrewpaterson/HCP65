//Files and formatted output (stdio.cpp).  stdin, stdout and stderr are the console; files are the host's (host.h).
#ifndef __LIBC65_STDIO_H__
#define __LIBC65_STDIO_H__
#include <stddef.h>
#include <stdarg.h>


#define EOF			(-1)
#define SEEK_SET	0
#define SEEK_CUR	1
#define SEEK_END	2
#define BUFSIZ		512
#define FILENAME_MAX	260


struct FILE;
typedef long		fpos_t;


extern "C"
{
extern FILE*	stdin;
extern FILE*	stdout;
extern FILE*	stderr;

FILE*	fopen(const char* szName, const char* szMode) noexcept;
int		fclose(FILE* pcFile) noexcept;
size_t	fread(void* pv, size_t uiSize, size_t uiCount, FILE* pcFile) noexcept;
size_t	fwrite(const void* pv, size_t uiSize, size_t uiCount, FILE* pcFile) noexcept;
int		fseek(FILE* pcFile, long iOffset, int iOrigin) noexcept;
long	ftell(FILE* pcFile) noexcept;
int		fflush(FILE* pcFile) noexcept;
int		feof(FILE* pcFile) noexcept;
int		fgetc(FILE* pcFile) noexcept;
int		getchar(void) noexcept;
char*	fgets(char* sz, int iSize, FILE* pcFile) noexcept;
int		fputc(int c, FILE* pcFile) noexcept;
int		fputs(const char* sz, FILE* pcFile) noexcept;
int		puts(const char* sz) noexcept;
int		putchar(int c) noexcept;
int		remove(const char* szName) noexcept;
int		rename(const char* szOld, const char* szNew) noexcept;

int		printf(const char* szFormat, ...) noexcept;
int		fprintf(FILE* pcFile, const char* szFormat, ...) noexcept;
int		sprintf(char* szDest, const char* szFormat, ...) noexcept;
int		snprintf(char* szDest, size_t uiSize, const char* szFormat, ...) noexcept;
int		vsprintf(char* szDest, const char* szFormat, va_list vaArguments) noexcept;
int		vsnprintf(char* szDest, size_t uiSize, const char* szFormat, va_list vaArguments) noexcept;
int		vprintf(const char* szFormat, va_list vaArguments) noexcept;
int		vfprintf(FILE* pcFile, const char* szFormat, va_list vaArguments) noexcept;

#if defined(__W65C816__)
//32 bit sizes and counts, for more than a size_t's 65535 of either (the buffer is then malloc32's).  fread and fwrite
//themselves read and write size times count bytes though that is more than a size_t holds.
unsigned long	fread32(void* pv, unsigned long ulSize, unsigned long ulCount, FILE* pcFile) noexcept;
unsigned long	fwrite32(const void* pv, unsigned long ulSize, unsigned long ulCount, FILE* pcFile) noexcept;
#endif // __W65C816__
}


#endif // __LIBC65_STDIO_H__
