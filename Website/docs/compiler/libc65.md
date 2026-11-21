# Libc65

Libc65 (`HCP65/Libraries/Libc65`) is the C library both processors' programs use: the standard
headers, their functions, soft float and the maths library, and the W65C816's runtime routines.
Most of it is C++ compiled with the program; the parts where speed matters are hand written
assembly for each processor, with the C++ kept as the reference.

The compiler gives Libc65 to the preprocessor as a system include directory, and a build links its
units as a library, so a program only gets the functions it uses.  `stdlib.cpp` uses BaseLib's
`TrueMallocator.h`, so `CommonLibraries/Libraries` must be an include directory too.

## Headers

| Header | Has |
|---|---|
| `stdint.h`, `inttypes.h`, `stddef.h`, `limits.h`, `stdbool.h` | The types and limits for each processor.  The fixed width types are `int` where `int` is that wide (`int16_t` on the W65C816, `int32_t` on the 65C64; `int32_t` is `long` on the W65C816); `size_t` is `unsigned int` (16 bits on the W65C816, `SIZE_MAX` 65535) and `ptrdiff_t` a pointer wide (`int` on the 65C64, `long` on the W65C816). |
| `stdarg.h` | `va_list` and `va_arg`, walking up the stack from the last named parameter. |
| `string.h`, `memory.h` | The string and memory functions.  On the W65C816 they take 16 bit counts (`size_t`), and `memcpy32`, `memmove32`, `memset32`, `memcmp32`, `memchr32` and `strlen32` take 32 bit ones. |
| `stdlib.h`, `malloc.h` | `malloc` and `free`, conversions (`strtol`, `strtod` ...), `rand`, `qsort`, `atexit`, `exit`.  On the W65C816 `malloc32`, `calloc32` and `realloc32` take 32 bit sizes. |
| `stdio.h` | The `printf` family, `stdout` and `stderr`, and files.  `fread` and `fwrite` read and write size times count bytes though that is more than a `size_t` holds; on the W65C816 `fread32` and `fwrite32` take a size or a count of more than 65535 (into or from a `malloc32` block). |
| `math.h` | The maths library (below). |
| `ctype.h`, `time.h`, `signal.h`, `errno.h`, `assert.h` | |
| `new` | `operator new` and `delete`, and placement `new`. |
| `host.h` | The emulator's host files: the host computer's file system, from a program running on the emulator. |

There is no C++ standard library beyond `<new>`.

## What it is written in

| Unit | Written in |
|---|---|
| `string.cpp` | C++.  On the W65C816, `stringW65C816_16.asm` (`memcpy`, `memmove`, `memset`, `memcmp`, `strlen`, with 16 bit counts) and `stringW65C816.asm` (the 32 suffixed ones and the other string functions; `memchr`, `strncpy`, `strncat` and `strncmp` are 16 bit entries into their 32 bit bodies) replace the block and vector functions, using `mvn` and `mvp`.  On the 65C64, `string.asm`. |
| `softfloat.cpp` | C++, the bit exact reference.  `softfloatW65C816.asm` and `softfloat.asm` replace it on each processor, so the C++ compiles to nothing there. |
| `stdlib.cpp`, `stdio.cpp`, `math.cpp`, `ctype.cpp`, `time.cpp`, `new.cpp`, `signal.cpp`, `host.cpp` | C++. |
| `IRW65C816Runtime-*.asm` | The W65C816's multiply, divide, shift and BCD routines (see [W65C816 Runtime](../hcp65/runtime/w65c816.md)). |
| `MathTbls/` | Garth Wilson's maths tables; the W65C816's multiplies use `MULT`. |

## Memory

`malloc` and `free` are BaseLib's `CTrueMallocator`, a two level segregated fit allocator: they take
the same time however many blocks there are, and a freed block is joined to the free blocks either
side.  On the emulators the heap is `0008:0000` to `000F:FFFF`, or above the program from
`0020:0000` when there is more than 2MB of memory.

On the W65C816 the heap is by banks (BaseLib's `CBankMallocator`): each 64KB bank is a region of
its own, so no block crosses a bank.  Small blocks come from the banks at the bottom of the heap,
and a bank goes back when its last block is freed, but one empty bank is kept for what comes next.
What doesn't fit a bank (from `malloc32`, or `malloc`'s largest sizes) takes whole adjacent banks
from the top, rounded up to 64KB.  `free` and `realloc` take either kind.

A program the HCP65 kernal runs has its heap from `0020:0000` to `00FE:FFFF`, but the maths tables'
banks at `0040:0000`.  The kernal maps a bank of it into the program when `malloc` first needs it,
and takes it back when it empties, so a program has only the memory it is using, up to all that is
free; `malloc` returns NULL when main memory runs out.  Only taking or giving back a bank calls the
kernal: the other allocations are as fast as anywhere else.  The `UserHeapSize` symbol is how much
of the heap is mapped before the program starts, if it wants it there from the start.

## Input and output

`stdout` and `stderr` go to the console device.  Files are the host's, through the host file
device: a program running on an emulator reads and writes files on the computer running it.
`printf` is not correctly rounded past about 15 significant digits for floating point, and `strtod`
is within an ulp or so.

`exit` runs what returning from `main` runs, then `_Exit`, which stops with the status in A: the
`thread_local` objects' destructors, then the `atexit` functions (at most 32) and the destructors of
globals and static locals, on one list, so they run in the reverse of the order they were registered
or made, then the destructors of globals made in the data.  `abort` is `_Exit(EXIT_FAILURE)`.

There is no clock yet: `time` is 0 and `clock` is -1.  `gmtime`, `localtime` (UTC) and `mktime` work
in integers only.

## Floating point

Neither processor has floating point instructions.  The compiler turns `float` and `double`
arithmetic, comparisons and conversions into calls of the soft float routines:

```text
SoftFloatAdd  SoftFloatSubtract  SoftFloatMultiply  SoftFloatDivide	(and SoftDouble...)
SoftFloatCompare			-1, 0, 1, or 2 when a NaN is involved
SoftFloatFromInt  ...Unsigned  ...LongLong  ...UnsignedLongLong
SoftFloatToInt    ...Unsigned  ...LongLong  ...UnsignedLongLong	(towards zero)
SoftFloatToDouble  SoftDoubleToFloat  SoftFloatSqrt  SoftDoubleSqrt
```

- Rounding is to nearest, ties to even, with subnormals, infinities and NaNs.
- NaNs behave as SSE's (and so MSVC's) do: an invalid operation gives the default NaN
  (`0xFFC00000` for a float), a NaN operand comes back quietened, and converting a NaN or an out of
  range value to an integer gives the most negative integer.
- Negation and tests against zero are written in line.
- Constant expressions are worked out by the compiler as the processor would, each float result
  rounded to float.

`softfloat.cpp` was checked bit for bit against SSE on millions of values, and each processor's
assembly against `softfloat.cpp`.  On the 65C64 the assembly takes 25 to 220 cycles a call for
normal operands, 20 to 96 times faster than the compiled C++.  On the W65C816 the compiler calls
`softfloatW65C816.asm`'s *frame operand* entries: the operands stay in the caller's frame and their
places are passed in X and Y, so nothing is pushed or removed.

## The maths library

`math.h` has `fabs`, `sqrt`, `trunc`, `floor`, `ceil`, `round`, `fmod`, `ldexp`, `frexp` and `modf`
(exact), and `exp`, `log`, `log10`, `pow`, `sin`, `cos`, `tan`, `atan`, `asin`, `acos` and `atan2`,
with their `f` and `l` forms.  The methods are fdlibm's, improved: tables and double-double
arithmetic to 106 bits, and Payne-Hanek reduction for large trigonometric arguments.

Checked against glibc's `long double` functions on a million arguments each, every function is
within 1 ulp, and correctly rounded for all but 0.02% to 3.5% of arguments (`sin`, `cos` and `tan`
are the 3%).  The constants were worked out exactly with Python integers and fractions, not copied.

## Testing

`HCP65/Tests/TestLibc65` compiles Libc65's tests for each processor and runs them on the emulators:
the string, conversion, `printf`, time and soft float functions against the same calls natively.
