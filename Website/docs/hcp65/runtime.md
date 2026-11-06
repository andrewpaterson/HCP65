# W65C816 Runtime

The C++ compiler (CppParserLib's W65C816 back end) turns multiplies, divides, variable shifts and
BCD arithmetic into calls to runtime routines named `R816_...`.  They are hand written assembly
in three files in `HCP65/Libraries/Libc65`:

| File | Holds |
|---|---|
| `IR816Runtime-Common.asm` | Everything that doesn't depend on the maths tables: all the divides, the 64 bit multiply, the shifts and BCD. |
| `IR816Runtime-MathTables.asm` | The 16 and 32 bit multiplies, using Garth Wilson's 8 × 8 product table (MULT). |
| `IR816Runtime-MathSafe.asm` | The same multiplies without any table. |

Every routine is in its own section, so a program links only the ones it uses.  The compiler
itself only writes the small tables its code reads (the divide-by-constant and bit tables).

## Choosing a math file

`CCPP65Builder::AddRuntime816` links `IR816Runtime-Common.asm` and one of the two math files.  It is
called by `AddMathTables` or `AddLibc65`.

- **MathTables** is the default.
- **MathSafe** is used when the build sets `CCPP65Builder::SetMathTables(false)`, or
  `MathTables="false"` in its `.hcp65build` configuration.  It must be set before the runtime is
  added.  The compiler then also calls `R816_Mul16` for an 8 bit multiply instead of reading MULT
  inline.

The kernal and `boot.exe` are built with **MathSafe**, because the tables aren't in memory until
`boot.exe` loads them.  Other programs (such as `modplay.exe`) are built with **MathTables**.

A link that is missing any `R816_` routine is an error, rather than the call being stubbed with
`stp`.

## Calling conventions

**Scratch routines** (the 16 and 32 bit multiplies and divides) take their operands in the
caller's scratch bytes and run on the caller's D:

| Bytes | Use |
|---|---|
| D + 15 | Operand a |
| D + 19 | Operand b |
| D + 5 to D + 22 | Work space the routine may change |

A 16 bit result comes back in A; a 32 bit result in A (low word) and X (high word).

**Pushed routines** (the 64 bit multiply and divides, and BCD) use the compiler's normal convention:
arguments pushed last first, D pointed at the stack, and a 64 bit result written through a hidden
pointer.

**Shifts** take the count in A.  The 32 bit ones shift operand a (D + 15) and return the result in
A and X; the 64 bit ones shift a value where it is in the caller's frame, at D + X.

**Remainders.**  Every divide also leaves its remainder at the caller's D + 5 (D + 5 to D + 12 for
64 bits).  When `x % d` comes straight after `x / d` with the same operands, signed or unsigned,
the compiler reads the remainder from there instead of calling a remainder routine or multiplying
back.

## IR816Runtime-Common.asm

| Routines | How they work |
|---|---|
| `R816_UDiv16`, `R816_URem16` | Unrolled shift and subtract; 8 steps when the quotient fits in a byte, and 8 bit wide steps (a byte at a time) for a divisor under 128.  Quick exits for a divisor of 0, a divisor of `$8000` or more, and a dividend smaller than the divisor. |
| `R816_UDiv32`, `R816_URem32` | Jump to the 16 bit divide when both operands fit in 16 bits.  Otherwise unrolled steps chosen by the divisor: 8 bits wide for one under 128, without the overflow guard for one under `$8000`, with it above; for one of 65536 or more, 16 unrolled steps (8 when the quotient fits in a byte), or a guarded loop for one of 2^31 or more. |
| `R816_SDiv16`, `R816_SRem16`, `R816_SDiv32`, `R816_SRem32` | Make the operands positive, call the unsigned divide on the same D, then fix the signs (the remainder takes the dividend's sign, as in C). |
| `R816_UDiv64`, `R816_URem64`, `R816_SDiv64`, `R816_SRem64` | One shared body.  When both values fit in 32 bits it uses the 32 bit divide; otherwise it skips zero top words and divides a bit at a time. |
| `R816_Mul64` | The low 64 bits of a product, from one `R816_UMulWide32` and up to two `R816_Mul32` (skipped when a high half is 0). |
| `R816_Shl32`, `R816_LShr32`, `R816_AShr32`, and the 64 bit ones | Shifts by a count known only at run time: whole words first, then a byte, then the remaining 1 to 7 bits. |
| `R816_BcdAdd`, `R816_BcdSubtract` | Decimal mode add or subtract of word arrays with carry, for `CNumber`. |
| `R816_Clz16`, `R816_Clz32`, `R816_Ctz16`, `R816_Ctz32` | `__builtin_clz` and `__builtin_ctz` of 16 and 32 bit values (the width for 0): the word and byte the count starts in found by tests, then the byte's count read from a 256 byte table (`R816_Clz8`, `R816_Ctz8`).  About 30 cycles plus the call. |

## IR816Runtime-MathTables.asm

| Routine | How it works |
|---|---|
| `R816_Mul16` | Up to three lookups in the product table (only the low bytes of the cross products are needed). |
| `R816_Mul32` | Up to ten byte products, skipping zero bytes and zero high halves; a fast path when b is under 256. |
| `R816_UMulWide32` | The full 64 bit product of two 32 bit values, from 16 byte products read through row pointers. |

With the tables, a 16 bit multiply inside a loop is `R816_Mul16`'s code in place, which saves the call's 14 cycles;
outside loops the shorter call is kept.

## IR816Runtime-MathSafe.asm

| Routine | How it works |
|---|---|
| `R816_Mul16` | Unrolled shift and add from the top bit; 8 steps when one operand is a byte. |
| `R816_UMul16` | A 16 × 16 → 32 bit helper (shift and add, 8 steps for a byte multiplier). |
| `R816_Mul32`, `R816_UMulWide32` | Built from `R816_UMul16`, skipping zero high halves. |

## Speed

Typical cycle counts:

| Routine | MathTables | MathSafe |
|---|---|---|
| `R816_Mul16` (16 × 16 bits) | 137 | 245 |
| `R816_Mul32` (32 × 32 bits) | 517 | 886 |
| `R816_UMulWide32` | 566 | 1210 |

The divides are the same in both:

| Divide | Cycles |
|---|---|
| 16 / 16 bits | about 113 (counting dividends below the divisor, which return early) |
| 16 bits / a divisor under 128 | about 275 |
| 16 bits / a divisor of 128 to 255 | about 254 |
| 32 bits / a divisor under 128 | about 515 |
| 32 bits / a divisor under `$8000` | about 487 |
| 32 bits / a divisor of `$8000` to `$FFFF` | about 428 |
| 32 / 32 bits | about 694 |
| `R816_SDiv64` | about 4230 on average; about 830 when both values fit in 32 bits |

The divides don't use a reciprocal table.  With only 8 × 8 products, one product costs about the
same as one shift and subtract step.  A reciprocal divide was measured and won only for a divisor
under 256 with a quotient over 255 (340 to 293 cycles), and lost otherwise.

## The maths tables

On the HCP65, `boot.exe` loads the tables from the SD card into main memory, and the kernal maps
them into every process at the same address (`HCP65/Libraries/UserLib/UserMathTables.asm`):

| Address | Size | Table | Read by |
|---|---|---|---|
| `$400000`–`$41FFFF` | 128KB | MULT (`MathMult`), from `\OS\Math\MULT.BIN` | The MathTables multiplies, inline 8 bit multiplies and the soft float |
| `$420000`–`$43FFFF` | 128KB | The upper half of INVERT (`MathInvertHigh`), from `$20000` in `\OS\Math\INVERT.BIN` | The soft float's divides (`softfloat816.asm`) |

`INVERT.BIN` on the card holds the whole table, but only its upper half is loaded.  The kernal's
heap starts after the tables, at `$440000`.

Programs that aren't for the HCP65 (the test programs) link the tables into the program instead
(`Libc65/MathTbls`).  INVERT is two sections there, so a program links only the half it uses.

## Tests

- `TestIR816BackendRuntimeRoutines` calls every multiply and divide routine directly, with the
  tables and without, and compares each result with the host's.  It also checks that the remainder
  is at D + 5, that nothing else in the caller's direct page changed, and that the stack is back
  where it was.
- `TestIR816BackendMathSafe` runs the arithmetic tests linked without any table.
- `TestCPP816SharedDivisions` checks that a divide followed by a remainder of the same operands
  uses one call, at 16, 32 and 64 bits, signed and unsigned.
