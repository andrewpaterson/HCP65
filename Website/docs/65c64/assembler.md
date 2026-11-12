# Assembler

The 65C64 assembler (`CAsm65C64Assembler` in Assembler65Lib) turns 65C64 source into bytes.  This
page is the source format: lines, labels, numbers, expressions and directives.  The instructions
themselves are on the [instruction pages](instructions/index.md), and their operands in
[Operands and Addressing](operands.md).

The assembler works in two ways:

- **Absolute.**  `.org` places the bytes at addresses, and the output is ready to load.
- **Object.**  The source is assembled into sections, symbols and relocations, and the
  [linker](linking.md) places them.

## Lines

```text
	{label:}… {instruction | directive | name = expression}  {; comment}
```

A line holds any number of labels, then at most one instruction, directive or constant, then a
comment.  All of them are optional.

```asm
	.org $2000
Start:
	cpy A, 5		; an instruction
Count = 10			; a constant
Loop: Again:			; two labels for one address
	sub A, 1
	brac z, Loop
	stp
```

- Mnemonics and directives aren't case sensitive.  On these pages they're in lower case.
- Labels and constants are case sensitive.
- Registers are upper case (`A`, `B`, `S`, `V`, `W`, `X`, `Y`, `C`, `P`) and those names are
  reserved.  Status flags are lower case (`c`, `z`, `n`, `v`, `f`, `g`, `e`, `l`), so `v` is the
  overflow flag and `V` the register.
- `;` starts a comment that runs to the end of the line.

## Labels

A label is a name followed by a colon.  Its value is the address of the next thing assembled.
Instructions are aligned to 16 bits, so a label before an instruction is bound after the address
is aligned, not before:

```asm
	.org $2000
	.byte 1			; $2000
Next:				; $2002: the instruction below is aligned
	cpy A, 1
```

A label can be used before it's defined.  A name that is never defined is an error in absolute
mode, and an external symbol when assembling an object.

## Constants

```text
	name = expression
```

A constant gives a name to a value.  It can use labels, so the size of a table is the
difference of two labels:

```asm
	.org $2000
Table:
	.long 1, 2, 3, 4
TableEnd:
Count = (TableEnd - Table) / 4
	cpy A, Count		; cpy A, 4
```

## Numbers

| Form | Example |
|---|---|
| Decimal | `123` |
| Hexadecimal | `$7B` or `0x7B` |
| Binary | `0b1111011` |
| Character | `'a'` |

Underscores may separate digits: `$1234_5678`, `0b1111_0000`.  A character can be an escape:
`\n`, `\r`, `\t`, `\0`, `\\`, `\'` or `\"`.

## Expressions

Expressions are worked out in 64 bits when the source is assembled.  The operators are C's,
here from lowest precedence to highest:

| Operators | Meaning |
|---|---|
| `|` | Bitwise or |
| `^` | Bitwise exclusive or |
| `&` | Bitwise and |
| `<<` `>>` | Shift left and right |
| `+` `-` | Add and subtract |
| `*` `/` `%` | Multiply, divide and remainder |
| `-` `+` `~` `!` | Negate, plus, bitwise not and logical not (of one value) |

Round brackets group arithmetic inside one argument:

```asm
	cpy A, (1 + 2) * 3
	cpy B, 1 << 12 | $FF
	cpy X, ~$0F & $FF
```

Round brackets have a second job: they group two or more operands into one, as in
`psh (A, B), S` or `sep (c, n)`.  Those brackets aren't optional (`sep c, n` is an error) and a
group of one is an error (`sep (c)`).

### The program counter

C is the program counter as an instruction runs: the address of the next instruction.  It is
never assembled as a value, because a value fixed at assembly time would be wrong once code is
moved.  C can only be written in the target of a branch, `bra` or `brsr`, as `C + offset` or
`C - offset`, where it means the offset itself:

```asm
	bra C - 2		; branch to itself
	brac z, C - 8		; the same as brac z, -8
```

Anywhere else C is an error: `cpy A, C`, `jmp C`, `.long C` and `Count = C - Table` are all
refused.  Use labels instead, and [`psh C`](instructions/stack.md#p-and-c) to get the program
counter at run time.

## Suffixes

One suffix may follow a value or an operand.  It starts with a colon:

| Suffix | On | Meaning |
|---|---|---|
| `:8` `:16` `:24` `:32` `:40` `:48` `:64` | A value | An immediate of that width in bits |
| `:s8` `:s16` … `:s64` | A value | A sign extended immediate of that width |
| `:16` | An address | An offset: the address `0000:xxxx`, in one word |
| `:32` | An address | An absolute address, in two words |
| `:s` | The second operand | Sign extend it to the operation's width |
| `:8` `:16` `:32` `:64` `:s8` `:s16` `:s32` | `cpyx`'s memory operand | The width of the value in memory |

Only one suffix is allowed, so `:s:16`, `:16:s` and `:16s` are errors.

## Directives

| Directive | Meaning |
|---|---|
| `.org address` | Assemble what follows at this address.  Absolute mode only. |
| `.align bytes` | Pad with zeros to a multiple of `bytes`, a power of two |
| `.byte value, "string", …` | 8 bit values, and the characters of strings |
| `.word value, …` | 16 bit values, little endian |
| `.long value, …` | 32 bit values |
| `.quad value, …` | 64 bit values |
| `.space bytes` | That many zero bytes |
| `.include "file"` | The lines of another source file, in place of this line.  Includes nest up to 8 deep. |
| `.inchex "file"` | The bytes of an Intel HEX file |
| `.inchex "file", start` | Its bytes from the address `start` to its highest address |
| `.inchex "file", start, length` | `length` of its bytes from the address `start` |

`.inchex` fills any gap in the file's addresses with `$FF`.  File names are relative to the
directory given to the assembler with `SetDirectory`.

```asm
	.org $3000
Message:
	.byte "Hello", 13, 10, 0
	.align 4
Table:
	.word 1, 2, 3
	.long Message
	.quad $1122334455667788
Buffer:
	.space 64
```

The directives that only an object takes (`.section`, `.global`, `.weak` and `.extern`) are on
the [Objects and Linking](linking.md) page.

## Choosing the shortest form

Most operands have a short and a long form, and the assembler picks the shortest that holds
the value:

- an [embedded](operands.md#embedded-mode-15) value before an immediate,
- the narrowest immediate that holds the value,
- an [offset](operands.md#address-offset) before an absolute address, and
- the one word branch always before a `bra` with an immediate.

A suffix forces a longer form: `cpy A, 5:32`, `cpy A, [$40:32]`.

The assembler makes a pass over the source, and repeats it (up to 16 times) until no label
moves and no instruction grows.  A forward reference starts in its shortest form and grows only
if it must.  An instruction never gets shorter than it was in an earlier pass, so the passes
always settle.  A last pass writes the output and reports the errors.

When a value doesn't fit the width it's been given, the assembler keeps its low bits and gives
a warning: an offset wider than 16 bits written with `:16`, or an absolute address wider than
32 bits.

## Errors and warnings

Each error is a line of text giving the source line and column, a message and the token it's
about:

```text
2:10: Register W cannot address memory here '4'
5:6: Undefined symbol 'Loop'
```

Errors stop the assembly: there's no output.  Warnings have the same form and don't stop it.

## Using the assembler from C++

```cpp
CAsm65C64Assembler	cAssembler;

cAssembler.Init();
if (cAssembler.Assemble(szSource))
{
	cAssembler.GetOutput();		//Segments of bytes at addresses.
}
else
{
	cAssembler.GetErrors();		//"line:column: message 'token'" lines.
}
cAssembler.GetWarnings();		//The same format.  Warnings don't stop assembly.
cAssembler.Kill();
```

`Assemble` is absolute mode.  `AssembleObject(szSource, &cObject)` writes a `CAsm65Object`
for the [linker](linking.md) instead.  `GetSymbol(szName, &iValue)` gives a label's or a
constant's value after assembly.
