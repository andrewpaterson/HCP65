# add, sub, cmp, neg, abs - Arithmetic

| Mnemonic | Form | Operation | Flags |
|---|---|---|---|
| `add` | Unsigned | O1 + O2 → O1 | n, v, z, c |
| | Signed | O1 + O2:s → O1 | n, v, z, c |
| `sub` | Unsigned | O1 - O2 → O1 | n, v, z, c |
| | Signed | O1 - O2:s → O1 | n, v, z, c |
| `cmp` | Unsigned | O1 - O2 | n, v, z, c |
| | Signed | O1 - O2:s | n, v, z, c |
| `neg` | | 0 - R2:s → R1 | n, z |
| `abs` | | abs(R2:s) → R1 | n, z |

`add` and `sub` add the second operand to the first or subtract it from the first.  `cmp`
subtracts without writing the result, only setting the flags.

They replace the W65C816's `adc`, `sbc`, `cmp`, `cpx` and `cpy`, and with a small value `inc`,
`inx`, `iny`, `dec`, `dex` and `dey`.

## Width and flags

The operation is the width of the register operand, or 32 bits with a memory operand and a
small value.  The signed forms sign extend a narrower second operand (written with `:s`).

| Flag | Meaning |
|---|---|
| n | The result's top bit |
| v | Signed overflow |
| z | The result is zero |
| c | `add`: the carry out.  `sub` and `cmp`: not borrow (set when O1 ≥ O2, unsigned) |

The carry is never an input: `add` and `sub` always add and subtract without it.  On a 64 bit
ALU a carry in is rarely needed, and it can be built from other instructions.

```asm
	wth A, 16
	cpy A, $FFFF
	add A, 1		; A = 0: z and c set
	cpy A, $7FFF
	add A, 1		; A = $8000: n and v set
	cpy A, 5
	sub A, 7		; A = $FFFE: n set, c clear (a borrow)
```

`cmp` sets v as `sub` does, so after a compare O1 < O2 as signed values when n ≠ v.  Branch on
it with `braso (n, v)` or `brane (n, v)` (see [Branches](branch.md)):

```asm
	cmp A, B
	braso (n, v), Less	; A < B, signed
	brac c, Lower		; A < B, unsigned
```

## Forms

| Source → destination | Assembly | Modes |
|---|---|---|
| Register, register | `add R1, R2` | 0 |
| Register, value | `add R, val` | 1, 15 |
| Register, memory | `add R, [address]` | 2–14 |
| Memory, register | `add [address], R` | 2–14 |
| Memory, small value | `add [address], val` | 2–10, 12 |

These are the operands of [`cpy`](cpy.md#forms), with these differences:

- Only -8 to 8 is embedded with a register (1 to 8 and -8 to -1, and 0 is the zero register).
  Other values are immediates.
- A single register address (`[addr + RA]`, `[offs + RA]`, `[RA]`, `[[RA]]` …) takes only V, X or
  Y as RA.  W isn't allowed: by the calling convention W is 0, so write `[n]`.
- In the two register modes, RA1 is A, B, S or V, and RA2 is only X or Y.

```asm
	add X, Y
	add A, B:s
	add A, 8		; embedded
	add A, 9		; 8 bit immediate
	sub W, 8
	add A, [$1234 + X]
	add A, [12 + V]
	add A, [V + X]
	add [$10:16], 1
	cmp [12 + V], -1
```

## neg and abs

```asm
	neg R1, R2
	neg R
	abs R1, R2
	abs R
```

`neg` puts 0 - R2 into R1, and `abs` puts the absolute value of R2 into R1.  R2 is sign extended
to R1's width.  With one register, R is both.  R1 and R2 are any of A, B, S, V, W, X and Y.  One
word, one cycle.

n and z are set from R1, except that `abs` sets n when R2 was negative.

```asm
	neg A, B
	abs X
	abs Y, B		; n set if B was negative
```

`neg` and `abs` are support for signed [multiply and divide](multiply-divide.md), which are
unsigned.
