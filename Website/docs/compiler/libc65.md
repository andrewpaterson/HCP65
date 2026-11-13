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
| `stdint.h`, `inttypes.h`, `stddef.h`, `limits.h`, `stdbool.h` | The types and limits for each processor.  `int32_t`, `size_t` and `ptrdiff_t` are `long` on both. |
| `stdarg.h` | `va_list` and `va_arg`, walking up the stack from the last named parameter. |
| `string.h`, `memory.h` | The string and memory functions. |
| `stdlib.h`, `malloc.h` | `malloc` and `free`, conversions (`strtol`, `strtod` ...), `rand`, `qsort`, `atexit`, `exit`. |
| `stdio.h` | The `printf` family, `stdout` and `stderr`, and files. |
| `math.h` | The maths library (below). |
| `ctype.h`, `time.h`, `signal.h`, `errno.h`, `assert.h` | |
| `new` | `operator new` and `delete`, and placement `new`. |
| `host.h` | The emulator's host files: the host computer's file system, from a program running on the emulator. |

There is no C++ standard library beyond `<new>`.

## What it is written in

| Unit | Written in |
|---|---|
| `string.cpp` | C++.  On the W65C816, `stringW65C816.asm` and `stringW65C816_16.asm` replace the block and vector functions (`memcpy`, `memset`, `memmove`, `strlen`, `strcmp` ...), using `mvn` and `mvp`.  On the 65C64, `string.asm`. |
| `softfloat.cpp` | C++, the bit exact reference.  `softfloatW65C816.asm` and `softfloat.asm` replace it on each processor, so the C++ compiles to nothing there. |
| `stdlib.cpp`, `stdio.cpp`, `math.cpp`, `ctype.cpp`, `time.cpp`, `new.cpp`, `signal.cpp`, `host.cpp` | C++. |
| `IRW65C816Runtime-*.asm` | The W65C816's multiply, divide, shift and BCD routines (see [W65C816 Runtime](../hcp65/runtime/w65c816.md)). |
| `MathTbls/` | Garth Wilson's maths tables; the W65C816's multiplies use `MULT`. |

## Memory

`malloc` and `free` are BaseLib's `CTrueMallocator`, a two level segregated fit allocator: they take
the same time however many blocks there are, and a freed block is joined to the free blocks either
side.  On the emulators the heap is `0008:0000` to `000F:FFFF`, or above the program from
`0020:0000` when there is more than 2MB of memory.  A program the HCP65 kernal runs has its heap
size given by its `UserHeapSize` symbol.

## Input and output

`stdout` and `stderr` go to the console device.  Files are the host's, through the host file
device: a program running on an emulator reads and writes files on the computer running it.
`printf` is not correctly rounded past about 15 significant digits for floating point, and `strtod`
is within an ulp or so.

`exit` calls the `atexit` functions (at most 32, last first) and the global destructors, then
`_Exit`, which stops with the status in A.  `abort` is `_Exit(EXIT_FAILURE)`.

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
