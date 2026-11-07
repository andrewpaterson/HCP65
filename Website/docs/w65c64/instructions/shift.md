# shl, shr - Shift

| Mnemonic | Operation | Flags |
|---|---|---|
| `shl` | O1 ‹‹ O2 → O1 | z, c, v |
| `shr` | O1 ›› O2 → O1 | n, z, c, v |

Shift the first operand left or right by the number of bits in the second.  Zeros are shifted
in.  They replace the W65C816's `asl` and `lsr`.  There's no rotate or arithmetic shift right,
but they can be built (see the [examples](#examples)).

## Width and flags

The shift is the width of the first operand's register.  A shift of memory is 32 bits.

| Flag | `shl` | `shr` |
|---|---|---|
| c | The last bit shifted out | The last bit shifted out |
| v | The bit shifted out before it | The bit shifted out before it |
| z | The result is zero | The result is zero |
| n | Unchanged | The top bit *before* the shift |

A shift of 0 clears c and v.

```asm
	wth A, 16
	cpy A, $C001
	shl A, 1		; A = $8002, c set
	cpy A, $8003
	shr A, 1		; A = $4001, c set, n set (A was negative)
```

## Forms

| Assembly | Shift amount |
|---|---|
| `shl R1, R2` | The register R2 |
| `shl R, amount` | 1 to 8 embedded, otherwise an 8 or 16 bit immediate (0 to 65535) |
| `shl R, [address]` | The value in memory |
| `shl [address], R` | The register R |
| `shl [address], amount` | 1 to 8, embedded |

- In the register forms R1 and R2 are any of A, B, S, V, W, X and Y.
- With memory, R is A, B, S or V.
- A single register address takes only V, X or Y as its register.  In the two register modes RA1
  is A, B, S or V, and RA2 is X or Y, and there's no form with an amount.

```asm
	shl A, 1
	shr A, X
	shl A, 20
	shl A, [$40]
	shl [12 + V], A
	shl [$40], 8
	shl [V + X], A
```

## Examples

A shift right that keeps the sign (the spreadsheet's arithmetic shift), shifting A right by X
bits.  `shr` sets n from A before it shifts, and `stab` copies n into the top X bits:

```asm
	shr A, X		; n = A's sign; A = A >> X
	gwth Y, (A, -1)		; Y = the index of A's top bit
	stab (A, Y, X, -1), n	; copy n into X bits from bit Y down
```

Rotates through carry (the W65C816's `rol` and `ror`), using the user flag f to hold the old
carry:

```asm
	stat f, c		; rol A
	shl A, 1
	stab (A, 0), f

	stat f, c		; ror A
	gwth X, (A, -1)
	shr A, 1
	stab (A, X), f
```
