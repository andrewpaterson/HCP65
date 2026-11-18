//The fixed width integers of the 65C64 and the W65C816: char 8 bits, short 16, long 32, long long 64, pointers 32.
//int is 32 bits on the 65C64 and 16 on the W65C816, and is the type of its width: the 16 bit types are int on the
//W65C816 (as avr-gcc's are), the 32 bit ones int on the 65C64 and long on the W65C816.  int, short and long are
//different types however wide, so an int16_t overload takes an int on the W65C816 as an int32_t one does on the 65C64
//(and Windows).
#ifndef __LIBC65_STDINT_H__
#define __LIBC65_STDINT_H__


#ifdef __W65C816__
typedef int						int16_t;
typedef unsigned int			uint16_t;
typedef long					int32_t;
typedef unsigned long			uint32_t;
#else
typedef short					int16_t;
typedef unsigned short			uint16_t;
typedef int						int32_t;
typedef unsigned int			uint32_t;
#endif // __W65C816__
typedef signed char				int8_t;
typedef long long				int64_t;
typedef unsigned char			uint8_t;
typedef unsigned long long		uint64_t;

typedef int8_t					int_least8_t;
typedef int16_t					int_least16_t;
typedef int32_t					int_least32_t;
typedef int64_t					int_least64_t;
typedef uint8_t					uint_least8_t;
typedef uint16_t				uint_least16_t;
typedef uint32_t				uint_least32_t;
typedef uint64_t				uint_least64_t;

typedef int						int_fast8_t;
typedef int						int_fast16_t;
typedef int32_t					int_fast32_t;
typedef long long				int_fast64_t;
typedef unsigned int			uint_fast8_t;
typedef unsigned int			uint_fast16_t;
typedef uint32_t				uint_fast32_t;
typedef unsigned long long		uint_fast64_t;

typedef int32_t					intptr_t;
typedef uint32_t				uintptr_t;
typedef long long				intmax_t;
typedef unsigned long long		uintmax_t;

#define INT8_MIN		(-127 - 1)
#define INT16_MIN		(-32767 - 1)
#define INT32_MIN		(-2147483647 - 1)
#define INT64_MIN		(-9223372036854775807LL - 1)
#define INT8_MAX		127
#define INT16_MAX		32767
#define INT32_MAX		2147483647
#define INT64_MAX		9223372036854775807LL
#define UINT8_MAX		0xff
#define UINT16_MAX		0xffff
#define UINT32_MAX		0xffffffffU
#define UINT64_MAX		0xffffffffffffffffULL

#define INTPTR_MIN		INT32_MIN
#define INTPTR_MAX		INT32_MAX
#define UINTPTR_MAX		UINT32_MAX
#define INTMAX_MIN		INT64_MIN
#define INTMAX_MAX		INT64_MAX
#define UINTMAX_MAX		UINT64_MAX
#define PTRDIFF_MIN		INT32_MIN
#define PTRDIFF_MAX		INT32_MAX
#define SIZE_MAX		UINT32_MAX

#define INT8_C(x)		(x)
#define INT16_C(x)		(x)
#define INT32_C(x)		(x ## L)
#define INT64_C(x)		(x ## LL)
#define UINT8_C(x)		(x)
#define UINT16_C(x)		(x)
#define UINT32_C(x)		(x ## UL)
#define UINT64_C(x)		(x ## ULL)
#define INTMAX_C(x)		(x ## LL)
#define UINTMAX_C(x)	(x ## ULL)


#endif // __LIBC65_STDINT_H__
