# mul, div - Multiply and Divide

| Mnemonic | Operation | Flags |
|---|---|---|
| `mul` | A × B → Y | v, z |
| `div` | Y ÷ A → Y, remainder B | v, z, c |

Unsigned multiply and divide on fixed registers.  Each does one step of the operation every time
it runs and leaves C on its opcode until the last step, so one `mul` or `div` repeats on its own
until it's done.  It can be interrupted between steps.

Both take no operands.  W counts the steps and must be 0 at the start.  Every step runs in full:
nothing is cut short because a value is zero.  A step is one cycle.

For signed operands, take the [absolute values](arithmetic.md#neg-and-abs) first and fix the
sign of the result afterwards.

## mul

```asm
	mul
```

| Register | Before | After |
|---|---|---|
| A | Multiplicand | Unchanged |
| B | Multiplier | Unchanged |
| W | 0 | 2 × B's width in bytes |
| Y | | A × B, truncated to Y's width |

Each step multiplies A by one byte of B, two nybbles, and adds it into Y, so there's a step for
each byte of B: two steps for a 16 bit B.  The first step clears Y.

| Flag | Meaning |
|---|---|
| v | Done: Y holds the product |
| z | Y is 0 |

```asm
	wtth (A, B), 16
	wth Y, 32
	cpy A, $6A8
	cpy B, $7C
	cpy W, 0
	mul			; Y = $33960
```

## div

```asm
	div
```

| Register | Before | After |
|---|---|---|
| A | Divisor | Unchanged |
| Y | Dividend | Quotient |
| B | 0 (or the top half of the dividend) | Remainder |
| W | 0 | 2 × Y's width in bytes |

Base 16 long division, a nybble a step, so there's a step for each nybble of Y: four steps for
a 16 bit Y.  Each step moves Y's top nybble into the remainder B, subtracts the largest multiple
of A that fits (at most 15), and moves that quotient nybble into the bottom of Y.

B is usually 0.  Any B below A is the top half of the dividend, and the quotient still fits in
Y: (B × 2<sup>y</sup> + Y) ÷ A.  The remainder is truncated to B's width every step.

| Flag | Meaning |
|---|---|
| v | Done: Y holds the quotient and B the remainder |
| z | Y is 0 |
| c | The remainder B isn't 0 |

Dividing by 0 gives quotient nybbles of 15 (all ones) and leaves the dividend in B.

```asm
	wth A, 16
	wth B, 16
	wth Y, 16
	cpy A, 7
	cpy B, 0
	cpy Y, 1000
	cpy W, 0
	div			; Y = 142, B = 6
```
