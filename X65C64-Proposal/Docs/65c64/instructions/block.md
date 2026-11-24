# bcpy, band, btst, bor, bxor, badd, bsub, bcmp, bpcpy, bpand, bpor, bpxor - Block

| Mnemonic | Name | Operation | Flags |
|---|---|---|---|
| `bcpy` | Block copy | R → [V]; A-1 → A; V+r → V | v |
| `band` | Block and | [V] ∧ R → [V]; A-1 → A; V+r → V | n, v, z |
| `btst` | Block test | [V] ∧ R; A-1 → A; V+r → V | n, v, z |
| `bor` | Block or | [V] ∨ R → [V]; A-1 → A; V+r → V | n, v, z |
| `bxor` | Block xor | [V] ⊻ R → [V]; A-1 → A; V+r → V | n, v, z |
| `badd` | Block add | [V] + R → [V]; A-1 → A; V+r → V | n, v, z |
| `bsub` | Block subtract | [V] - R → [V]; A-1 → A; V+r → V | n, v, z |
| `bcmp` | Block compare | [V] - R; A-1 → A; V+r → V | n, v, z |
| `bpcpy` | Block precopy | [V]; R → [V]; A-1 → A; V+r → V | n, v, z |
| `bpand` | Block preand | [V] ∧ R → [V]; A-1 → A; V+r → V | n, v, z |
| `bpor` | Block preor | [V] ∨ R → [V]; A-1 → A; V+r → V | n, v, z |
| `bpxor` | Block prexor | [V] ⊻ R → [V]; A-1 → A; V+r → V | n, v, z |

Apply a register to every element of a block of memory: fill it, mask it, add to it, or search
it.  V points at the block, A counts the elements and R holds the value.

## How they run

Each step works on the element at V.  It loads the element into D, combines it with R, writes D
back (except `btst` and `bcmp`), moves V on by one element and subtracts 1 from A.  `bcpy` only
writes R, without reading.

The instruction leaves C on its opcode, so it repeats on its own until:

- A goes negative (v is set), so A + 1 elements are done, or
- stopping is allowed and the stop condition holds after a step.

It can be interrupted between steps.  A step takes the opcode cycle plus a cycle per word read
and written.

The element width is R's width (32 bits for S, V and W).

| Flag | Meaning |
|---|---|
| v | A is negative: the block is done |
| z | D is 0 |
| n | D is negative |

## The pre forms

The *pre* forms (`bpcpy`, `bpand`, `bpor` and `bpxor`) set n and z from the element as it was
read, before it's changed.  So their stop condition sees the original value.  `bpand` with $7F
stopping on n strips the bit 7 end marker from a string and stops at its last character.
`bpand` with $DF stopping on z upper cases a zero terminated string and stops at the terminator,
even if a character ands to zero.

`bcpy` sets only v, and never stops early.

## Forms

```text
	op R, V, direction, A
	op R, V, direction, A, stop
```

| Part | Allowed |
|---|---|
| R | B, S, V, W, X or Y (A is the count) |
| direction | `+1` (V moves up) or `-1` (V moves down) |
| stop | `z` (stop when z is set), `!z` (when z is clear), `n` (when n is set) or `!n` (when n is clear) |

One word.

```asm
	bcpy B, V, +1, A
	band X, V, -1, A, z
	btst X, V, -1, A, !z
	bpand B, V, +1, A, !n
```

## Examples

Fill 4 words with $ABCD:

```asm
	wth A, 32
	wth B, 16
	cpy V, $3000
	cpy A, 3		; 3 + 1 elements
	cpy B, $ABCD
	bcpy B, V, +1, A
```

Upper case a zero terminated string of up to 10 characters, stopping after the terminator:

```asm
	wth A, 32
	wth B, 8
	cpy V, String
	cpy A, 9
	cpy B, $DF
	bpand B, V, +1, A, z
String = $3000
```

## Encoding

The block instructions are instruction code 8, with bit 4 clear.  Bits 5 to 8 are the sub
instruction.  See [Opcode Encoding](../opcode-encoding.md) for the bits, and [Sizes and
Timing](../timing.md) for the words and cycles.
