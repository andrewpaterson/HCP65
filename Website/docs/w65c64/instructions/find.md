# fsbi, fcbi, fscr, fccr, fsny, fcny, fsby, fcby - Find

| Mnemonic | Name | Operation | Flags |
|---|---|---|---|
| `fsbi` | Find set bit | if c {N+1 → N}; while R:b[N] = 0, N+1 → N | v, c |
| `fcbi` | Find clear bit | if c {N+1 → N}; while R:b[N] = 1, N+1 → N | v, c |
| `fscr` | Find set crumb | As `fsbi`, for 2 bit crumbs with any bit set | v, c |
| `fccr` | Find clear crumb | As `fcbi`, for 2 bit crumbs with any bit clear | v, c |
| `fsny` | Find non-zero nybble | if c {N+1 → N}; while ∨R:n[N] = 0, N+1 → N | v, c |
| `fcny` | Find zero nybble | if c {N+1 → N}; while ∧R:n[N] = 1, N+1 → N | v, c |
| `fsby` | Find non-zero byte | if c {N+1 → N}; while ∨R:[N] = 0, N+1 → N | v, c |
| `fcby` | Find zero byte | if c {N+1 → N}; while ∧R:[N] = 1, N+1 → N | v, c |

Scan a register for a unit (bit, crumb, nybble or byte) with any bit set (the *find set*
forms) or any bit clear (the *find clear* forms).  The scan starts at the unit indexed by N and
moves up or down.  N is left at the unit found.

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

```asm
	fsbi R, N
	fsbi R, N, +1
	fsbi R, N, -1
```

R is the register scanned and N the index register: any of A, B, S, V, W, X and Y.  `+1` scans up
(the default) and `-1` down.

```asm
	fsbi A, X
	fcby B, Y, -1
	fccr A, X
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
