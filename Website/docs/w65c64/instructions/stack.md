# psh, pul - Stack

| Form | Operation | Flags |
|---|---|---|
| Push registers | {A → [S]; S-a → S}; {B → [S]; S-b → S}; … {Y → [S]; S-y → S} | n, z |
| Push operand | O2 → [S]; S-o → S | |
| Push processor state | P → [S]; S-2 → S | |
| Push program count | C → [S]; S-4 → S | |
| Pull registers | {S+a → S; [S] → A}; {S+b → S; [S] → B}; … {S+y → S; [S] → Y} | n, z |
| Pull into address | S+4 → S; [S] → O2 | |
| Pull processor state | S+2 → S; [S] → P | all |

Push values onto a stack and pull them off.  The stack is S, or any of A, B, V, W, X and Y.  They
replace the W65C816's `pha`, `phx`, `phy`, `phd`, `php`, `pea`, `pei`, `per`, `pla`, `plx`, `ply`,
`pld` and `plp`.

## The stack

The stack grows down.  Pushing n bytes stores them, little endian, at S-n+1 to S, and then
subtracts n from S.  Pulling n bytes adds n to S, then reads S-n+1 to S.  After reset S is
`0001:FFFF`.

## Registers

```asm
	psh R
	psh (R, R, …)
	pul R
	pul (R, R, …)
```

One instruction pushes or pulls any set of A, B, S, V, W, X and Y: one word.  Registers are
always pushed in the order A, B, S, V, W, X, Y, and pulled in the reverse order Y, X, W, V, S,
B, A, whatever order they're written in.  Each is pushed or pulled at its width.  The values
pushed are the ones from before the stack register moves.

n and z are set from the last register pushed or pulled.

```asm
	wth A, 16
	wth B, 32
	psh (A, B), S		; 2 bytes of A, then 4 of B: S moves down 6
	pul (A, V, W), S
	psh A
	psh A, X		; push A on the stack in X
	pul (A, B), V
```

## Operands

```asm
	psh value
	psh value:width
	psh [address]
	pul [address]
```

- An immediate is pushed at its own width, unsigned: the narrowest that holds it, or the
  width given (8, 16, 24, 32, 40, 48 or 64 bits).
- A value in memory is read and pushed as 32 bits, like the W65C816's `pei`.
- `pul [address]` pulls 32 bits into memory.

The address is any [addressing mode](../operands.md#memory-addresses) but the two register ones
(11, 13 and 14).

```asm
	psh $1234
	psh 5:16
	psh 300:32
	psh [$40 + X], V
	psh [[$40]]
	pul [$40], W
```

## P and C

```asm
	psh P
	psh C
	psh (C, P)
	pul P
```

P is 2 bytes and C is 4.  `psh C` pushes the address of the next instruction.  To push another
address relative to the code, push C and then add to the value on the stack.  There's no
`pul C`: pull C with [`ret`](return.md).  Pulling P sets every flag in it, including the
register widths.

```asm
	psh P
	psh C
	psh (C, P), V
	psh C, V
	pul P
```
