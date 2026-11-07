# and, or, xor, tst - Logical

| Mnemonic | Form | Operation | Flags |
|---|---|---|---|
| `and` | Zero extend | O1 ∧ O2 → O1 | n, z |
| | Sign extend | O1 ∧ O2:s → O1 | n, z |
| `or` | Zero extend | O1 ∨ O2 → O1 | n, z |
| | Sign extend | O1 ∨ O2:s → O1 | n, z |
| `xor` | Zero extend | O1 ⊻ O2 → O1 | n, z |
| | Sign extend | O1 ⊻ O2:s → O1 | n, z |
| `tst` | Zero extend | O1 ∧ O2 | n, z |
| | Sign extend | O1 ∧ O2:s | n, z |

And, or or exclusive or the second operand into the first.  `tst` ands them without writing
the result, only setting the flags.

They replace the W65C816's `and`, `ora`, `eor` and `bit`.  With memory as the first operand,
`or` is `tsb`.  `xor R, -1` inverts a register.

## Width and flags

The operation is the width of the register operand, or 32 bits with a memory operand and a
small value.  A narrower second operand is zero extended, or sign extended with `:s`.

n and z are set from the result (for `tst`, the result that isn't written).

```asm
	wth A, 16
	cpy A, $F0F0
	and A, $FF00		; A = $F000
	xor A, -1		; A = $0FFF
	tst A, $F000		; z set: no bits in common
```

## Forms

The logical instructions take every operand that [`cpy`](cpy.md#forms) takes, with the same
registers and addressing modes:

| Source → destination | Assembly | Modes |
|---|---|---|
| Register, register | `and R1, R2` | 0 |
| Register, value | `and R, val` | 1, 15 |
| Register, memory | `and R, [address]` | 2–14 |
| Memory, register | `and [address], R` | 2–14 |
| Memory, small value | `and [address], val` | 2–10, 12 |

- A value from -16 to 16 with a register is embedded in the opcode.  Others are immediates.
- A small value with memory is -8 to 8.
- Memory can't be sign extended: `and A, [$40]:s` is an error.
- Every address register is allowed: A, B, S, V, W, X and Y, and in the two register modes A, B,
  S or V with V, W, X or Y.

```asm
	and A, B
	or A, B:s
	xor A, -1
	and A, 16		; embedded
	and A, 17		; 8 bit immediate
	and [$40], A
	or [$40 + W], 3
	xor [V + W], A
	tst A, [$1234 + X]
	tst [$40], 3
```

## Examples

Set and clear bits in memory, like the W65C816's `tsb` and `trb`:

```asm
	or [$40], A		; tsb: set A's bits in [$40]
	xor A, -1		; trb: invert A ...
	and [$40], A		; ... and clear its bits in [$40]
```
