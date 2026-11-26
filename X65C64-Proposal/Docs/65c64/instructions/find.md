# fsbi, fcbi, fscr, fccr, fsny, fcny, fsby, fcby, fzcr, fzny, fzby, ffcr, ffny, ffby - Find

--8<-- "find.txt"

Scan a register for a unit (bit, crumb, nybble or byte) that has:

| What | A bit | A crumb, nybble or byte |
|---|---|---|
| Any bit set | `fsbi`, *find set* | `fscr` `fsny` `fsby`, *find non-zero* |
| Any bit clear | `fcbi`, *find clear* | `fccr` `fcny` `fcby`, *find non-full* |
| All bits clear | (as `fcbi`) | `fzcr` `fzny` `fzby`, *find zero* |
| All bits set | (as `fsbi`) | `ffcr` `ffny` `ffby`, *find full* |

For a single bit, any and all are the same, so there are no zero or full bit forms.  The scan
starts at the unit indexed by N and moves up or down; the zero and full finds only move up.
N is left at the unit found.

## Width and flags

The scan covers only the register's width.  Unit 0 is the least significant.

- If c is set, N moves on one unit before the scan starts, so the next find carries on past the
  last unit found.
- c is always set afterwards, so a loop of finds visits each unit in turn.
- v is set when no unit was found.  N is then one past the end: the width in units going up,
  all ones (at N's width) going down.

One cycle.

```asm
	wth A, 16
	cpy A, $0110
	cpy X, 0
	cep c
	fsbi A, X		; X = 4
	fsbi A, X		; X = 8 (c was set, so it starts at 5)
	fsbi A, X		; X = 16, v set: no more
```

## Forms

```text
	fsbi R, N
	fsbi R, N, +1
	fsbi R, N, -1
	fzby R, N
	fzby R, N, +1
```

R is the register scanned and N the index register: any of A, B, S, V, W, X and Y.  `+1` scans up
(the default) and `-1` down.  The zero and full finds (`fz`, `ff`) take only `+1`.

```asm
	fsbi A, X
	fcby B, Y, -1
	fccr A, X
	fzby A, X
	ffcr B, Y, +1
```

## Example

Count the set bits of A in Y:

```asm
	cpy Y, 0
	cpy X, 0
	cep c
Loop:
	fsbi A, X
	bras v, Done
	add Y, 1
	sep c			; add cleared c: set it so the next find moves on
	bra Loop
Done:
```

Find the zero that ends a C string held in A, eight characters at a time:

```asm
	cpy X, 0
	cep c
	fzby A, X		; X = the length (the first zero byte), or 8 with v set: no zero yet
```

## Encoding

The any-bit finds are instruction code 14, extend 1.  The zero and full finds are in the bits
multiply and divide leave unused (instruction code 14, extend 0, sub instruction 0 for zero and 1
for full): bits 15-13 the register, 12-10 the index and 9-8 the unit (1 crumb, 2 nybble, 3 byte;
0 is multiply or divide).  See [Opcode Encoding](../opcode-encoding.md) for the bits, and
[Sizes and Timing](../timing.md) for the words and cycles.
