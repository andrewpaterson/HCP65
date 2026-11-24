# W65C816 Assembler

The W65C816 assembler (`CAsmW65C816Assembler` in Assembler65Lib) uses the W65C816's own mnemonics
(`lda`, `sta`, `adc`, `jsl`, `rep` ...) in the style of the [65C64 assembler](../65c64/assembler.md):
upper case registers, lower case flags, memory in square brackets, a bare value is a constant, and
`:` suffixes for widths.  Lines, labels, numbers, expressions, directives, sections and objects are
the 65C64 assembler's, and it writes the same objects, so [one linker](linker.md) links both.

This page is what is different.  For the rest, see [Assembler](../65c64/assembler.md) and
[Objects and Linking](../65c64/linking.md).

## Registers and flags

| Name | Is |
|---|---|
| `A X Y S D` | The accumulator, the index registers, the stack pointer and the direct page register |
| `c z i d x m v n` | The status flags, for `rep` and `sep` |

`A` as an operand is the accumulator addressing mode: `asl A`, `inc A`.

## Widths

The size of an immediate depends on m (for `lda`, `adc`, `and`, `bit`, `cmp`, `eor`, `ora`, `sbc`)
or on x (for `ldx`, `ldy`, `cpx`, `cpy`), so the assembler follows them down each section:

```asm
		rep (m, x)			; 16 bit A and X/Y; also rep m, sep x, rep $30
		sep m				; 8 bit A
		.width A, 16		; says what the width is
		.width X, 8			; X means X and Y, which always have the same width
		.width (A, X), 16
```

- At the start of a section both widths are unknown, and an immediate whose size depends on an
  unknown width is an error, so a wrong guess can't assemble silently.
- `rep` and `sep` with a constant change the widths.  `xce`, `plp` and `rti` make them unknown.
- Widths are followed top to bottom, not along jumps, so code reached by a jump from code with
  other widths says `.width` at its label.  The compiler writes `.width` at each function start.
- One immediate can be given a width with a suffix, `lda 5:8` or `ldx 1000:16`.  It is an error if
  it disagrees with the tracked width.

## Operands

| Operand | Mode |
|---|---|
| `value` | Immediate: `lda 5`, `ldx Count`, `pea Label`, `rep (m, x)` |
| `A` | Accumulator: `asl A` |
| `[value]` | Absolute, in the data bank: `lda [$1234]` |
| `[value]:24` | Long: `lda [$123456]:24`, or `[value]` when the value is over `$FFFF` |
| `[value + X]`, `[value + Y]` | Absolute,x and absolute,y |
| `[value + X]:24` | Long,x |
| `[D + value]` | Direct page: `lda [D + 4]` |
| `[D + value + X]`, `[D + value + Y]` | Direct page,x and direct page,y |
| `[[D + value]]` | (Direct page): a 16 bit pointer, in the data bank |
| `[[D + value + X]]` | (Direct page,x) |
| `[[D + value] + Y]` | (Direct page),y |
| `[[D + value]:24]` | [Direct page]: a 24 bit pointer |
| `[[D + value]:24 + Y]` | [Direct page],y |
| `[S + value]` | Stack relative |
| `[[S + value] + Y]` | (Stack relative),y |

The direct page and the stack are written as registers in the address, so it is always clear which
bank and base an address uses.  D's value isn't known to the assembler, so `[D + value]` is always
direct page, with a value from 0 to 255.

Without `:24`, `[value]` is absolute when the value fits 16 bits and long when it doesn't; a label
that isn't defined yet is absolute unless `:24` is given.  In an object such a field is relocated
*in bank*: the linker writes the label's low 16 bits, and it is an error unless the label is in
the instruction's bank.

## Jumps, calls and branches

The target of a jump is the effective address, as on the 65C64:

| Instruction | Mode |
|---|---|
| `jmp Label` | Absolute, in the program bank |
| `jmp [Vector]` | (Absolute): the address read from `Vector`, in bank 0 |
| `jmp [Table + X]` | (Absolute,x), in the program bank |
| `jml Label` | Long |
| `jml [Vector]:24` | [Absolute]: a 24 bit address read from `Vector` |
| `jsr Label`, `jsr [Table + X]`, `jsl Label` | Calls |
| `rts`, `rtl`, `rti` | Returns |
| `bra Label`, `brl Label`, `bne Label` ... | Relative; also `C + offset` and `C - offset`, where `C` is the next instruction |
| `per Label` | Pushes the label's address relative to the next instruction |

After `.relax` (to the end of the section), a branch to a label too far for a byte, or in another
section, is assembled as the opposite branch over a `brl` (`bne Far` becomes `beq C + 3` /
`brl Far`), and `bra` as `brl`.  The compiler writes `.relax` in every function.

## Block moves and banks

```asm
		mvn Source >> 16, Destination >> 16
		mvp Source >> 16, Destination >> 16
```

Operands are the source bank then the destination bank (the instruction's bytes are the other way
round).  `Label >> 16` is a label's bank and `Label & $FFFF` its offset in the bank.  In an object
they are relocations of that form, so nothing can be added afterwards: write `(Label + 2) >> 16`,
not `(Label >> 16) + 2`.

## Data

The 65C64 assembler's data directives (`.byte`, `.word`, `.long`, `.quad`, `.space`, `.align`),
and:

| Directive | Writes |
|---|---|
| `.word24 value, ...` | 24 bits, little endian: a far address without its pad byte |

A far pointer in C++ data is `.long Label`: 24 bits and a zero byte.

## Bank 0

Bank 0 holds only the stack, the direct page, the vectors and a `jml` stub for each vector.  Code
goes elsewhere, so a vector points at its stub:

```asm
		.section rodata.Vectors, at 0xFFFC
		.word ResetStub
		.section text.ResetStub, at 0xFF00
ResetStub:
		jml Start
```

## Everything else

The rest of the instruction set is the W65C816's mnemonics with these operands: implied (`clc`,
`tax`, `phb`, `xba`, `xce`, `wai`, `stp` ...), `A`, a value, or an address.  `brk`, `cop` and `wdm`
take a signature byte (`brk 0`).

The assembler's opcode table is checked against MicroprocessorLib's W65C816 simulator by
`TestAsmW65C816Opcodes`.

## Using the assembler from C++

```cpp
CAsmW65C816Assembler	cAssembler;
CAsm65Object			cObject;

cObject.Init("main");
cAssembler.Init();
bResult = cAssembler.AssembleObject(szSource, &cObject);		//Sections and relocations, for the linker.
cAssembler.Kill();
```

`Assemble` assembles absolute code (with `.org`) instead.  The W65C816 assembler shares its passes,
tokeniser, expressions, symbols, errors, sections and output with the 65C64's through
`CAsm65BaseAssembler`.
