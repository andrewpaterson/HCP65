# sep, cep, stat, bist, stab - Status

| Mnemonic | Name | Operation | Flags |
|---|---|---|---|
| `sep` | Set state bits | P ∨ E → P | the flags named |
| `cep` | Clear state bits | P ∧ !E → P | the flags named |
| `stat` | Status to status | p2 → p1 | the flag written |
| `bist` | Bit to status | R:b[N] → p | the flag written |
| `stab` | Status to bit | p → R:b[N]; {p → R:b[N±1] … R:b[N±(count-1)]} | |

Set, clear and copy the status flags, and move them to and from register bits.

## sep and cep

```text
	sep flag
	sep (flag, flag, …)
	cep flag
	cep (flag, flag, …)
```

Set or clear any of the flags e, l, f, g, n, v, z and c, replacing the W65C816's `sep`, `rep`,
`sec`, `clc`, `sei` and `cli`.  One word.  The register widths are set with [`wth`](width.md)
instead.

```asm
	sep c			; sec
	cep c			; clc
	sep (e, l)		; disable interrupts
	cep l			; enable the level interrupt
	sep (c, n)
```

## stat

```text
	stat destination, source
```

Copy one status flag into another.  Both are c, z, n, v, f or g, and the source can also be 0
or 1.

```asm
	stat f, c		; save c in f
	stat c, 1		; set c
```

## bist

```text
	bist flag, (R, index)
```

Copy bit `index` of register R into a flag.  The flag is c, z, n, v, f or g.  R is any of A, B,
S, V, W, X and Y.  The index is:

- 0 to 5: embedded, one word,
- 6 to 63: an immediate, two words,
- a register: two words.

```asm
	bist f, (A, 3)
	bist c, (A, 40)
	bist c, (A, X)
```

## stab

```text
	stab (R, index), flag
	stab (R, index, count), flag
	stab (R, index, count, +1), flag
	stab (R, index, count, -1), flag
```

Copy a flag into bit `index` of register R.  With a count, it fills `count` bits from `index`,
going up (`+1`, the default) or down (`-1`).  The flag is 0, 1, c, z, n, v, f or g, so `stab`
can also set or clear a run of bits.

- R is any of A, B, S, V, W, X and Y.  The register's width is ignored, and bits outside 0 to
  63 are dropped.
- The index is 0 to 63 or a register, and the count is 0 to 63 or a register.  The index
  register isn't changed.
- Without a count, an index of 0 to 5 is embedded: one word.  Otherwise it's two words.
- 32 or more bits take a cycle more.

```asm
	stab (A, 0), f
	stab (A, X), c
	stab (A, 40), 1
	stab (A, 2, 10), 1	; set bits 2 to 11
	stab (A, Y, X, -1), n	; copy n into X bits from bit Y down
```

## Encoding

`sep` and `cep` are instruction code 11.  `stat`, `bist` and `stab` are instruction code 15.  See
[Opcode Encoding](../opcode-encoding.md) for the bits, and [Sizes and Timing](../timing.md) for
the words and cycles.
