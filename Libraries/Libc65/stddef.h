//size_t, ptrdiff_t, NULL and offsetof for the 65C64 and the W65C816 (32 bit pointers; long is 32 bits on both).
#ifndef __LIBC65_STDDEF_H__
#define __LIBC65_STDDEF_H__


//size_t is unsigned int on both: 32 bits on the 65C64 (as on Win32), 16 on the W65C816, where it is sizeof's type and
//BaseLib's size and malloc's blocks never cross a bank (string.h's and stdlib.h's 32 suffixed functions take longer
//counts there).  ptrdiff_t is as wide as a pointer: long on the W65C816, so a difference of pointers into a block of
//more than 32KB isn't cut (the standard asks only that it be signed and pointer - pointer's type).
typedef unsigned int			size_t;
#ifdef __W65C816__
typedef long					ptrdiff_t;
#else
typedef int						ptrdiff_t;
#endif // __W65C816__
typedef long long				max_align_t;

#ifndef NULL
//Pointer sized (long is 32 bits on both targets), so NULL passed to a variadic function is a whole pointer.
#define NULL					0L
#endif // NULL

#define offsetof(type, member)	((size_t)&(((type*)0)->member))


#endif // __LIBC65_STDDEF_H__
