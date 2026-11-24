# cpyx - Copy Extend

| Form | Operation | Flags |
|---|---|---|
| Zero extend | O2:o → R | n, z |
| Sign extend | O2:o:s → R | n, z |

Copy a value of a given width between memory and a register, whatever the register's width.
Where [`cpy`](cpy.md) always reads and writes memory at the register's width, `cpyx` names the
width of the value in memory: 8, 16, 32 or 64 bits.  It loads a byte into a 64 bit register, or
stores the low 16 bits of a 32 bit one.

## Width and flags

- **Memory → register.**  The value is zero extended to the register's width, or sign extended
  with `:s8`, `:s16` or `:s32`.  A value wider than the register is truncated.
- **Register → memory.**  The register's low bits are written at the data width.  If the data
  width is wider than the register, the register is zero extended.  There's no sign extending
  store.

n and z are set from the value copied: at the register's width when loading, and at the data
width when storing.

```asm
	wth A, 64
	cpyx A, [$3000]:8	; $85 → A = $0000000000000085
	cpyx A, [$3000]:s8	; $85 → A = $FFFFFFFFFFFFFF85
	wth B, 16
	cpyx [$3010]:32, B	; B's 16 bits written as 32
```

## Forms

```text
	cpyx R, [address]:bits
	cpyx R, [address]:sbits
	cpyx [address]:bits, R
```

| Part | Allowed |
|---|---|
| R | A, B, X or Y |
| bits | 8, 16, 32 or 64 |
| sbits | s8, s16 or s32 (loads only) |

The suffix goes after the memory operand.  `cpyx` takes only these addressing modes, with V, X
or Y as the address register RA:

| Mode | Address |
|---|---|
| 2 or 6 | `[addr]`, `[addr + RA]` |
| 3 or 7 | `[[addr]]` |
| 4 or 8 | `[[addr + RA]]` |
| 10 | `[RA]` |
| 12 | `[[RA]]` |

```asm
	cpyx A, [$1234]:16
	cpyx A, [12 + V]:8
	cpyx X, [$10 + V]:s16
	cpyx [$12345678 + X]:8, Y
	cpyx X, [[V]]:64
	cpyx Y, [Y]:s32
```

## Example

Walk a zero terminated string a byte at a time, with A at any width:

```asm
	wth X, 32
	cpy X, String
Loop:
	cpyx A, [X]:8		; one byte, z set at the terminator
	bras z, Done
	add X, 1
	bra Loop
Done:
	stp
String:
	.byte "Hello", 0
```

## Encoding

`cpyx` is instruction code 12, extend 3.  The sign extending load is instruction code 15, extend
6.  See [Opcode Encoding](../opcode-encoding.md) for the bits, and [Sizes and
Timing](../timing.md) for the words and cycles.
