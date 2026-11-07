# cpy - Copy

| Form | Operation | Flags |
|---|---|---|
| Zero extend | O2 → O1 | n, z |
| Sign extend | O2:s → O1 | n, z |

Copy the second operand into the first.

Usually the first operand (the destination) is a register and the second (the source) is any
of the [operand types](../operands.md): that loads a value, from memory or another register,
into the register.  Just as often the first operand is a memory address and the second a
register, which stores the register into memory.  A small value can also be stored straight
into memory.

`cpy` replaces the W65C816's `lda`, `ldx`, `ldy`, `sta`, `stx`, `sty`, `stz` and, through the
register form, most of the transfers.

## Width and flags

The copy is the width of the register operand.  Copying into memory writes that many bits, and
copying from memory reads that many.  A small value written to memory is 32 bits.

n and z are set from the value copied, at that width.

```asm
	wth A, 16
	cpy A, [$1000]		; reads 16 bits
	wth A, 64
	cpy A, [$1000]		; reads 64 bits
	cpy [$1000], 0		; writes 32 bits of 0, sets z
```

## Forms

| Source → destination | Assembly | Modes |
|---|---|---|
| [Register → register](#register-register) | `cpy R1, R2` | 0 |
| [Value → register](#value-register) | `cpy R, val` | 1, 15 |
| [Memory → register](#memory-and-registers) | `cpy R, [address]` | 2–14 |
| [Register → memory](#memory-and-registers) | `cpy [address], R` | 2–14 |
| [Small value → memory](#small-value-memory) | `cpy [address], val` | 2–10, 12 |

R, R1 and R2 are any of A, B, S, V, W, X and Y.

## Register → register

```asm
	cpy R1, R2
	cpy R1, R2:s
```

Copy register R2 into register R1.  One word.

If R1 is wider than R2, R1's high bits are filled with zeros, or with copies of R2's sign bit
when R2 is followed by `:s`.  If R1 is narrower, R2's high bits are dropped.

```asm
	cpy A, B
	cpy A, B:s
	cpy X, S
```

## Value → register

```asm
	cpy R, val
	cpy R, val:width
	cpy R, val:s
	cpy R, val:swidth
```

Copy a value into register R.

- A value from -16 to 16 is [embedded](../operands.md#embedded-mode-15) in the opcode: one
  word.  0 is the zero register.
- Any other value is an [immediate](../operands.md#immediate-mode-1) that follows the opcode,
  in the narrowest of 8, 16, 24, 32, 40, 48 or 64 bits that holds it.  A width suffix (`:8`
  `:16` `:24` `:32` `:40` `:48` `:64`) picks a wider one.  56 isn't a width.

If R is wider than the value, R's high bits are filled with zeros, or sign extended when the
value is negative or has an `:s` suffix.  If R is narrower, the value's high bits are dropped.

```asm
	cpy A, 8192		; $00002000 in a 32 bit A
	cpy A, -32768		; $FFFF8000
	cpy A, $1a2b3c4d	; $1A2B3C4D
	cpy A, $1a2b3c4d5e6f7081	; $5E6F7081: a 64 bit immediate, truncated
	cpy A, $4:s8		; $04 in an 8 bit A
	cpy A, -1:16		; $FFFF in a 16 bit A
	cpy A, 0		; $0000 in a 16 bit A (the zero register)
	cpy A, -8		; $FFF8 in a 16 bit A (embedded)
	cpy A, 7		; $0007 in a 16 bit A (embedded)
```

## Memory and registers

```asm
	cpy R, [address]
	cpy [address], R
```

Copy memory into register R, or register R into memory.  The value in memory is the same
width as R, so there's no extension: `:s` isn't allowed on a memory operand.

The address is any of the [memory addressing modes](../operands.md#memory-addresses).  In the
tables, RA, RA1 and RA2 are the address registers.

### Address Absolute

A 32 bit address follows the opcode: three words.  `:32` forces absolute addressing for an
address below `0001:0000`.

| Mode | Memory → register | Register → memory | Effective address |
|---|---|---|---|
| 2 | `cpy R, [addr]` | `cpy [addr], R` | addr |
| 2 | `cpy R, [addr + RA]` | `cpy [addr + RA], R` | addr + RA |
| 3 | `cpy R, [[addr]]` | `cpy [[addr]], R` | [addr] |
| 4 | `cpy R, [[addr + RA]]` | `cpy [[addr + RA]], R` | [addr + RA] |
| 5 | `cpy R, [[addr] + RA]` | `cpy [[addr] + RA], R` | [addr] + RA |

```asm
	cpy A, [$5e6f7081]
	cpy A, [0:32]
	cpy [$5e6f7081], A
	cpy [511:32], A
	cpy A, [$5e6f7081 + X]
	cpy [$5e6f7081 + X], A
	cpy A, [[$5e6f7081]]
	cpy [[$5e6f7081]], A
	cpy A, [[$1a2b3c4d + X]]
	cpy A, [[$3c4d:32 + B]]
	cpy [[1:32 + B]], A
	cpy A, [[$1a2b3c4d] + X]
	cpy [[33:32] + V], A
```

### Address Offset

A 16 bit offset follows the opcode, addressing `0000:offs`: two words.  `:16` forces offset
addressing, keeping the low 16 bits of a wider value (with a warning).

| Mode | Memory → register | Register → memory | Effective address |
|---|---|---|---|
| 6 | `cpy R, [offs]` | `cpy [offs], R` | 0000:offs |
| 6 | `cpy R, [offs + RA]` | `cpy [offs + RA], R` | 0000:offs + RA |
| 7 | `cpy R, [[offs]]` | `cpy [[offs]], R` | [0000:offs] |
| 8 | `cpy R, [[offs + RA]]` | `cpy [[offs + RA]], R` | [0000:offs + RA] |
| 9 | `cpy R, [[offs] + RA]` | `cpy [[offs] + RA], R` | [0000:offs] + RA |

```asm
	cpy A, [$7081]
	cpy A, [0:16]
	cpy [$7081], A
	cpy [511:16], A
	cpy A, [$7081 + X]
	cpy [$7081 + X], A
	cpy W, [[$7081]]
	cpy [[$6FE4]], A
	cpy A, [[$1a2b + X]]
	cpy [[1:16 + B]], A
	cpy A, [[33] + V]
	cpy [[$7081] + S], V
```

### Address Register

The address is in registers: one word.

| Mode | Memory → register | Register → memory | Effective address |
|---|---|---|---|
| 10 | `cpy R, [RA]` | `cpy [RA], R` | RA |
| 11 | `cpy R, [RA1 + RA2]` | `cpy [RA1 + RA2], R` | RA1 + RA2 |
| 12 | `cpy R, [[RA]]` | `cpy [[RA]], R` | [RA] |
| 13 | `cpy R, [[RA1 + RA2]]` | `cpy [[RA1 + RA2]], R` | [RA1 + RA2] |
| 14 | `cpy R, [[RA1] + RA2]` | `cpy [[RA1] + RA2], R` | [RA1] + RA2 |

RA is any of A, B, S, V, W, X and Y.  RA1 is A, B, S or V, and RA2 is V, W, X or Y.

```asm
	cpy A, [W]
	cpy [S], X
	cpy A, [V + X]
	cpy [S + V], W
	cpy A, [[V]]
	cpy [[V]], A
	cpy X, [[A + W]]
	cpy [[B + Y]], S
	cpy X, [[A] + W]
	cpy [[V] + X], A
```

## Small value → memory

```asm
	cpy [address], val
```

Copy a small value into memory.  The value is -8 to 8, embedded in the opcode, and it's always
written as 32 bits.  There's no form that writes an immediate to memory, so copy a larger value
into a register first.

Every addressing mode but 11, 13 and 14 (the two register modes) can take a small value.

| Mode | Assembly |
|---|---|
| 2 | `cpy [addr], val` |
| 2 | `cpy [addr + RA], val` |
| 3 | `cpy [[addr]], val` |
| 4 | `cpy [[addr + RA]], val` |
| 5 | `cpy [[addr] + RA], val` |
| 6–9 | The same with an offset |
| 10 | `cpy [RA], val` |
| 12 | `cpy [[RA]], val` |

```asm
	cpy [$5e6f7081], 0
	cpy [$5e6f7081], -1
	cpy [2346:32], 5
	cpy [$5e6f7081 + S], 1
	cpy [[$7081:32]], 8
	cpy [[1:32 + B]], 1
	cpy [[$5e6f7081] + X], -1
	cpy [$7081], -1
	cpy [$7081 + Y], 0
	cpy [[77]], 3
	cpy [[1 + B]], 1
	cpy [[255] + X], 7
	cpy [V], 5
	cpy [[S]], -2
```
