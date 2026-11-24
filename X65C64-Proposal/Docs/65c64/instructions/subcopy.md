# scp, scb - Subcopy

| Mnemonic | Operation | Flags |
|---|---|---|
| `scp` | R1:[O1]…R1:[O1+N] → R2:[O2]…R2:[O2+N] | n, z |
| `scb` | R1:b[O1]…R1:b[O1+N] → R2:b[O2]…R2:b[O2+N] | n, z |

Copy a run of bytes (`scp`) or bits (`scb`) from one register to another.  The source run starts
at an index in the first register and the destination at an index in the second.  The
destination's other bytes or bits are unchanged.

Subcopy replaces the W65C816's transfers (`tax`, `txa`, `tcd`, `tsc`, `xba` …), and does far
more: it moves any field of one register into any place in another.

## Width and flags

Subcopy ignores the registers' set widths and works on all 64 bits (32 for S, V and W).
Source bytes or bits past 63 read as 0, and destination ones past 63 are dropped.  The source
is read before the destination is written, so a copy within one register works.

n and z are set from the destination register at its set width.  Two cycles: the opcode and
the index word.

```asm
	wth A, 64
	wth B, 64
	cpy A, $1122334455667788
	cpy B, 0
	scp (A, 2), (B, 0), 3	; B = $0000000000445566
```

## Forms

```text
	scp (R1, index1), (R2, index2), count
	scb (R1, index1), (R2, index2), count
```

R1 is the source and R2 the destination: any of A, B, S, V, W, X and Y.  Each index and the
count is either a value or a register (read at its width):

| | `scp` (bytes) | `scb` (bits) |
|---|---|---|
| Index | 0 to 7 | 0 to 63 |
| Count | 1 to 8 | 1 to 64 |

```asm
	scp (A, 2), (B, X), 4
	scp (A, X), (B, 0), Y
	scb (A, 40), (B, 3), 33
	scb (A, 4), (B, 60), 8	; only bits 60 to 63 of B are written
```

## Examples

Swap the bytes of a 16 bit A (the W65C816's `xba`), through B:

```asm
	scp (A, 0), (B, 1), 1
	scp (A, 1), (B, 0), 1
	scp (B, 0), (A, 0), 2
```

Copy the low byte of X into the third byte of A, leaving A's other bytes alone:

```asm
	scp (X, 0), (A, 2), 1
```

## Encoding

`scp` and `scb` are instruction code 9.  See [Opcode Encoding](../opcode-encoding.md) for the
bits, and [Sizes and Timing](../timing.md) for the words and cycles.
