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
| `wchar_t`, `char16_t` | 2 bytes | 2 bytes | Unsigned. |
| Pointers, references | 4 bytes | 4 bytes | On the W65C816 a 24 bit address with a zero top byte. |
| `size_t`, `ptrdiff_t` | 4 bytes | 4 bytes | `unsigned long` and `long`. |
| Pointers to members | 4 bytes | 4 bytes | A field's offset, or a member function's address. |

An enum's underlying type follows the standard: `int`, then `unsigned int`, `long`,
`unsigned long` and `long long` as its values need, so on the W65C816 an enum with a value of
`0xFFFFFFFF` is 32 bits.

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
- `uint16_t` promotes to `unsigned int`, so `u16 - 1` is 65535, not -1.
- `uint8_t << 16` and seeds, counts and sizes cast to `int`.
- Multi character literals (`'\0MAN'`) are `long`, and `NULL` passed to a variadic function
  must be pointer sized (`0L`).

The compiler warns about the constant cases: an *integer overflow* warning for a constant
expression whose value doesn't fit its type, and an *integer conversion overflow* warning for a
constant that an implicit conversion cuts short.

## What is supported

| Area | Supported |
|---|---|
| Statements | Everything in C: `if`, `while`, `do`, `for`, `switch`, `break`, `continue`, `return`, `goto`.  Declarations in conditions (`if (int x = f())`). |
| Functions | Overloading, default arguments, recursion, variadic functions (`stdarg.h`), pointers to functions. |
| Classes | Structs, classes and unions, named or not, local and anonymous; multiple inheritance with several polymorphic bases; virtual functions, pure virtuals, virtual destructors; `override` and `final`; constructors, destructors, member initialisers, default member initialisers; `= default` and `= delete`; static members; friends; nested classes. |
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
| `__W65C816_ASSEMBLY__` | The W65C816 build uses the hand written assembly units beside their C++ (`string816.asm`, `softfloat816.asm` ...). |

## What isn't supported yet

Most of these give an error that names the construct.  A function that uses C++ the back end
can't write gives *"This function uses C++ the W65C816 compiler doesn't support yet"* with the
compiler's own file and line, which says which construct it was.

| Not supported | Notes |
|---|---|
| Exceptions | `try`, `catch` and `throw`.  `try` is parsed, but a function using it is an error. |
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

Two constructs compile without an error but are **wrong**, so they must not be used:

!!! warning "Silently wrong"
    - **Bit-fields** are laid out and stored as whole fields: the width is ignored, so a value
      is never cut down to it.
    - **Delegating constructors** (`C() : C(7) {}`) don't call the constructor they name.

Overload resolution is a simplified form of the standard's ranking: exact match, then promotion,
then conversion, then a user defined conversion.  It gives the standard's answer for the code it
has been used on, but not every case.

The full list, with the order the work is planned in, is `CppCompilerLib/CppParserTodo.txt`
section 6.

## Floating point

Neither processor has floating point instructions, so `float` and `double` are done by Libc65's
soft float routines, hand written in assembly for each processor.  Rounding is to nearest, ties
to even, with subnormals, infinities and NaNs; NaNs behave as SSE's do, so results match a
Windows build bit for bit.  [Libc65](libc65.md#floating-point) has the details.

A build can refuse floating point altogether (`NoFloats`): any `float` or `double` is then an
error.  The HCP65 kernal is built this way.
