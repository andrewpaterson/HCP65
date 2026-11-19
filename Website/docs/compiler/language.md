# Supported C++

The compiler takes C++14 as far as the tables below go.  The preprocessor and tokeniser go
further, most of the way to C++17; the gaps are in the code generation.  How much C++ works is
measured by the project's own libraries: BaseLib, StandardLib and SupportLib compile for both
processors and their test suites run on the emulators, bit for bit against the same tests built
natively.

The preprocessor defines `__cplusplus` as `201402L`.

## Types

| Type | W65C816 | 65C64 | Notes |
|---|---|---|---|
| `char` | 1 byte | 1 byte | Signed, as MSVC. |
| `short` | 2 bytes | 2 bytes | |
| `int`, `unsigned` | **2 bytes** | 4 bytes | On the W65C816 `int` and `short` are the same size. |
| `long` | 4 bytes | 4 bytes | As on Windows. |
| `long long` | 8 bytes | 8 bytes | 8 byte aligned in classes, as MSVC. |
| `float` | 4 bytes | 4 bytes | IEEE 754 single, in software. |
| `double`, `long double` | 8 bytes | 8 bytes | IEEE 754 double, in software.  `long double` is a double but its own type for overloads. |
| `bool` | 1 byte | 1 byte | |
| `wchar_t`, `char16_t` | 2 bytes | 2 bytes | Unsigned.  `L'x'` is a `wchar_t`, `u'x'` a `char16_t`. |
| `char32_t` | 4 bytes | 4 bytes | Unsigned.  `U'x'` is one. |
| Pointers, references | 4 bytes | 4 bytes | On the W65C816 a 24 bit address with a zero top byte. |
| `size_t` | **2 bytes** | 4 bytes | `unsigned int` on both, so `sizeof`'s type, `size_t` and BaseLib's `size` are one type.  On the W65C816 `memcpy`, `malloc` and the others take 16 bit counts; `memcpy32`, `malloc32` and the other 32 suffixed functions take longer ones (see [Libc65](libc65.md)). |
| `ptrdiff_t` | 4 bytes | 4 bytes | A pointer wide: `int` on the 65C64, `long` on the W65C816, so a difference of pointers into a block of more than 32KB isn't cut. |
| Pointers to members | 4 bytes | 4 bytes | A field's offset, or a member function's address. |
| `std::nullptr_t` | 4 bytes | 4 bytes | `nullptr`'s type, a type of its own: it becomes any pointer or pointer to a member, a `bool` only where it is tested or directly initialises one, and nothing else. |

Each fundamental type is a type of its own, however wide: `int` and `long` are different types
on the 65C64 though both are 32 bits, as are `short` and `int` on the W65C816, and `char`,
`signed char` and `unsigned char` everywhere.  They overload, specialise templates and are
labelled apart, and promotions and the usual arithmetic conversions go by the standard's ranks
(`long + unsigned int` is `unsigned long` on the 65C64, `long` on the W65C816).  Libc65's fixed
width types are `int` where `int` is that wide: `int16_t` and `uint16_t` are `int` and
`unsigned int` on the W65C816, `int32_t` and `uint32_t` on the 65C64 (and `long` and
`unsigned long` on the W65C816).

An enum's underlying type follows the standard: `int`, then `unsigned int`, `long`,
`unsigned long` and `long long` as its values need, so on the W65C816 an enum with a value of
`0xFFFFFFFF` is 32 bits, and promotes to its underlying type: it is compared in 32 bits.

Classes are laid out the way MSVC lays them out (bases, then the vtable pointer, then the fields
each aligned to its size), because BaseLib reads and writes classes as raw bytes and expects the
same layout everywhere.  `#pragma pack` is followed.

### A 16 bit `int`

On the W65C816 `int` is 16 bits; to get 32 bits, declare a `long` or an `int32_t`.  There is no
option to make `int` 32 bits.  Code written for a 32 bit `int` meets these traps, all silent:

- An `int` passed to an overload set with a 16 bit and a 32 bit member picks the 16 bit one.
  BaseLib's own overloads (`AssertInt`, `IntToString`, `WriteInt32` ...) have a 16 bit family
  under `__W65C816__`.
- `*(int*)` and `sizeof(int)` used for 4 byte data.
- `uint16_t` is `unsigned int`, so `u16 - 1` is 65535, not -1.
- `uint8_t << 16` and seeds, counts and sizes cast to `int`.
- Multi character literals (`'\0MAN'`) are `long`, and `NULL` passed to a variadic function
  must be pointer sized (`0L`).
- A virtual function's override that takes an `int` where the base class's takes an `int32` is
  a different function, which calls through the base class never reach.  Declare it `override`
  and the compiler says so (below).

The compiler warns about the constant cases: an *integer overflow* warning for a constant
expression whose value doesn't fit its type, and an *integer conversion overflow* warning for a
constant that an implicit conversion cuts short.

### `override` and `final`

A function declared `override` must override a virtual function of a base class, and no function
may override one declared `final`: both are errors.  A function that overrides one without being
declared `override` (or `final`) gets a warning, as G++'s `-Wsuggest-override` gives, except a
destructor.  The warning is on unless a build configuration turns it off
(`WarnMissingOverride="false"`, see [Building](building.md)).  As the compiler's other checks of
a class, these are made where the class is first needed whole, so a class nothing uses isn't
checked.

### BaseLib's `size` and `lsize`

BaseLib counts and measures with its own `size` and `lsize` types (`BaseLib/PrimitiveTypes.h`).
`size` is `size_t`; `lsize` is for what can pass 64KB.  A call doesn't say which function it wants:
on the W65C816 `memcpy`, `memmove`, `memset`, `memcmp`, `memchr`, `malloc`, `calloc`, `realloc`,
`fread` and `fwrite` called with an `lsize` count (or a `long`, or a constant made with `KB` or
`MB`) are Libc65's `memcpy32`, `malloc32`, `fread32` and the others, chosen by the count's type
by overloads in `PrimitiveTypes.h`; with a `size`, a `size_t` or a `sizeof` they are the C
library's own.
`Memory32.h` has the 32 suffixed names on every target for the few places that name them, and
`strlen32`, a string's whole length:

| Type | W65C816 | 65C64 | Notes |
|---|---|---|---|
| `size` | **2 bytes** | 4 bytes | `size_t` on every target: the W65C816's natural width there. |
| `lsize` | 4 bytes | 4 bytes | `uint32` on the W65C816, for counts and lengths that can pass 64KB; `size` elsewhere. |

The two are different types only on the W65C816, so only there can a function be overloaded on
both.  A host build checks the W65C816's size model with `g++ -m32 -DBASELIB_SIZE16
-Wconversion`, which warns wherever an `lsize`, or anything wider, is narrowed to a `size`.

## What is supported

| Area | Supported |
|---|---|
| Statements | Everything in C: `if`, `while`, `do`, `for`, `switch`, `break`, `continue`, `return`, `goto`.  Declarations in conditions (`if (int x = f())`). |
| Functions | Overloading, default arguments, recursion, variadic functions (`stdarg.h`), pointers to functions. |
| Classes | Structs, classes and unions, named or not, local and anonymous; multiple inheritance with several polymorphic bases; virtual functions, pure virtuals, virtual destructors; `override` and `final`; constructors, destructors, member initialisers, default member initialisers, delegating constructors; `= default` and `= delete`; static members; friends; nested classes. |
| Bit-fields | Laid out as MSVC lays them out: a bit-field shares a unit the size of its type, from the unit's lowest bit, and a new unit starts when the type's size changes, the bits run out, or after a zero width bit-field.  Signed ones are sign extended when read; unnamed ones (`int : 3;`) are padding.  A bit-field has no address, but a `const` reference can bind to a copy of one. |
| Objects by value | Passed and returned by value; temporaries destroyed at the end of their full expression; results made in place (as C++17 requires). |
| Operators | Member and free operator functions, conversion functions, `operator->`, pointers to members (`.*` and `->*`). |
| Templates | Class, function and member templates; explicit and partial specialisation; non-type and default arguments; deduction from `T`, `T*`, `T&` and `X<..., T, N, ...>`; explicit instantiation; variadic templates with a function parameter pack expanded in a call's arguments. |
| Names | Namespaces (named, unnamed, inline), `using` declarations and directives, argument dependent lookup, `using Base::Base;` (inherited constructors), type aliases (`using Int = int;`). |
| C++11 | `nullptr`, `enum class`, `static_assert`, `override`, `final`, brace initialisation of aggregates, `T&&` parameters that pass their arguments on. |
| Casts | C style, `static_cast`, `reinterpret_cast`, `const_cast`, functional casts, and `dynamic_cast<T*>`. |
| Memory | `new`, `new[]`, `delete`, `delete[]` and placement `new(address)`, over Libc65's `operator new`. |
| Constants | Constant expressions with enumerators, `const` variables, casts, `sizeof` and type traits; floating point constants worked out as the processor would. |

### Extensions

| Extension | Meaning |
|---|---|
| `__builtin_clz`, `__builtin_ctz`, `__builtin_popcount` | And their `l` and `ll` forms. |
| `__builtin_expect`, `__builtin_unreachable` | |
| `__builtin_trap` | Stops the processor (`stp`). |
| `__builtin_alloca` | Memory from the stack, freed when the function returns. |
| `__builtin_memcpy` | A constant count of up to 64 bytes is copied in place. |
| `__builtin_huge_val`, `__builtin_inf`, `__builtin_nan` | Floating point constants. |
| `__builtin_bcd_add`, `__builtin_bcd_subtract` | Packed BCD arithmetic in the W65C816's decimal mode. |
| `__restrict` | On a global pointer or a pointer parameter: what it points at isn't reached any other way.  Lets loads leave loops. |
| `__attribute__((used))` | Keeps a global the program never names, so its constructor runs. |
| `__func__`, `__FUNCTION__`, `__PRETTY_FUNCTION__`, `__FUNCSIG__`, `__FUNCDNAME__` | String literals. |
| `#pragma pack` | As MSVC. |

### Predefined macros

| Macro | Defined |
|---|---|
| `__W65C816__` | Compiling for the W65C816. |
| `__65C64__` | Compiling for the 65C64. |
| `__LIBC65__` | Either: the program is built against Libc65. |
| `__W65C816_ASSEMBLY__` | The W65C816 build uses the hand written assembly units beside their C++ (`stringW65C816.asm`, `softfloatW65C816.asm` ...). |

## What isn't supported yet

Most of these give an error that names the construct.  A function that uses C++ the back end
can't write gives *"This function uses C++ the W65C816 compiler doesn't support yet"* with the
compiler's own file and line, which says which construct it was.

| Not supported | Notes |
|---|---|
| Exceptions | `try`, `catch` and `throw` are being added.  They will be compiled only for a program built with `Exceptions="true"` (see [Building](building.md)), which also defines `__cpp_exceptions`; a program built without it is told so where it uses one.  For now a function that uses one is an error either way. |
| Lambdas | |
| `auto` and `decltype` | |
| Range-based `for` | Parsed, but an error in a function. |
| `constexpr` functions | `constexpr` is taken as `inline`: a call isn't a constant expression. |
| Alias templates | `template<class T> using W = V<T>;` |
| `typeid` and `std::type_info` | No `<typeinfo>`. |
| Static locals with a non constant initialiser | Static locals that are objects too: they need a first time guard. |
| Virtual inheritance | |
| Rvalue references as a type of their own | `&&` is taken as `&`.  No move constructors, no `std::move`. |
| `std::initializer_list` | Brace initialisation of aggregates works. |
| Parameter packs anywhere but a call's arguments | No `sizeof...`, no packs in base lists or initialisers. |
| Template template parameters, SFINAE | Constructor templates and member class templates neither. |
| Class `operator new` and `delete` | The global ones are always used. |
| Wide, Unicode and raw string literals | |
| `thread_local` | |
| The C++ standard library | Libc65 has the C library and `<new>` only. |

Overload resolution is a simplified form of the standard's ranking: exact match, then promotion,
then conversion, then a user defined conversion.  It gives the standard's answer for the code it
has been used on, but not every case.

## Floating point

Neither processor has floating point instructions, so `float` and `double` are done by Libc65's
soft float routines, hand written in assembly for each processor.  Rounding is to nearest, ties
to even, with subnormals, infinities and NaNs; NaNs behave as SSE's do, so results match a
Windows build bit for bit.  [Libc65](libc65.md#floating-point) has the details.

A build can refuse floating point altogether (`NoFloats`): any `float` or `double` is then an
error.  The HCP65 kernal is built this way.
