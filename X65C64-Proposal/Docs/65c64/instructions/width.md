# wth, wtth, gwth, wthx - Register Width

| Mnemonic | Name | Operation | Flags |
|---|---|---|---|
| `wth` | Set register width | O → r | a, b, x, y |
| `wtth` | Set two register widths | O2 → r1; O2 → r2 | a, b, x, y |
| `gwth` | Get register width | 8r → R; {8r - 1 → R} | |
| `wthx` | Extend register width | extend(R) → R; O → r | a, b, x, y |

A, B, X and Y are each 8, 16, 32 or 64 bits wide (see [Register widths](../registers.md#register-widths)).
These instructions set, read and extend the widths, in place of the W65C816's `rep` and `sep`
on m and x.  Widths are written in bits.

## wth and wtth

```text
	wth R, bits
	wth R, register
	wtth (R1, R2), bits
	wtth (R1, R2), register
```

Set the width of one register, or two, to 8, 16, 32 or 64 bits.  R, R1 and R2 are A, B, X or Y.
A width in a register is a number of bits, rounded up to the next width: 1 to 8 is 8 bits, 9 to
16 is 16, 17 to 32 is 32 and more is 64.

Only the width changes, not the register's bits.  Widening shows whatever was in the high bits
before, so use `wthx` to widen a value.

```asm
	wth A, 32
	wth B, X
	wtth (X, Y), 32
```

```asm
	wth A, 64
	cpy A, -1
	wth A, 8
	cpy A, 5		; only the low byte is written
	wth A, 64		; A = $FFFFFFFFFFFFFF05
```

## gwth

```text
	gwth R, Q
	gwth R, (Q, -1)
```

Put the width of Q (A, B, X or Y) in bits into R (any of A, B, S, V, W, X and Y).  With `-1`
it's one less: the index of Q's top bit.

```asm
	gwth Y, A		; 16 for a 16 bit A
	gwth Y, (A, -1)		; 15
```

## wthx

```text
	wthx R, bits
	wthx R, bits:s
```

Change the width of R (A, B, X or Y) and extend its value from the old width to the new one,
with zeros or, with `:s`, its sign.

```asm
	wth A, 8
	cpy A, $80
	wthx A, 32:s		; A = $FFFFFF80, 32 bits
	wthx A, 64		; A = $00000000FFFFFF80, 64 bits
```

## Encoding

`wth`, `wtth`, `gwth` and `wthx` are instruction code 11.  See [Opcode
Encoding](../opcode-encoding.md) for the bits, and [Sizes and Timing](../timing.md) for the words
and cycles.
