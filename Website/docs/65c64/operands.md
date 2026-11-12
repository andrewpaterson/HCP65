# Operands and Addressing

Most instructions take two operands.  The first (O1) is the destination and the second (O2)
the source:

```asm
	cpy A, B		; B → A
	cpy A, [$1000]		; load A from memory
	cpy [$1000], A		; store A into memory
```

At most one operand is memory.  The other is a register or, with some instructions, a value.

Each operand has one of 16 *operand access types*, held in 4 bits of the opcode:

| Mode | Name | Type | Assembly |
|---|---|---|---|
| 0 | [Register / None](#register-mode-0) | Register | `R` |
| 1 | [Immediate](#immediate-mode-1) | Constant | `value` |
| 15 | [Embedded](#embedded-mode-15) | Constant | `value` (small) |
| 2 | [Address Absolute](#address-absolute) | Address | `[addr]` |
| 2 | Address Absolute Register | Address | `[addr + R]` |
| 3 | Address Absolute Indirect | Address | `[[addr]]` |
| 4 | Address Absolute Register Indirect | Address | `[[addr + R]]` |
| 5 | Address Absolute Indirect Register | Address | `[[addr] + R]` |
| 6 | [Address Offset](#address-offset) | Address | `[offs]` |
| 6 | Address Offset Register | Address | `[offs + R]` |
| 7 | Address Offset Indirect | Address | `[[offs]]` |
| 8 | Address Offset Register Indirect | Address | `[[offs + R]]` |
| 9 | Address Offset Indirect Register | Address | `[[offs] + R]` |
| 10 | [Address Register](#address-register) | Address | `[R]` |
| 11 | Address Register Register | Address | `[R1 + R2]` |
| 12 | Address Register Indirect | Address | `[[R]]` |
| 13 | Address Register Register Indirect | Address | `[[R1 + R2]]` |
| 14 | Address Register Indirect Register | Address | `[[R1] + R2]` |

Absolute and Absolute Register are one mode.  Without a register, the zero register is
encoded, and the processor skips adding it.  Offset and Offset Register are one mode in the
same way.

## Operation width

An instruction works at the width of its register operand: if A is 16 bits, `cpy A, [$1000]`
reads 16 bits from memory.  When there's no register operand (a memory operand and a small
value), the operation is 32 bits wide.

If the second operand is narrower than the operation, it's zero extended, or sign extended
with the `:s` suffix.  If it's wider, its high bits are dropped.  The first operand is never
extended.

```asm
	cpy A, B		; B zero extended or truncated to A's width
	cpy A, B:s		; B sign extended to A's width
```

## Register (mode 0)

Any of A, B, S, V, W, X and Y.  Mode 0 with register code 0 is the zero register, which is how
the assembler writes the value 0.

## Immediate (mode 1)

The value follows the opcode, little endian, in one to four 16 bit words.  The assembler uses
the narrowest width that holds the value.  A suffix picks another width:

| Suffix | Meaning |
|---|---|
| `:8` `:16` `:24` `:32` `:40` `:48` `:64` | An immediate of that width (`:56` isn't a width) |
| `:s8` `:s16` … `:s64` | A sign extended immediate of that width |
| `:s` | Sign extend |

A negative value sign extends without a suffix.  A value takes ceil(bytes ÷ 2) words, so a
24 bit immediate takes two words, like a 32 bit one.

```asm
	cpy A, 200		; 8 bit immediate, 1 word
	cpy A, 8192		; 16 bit immediate, 1 word
	cpy A, $1a2b3c4d	; 32 bit immediate, 2 words
	cpy A, -32768		; 16 bit immediate, sign extended: $FFFF8000 in a 32 bit A
	cpy A, 5:24		; 24 bit immediate, 2 words
```

Only one suffix is allowed: `:s:16`, `:16:s` and `:16s` are errors.

## Embedded (mode 15)

A small value held in the opcode itself, with no immediate word, like the W65C816's `inc` and
`dec`.  The assembler embeds any value that fits.

| Instruction | With a register | With memory |
|---|---|---|
| `cpy`, `and`, `or`, `xor`, `tst` | -16 to 16 | -8 to 8 |
| `add`, `sub`, `cmp` | -8 to 8 | -8 to 8 |
| `shl`, `shr` | 1 to 8 | 1 to 8 |

Embedded values are signed and sign extend to the operation's width.  0 is the zero register
(mode 0), not an embedded value.  In the opcode a field of n bits holds 1 to 2<sup>n-1</sup>
as the value less one, and -2<sup>n-1</sup> to -1 as the value plus 2<sup>n</sup>: a 4 bit
field holds 1 to 8 as 0 to 7 and -8 to -1 as 8 to 15.

The branches, `bist`, `stab`, `nop`, `scp` and `scb` embed values of their own, described on
their pages.

A value used with memory must be embedded: there's no form that writes an immediate to
memory.  Copy a larger value into a register first.  A small value written to memory is
always 32 bits.

```asm
	cpy A, 7		; embedded
	cpy A, -16		; embedded
	cpy A, 17		; too big to embed: an 8 bit immediate
	cpy [$1000], -1		; 32 bits of $FF written to $1000
```

## Memory addresses

Addresses are 32 bits.  Opcodes and their operand words are aligned to 16 bits, but data can
be at any address.  A register used as an address is its value at its width, zero extended to
32 bits.  The pointer read by an indirect mode is always 32 bits.

In assembly the address's value comes first and the register second: `[4 + V]`, not
`[V + 4]`.

### Address Absolute (modes 2–5) {#address-absolute}

The 32 bit address follows the opcode in two words: low word, then high word.

| Mode | Assembly | Effective address |
|---|---|---|
| 2 | `[addr]` | addr |
| 2 | `[addr + R]` | addr + R |
| 3 | `[[addr]]` | [addr] |
| 4 | `[[addr + R]]` | [addr + R] |
| 5 | `[[addr] + R]` | [addr] + R |

R is any of A, B, S, V, W, X and Y.

### Address Offset (modes 6–9) {#address-offset}

The same, with a single 16 bit offset word.  The offset is the address `0000:offs`, so an
offset reaches the first 64KB of memory and saves a word.

| Mode | Assembly | Effective address |
|---|---|---|
| 6 | `[offs]` | 0000:offs |
| 6 | `[offs + R]` | 0000:offs + R |
| 7 | `[[offs]]` | [0000:offs] |
| 8 | `[[offs + R]]` | [0000:offs + R] |
| 9 | `[[offs] + R]` | [0000:offs] + R |

R is any of A, B, S, V, W, X and Y.

### Choosing absolute or offset

The assembler uses an offset when the address is below `0001:0000`, and absolute otherwise.  A
suffix on the address picks one:

| Suffix | Meaning |
|---|---|
| `:16` | Offset.  A wider value keeps its low 16 bits, with a warning. |
| `:32` | Absolute.  A wider value keeps its low 32 bits, with a warning. |

```asm
	cpy A, [$7081]		; offset, 1 word
	cpy A, [$5e6f7081]	; absolute, 2 words
	cpy A, [$7081:32]	; absolute, 2 words
	cpy A, [Table + X]	; offset or absolute, depending on where Table is
Table:
```

A negative address, down to -$80000000, is absolute and is 32 bit two's complement.  Addresses
wrap at 32 bits, so a negative address plus a register points below the register:

```asm
	cpy A, [-4 + V]		; the value at V - 4
```

When assembling an object for the linker, a label's address is 32 bits unless `:16` is given.

### Address Register (modes 10–14) {#address-register}

The address is in registers, so no operand word follows the opcode.

| Mode | Assembly | Effective address | Registers |
|---|---|---|---|
| 10 | `[R]` | R | R: A, B, S, V, W, X or Y |
| 11 | `[R1 + R2]` | R1 + R2 | R1: A, B, S or V.  R2: V, W, X or Y |
| 12 | `[[R]]` | [R] | R: A, B, S, V, W, X or Y |
| 13 | `[[R1 + R2]]` | [R1 + R2] | R1: A, B, S or V.  R2: V, W, X or Y |
| 14 | `[[R1] + R2]` | [R1] + R2 | R1: A, B, S or V.  R2: V, W, X or Y |

For modes 11 and 13 the assembler takes the two registers in either order, so `[X + V]` is
`[V + X]`.  Modes 11, 13 and 14 have no room in the opcode for a small value, so they can't
take one.

```asm
	cpy A, [V + X]
	cpy B, [X + V]		; the same address as [V + X]
	cpy X, [[A + W]]
	cpy V, [[B] + Y]
```

## Restrictions

Some instructions narrow these choices.

- `add`, `sub`, `cmp`, `shl`, `shr` and `cpyx` take only V, X or Y (or no register) as the
  register in a single register address (`[addr + R]`, `[offs + R]`, `[R]` …).  W isn't
  allowed: by the [calling convention](calling-convention.md) W is 0, so write `[n]` for
  `[n + W]`.  `cpy` and the logical instructions take any register.
- In a two register address (`[R1 + R2]`, `[[R1 + R2]]`, `[[R1] + R2]`), `add`, `sub`, `cmp`,
  `shl`, `shr`, `jmp` and `jsr` take only X or Y as R2.  `cpy` and the logical instructions
  take V, W, X or Y.
- `shl` and `shr` with a memory operand take only A, B, S or V as the register operand.
- `cpyx` takes only the modes `[addr]`, `[addr + R]`, `[[addr]]`, `[[addr + R]]`, `[R]` and
  `[[R]]`.
- `psh` and `pul` of a value in memory, and a small value copied or combined into memory, can't
  use a two register address.
- `cpy`, `and`, `or`, `xor` and `tst` can't sign extend a value read from memory: `cpy A,
  [$1000]:s` is an error.  `add`, `sub` and `cmp` can.

## Timing

An instruction takes:

- 1 cycle for the opcode,
- 1 cycle per operand word,
- 2 or 3 cycles to read a 32 bit pointer (the indirect modes),
- 1 cycle per word of data read, and
- 1 cycle per word of data written.

Address calculation takes no cycles.  An access to an unaligned value takes one cycle per word
it touches, so an unaligned 16 bit value takes two.
