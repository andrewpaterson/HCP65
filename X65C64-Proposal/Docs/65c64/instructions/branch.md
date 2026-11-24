# bras, brac, brasa, braso, brae, brane - Branch

| Mnemonic | Name | Operation |
|---|---|---|
| `bras` | Branch on set | C+i → C; if p {C + O2 → C} |
| `brac` | Branch on clear | C+i → C; if !p {C + O2 → C} |
| `brasa` | Branch on set and set | C+i → C; if (p1 ∧ p2) {C + O2 → C} |
| `braso` | Branch on set xor set | C+i → C; if (p1 ⊻ p2) {C + O2 → C} |
| `brae` | Branch on equal | C+i → C; if (p1 = p2) {C + O2 → C} |
| `brane` | Branch on not equal | C+i → C; if (p1 ≠ p2) {C + O2 → C} |

Branch to a label (or a byte offset from the next instruction) when a status flag, or a pair of
flags, holds.  No flags are changed.

## Forms

```text
	bras flag, target
	brac flag, target
	brasa (flag1, flag2), target
	braso (flag1, flag2), target
	brae (flag1, flag2), target
	brane (flag1, flag2), target
```

| Instruction | First flag | Second flag | Embedded offsets (one word) |
|---|---|---|---|
| `bras`, `brac` | 0, 1, c, z, n, v, f or g | | 2, 4, 6, 8, -4, -6, -8, -10 |
| `brasa`, `braso` | 0, 1, c, z, n, v, f or g | c, z, n or v | 2, 4, 6, 8, -4, -6, -8, -10 |
| `brae`, `brane` | c, z, n, v, f or g | c, z, n, v, f or g | -8, -4, 4, 8 |

Any other offset is a signed 8, 16, 24 or 32 bit immediate after the opcode.  The flags 0 and
1 are constants, so `bras 1, target` always branches.

As with [`bra`](jump.md#relative-targets), a target without a label is a byte offset from the
next instruction, and C can be written in it: `brac z, -8` and `brac z, C - 8` are the same.

```asm
	brac z, Loop
	brac z, -8
	bras c, 8
	bras c, 100
	brasa (z, c), C + 2
	braso (n, v), C + 1000
	brae (c, z), 8
	brane (n, v), -4
	brae (f, g), 100
Loop:
```

## Conditions

| W65C816 | 65C64 | Branches when |
|---|---|---|
| `bcs` | `bras c, target` | c set |
| `bcc` | `brac c, target` | c clear |
| `beq` | `bras z, target` | z set |
| `bne` | `brac z, target` | z clear |
| `bmi` | `bras n, target` | n set |
| `bpl` | `brac n, target` | n clear |
| `bvs` | `bras v, target` | v set |
| `bvc` | `brac v, target` | v clear |

After a `cmp O1, O2` (see [`cmp`](arithmetic.md)):

| Condition | Branch |
|---|---|
| O1 = O2 | `bras z, target` |
| O1 ≠ O2 | `brac z, target` |
| O1 < O2, unsigned | `brac c, target` |
| O1 ≥ O2, unsigned | `bras c, target` |
| O1 < O2, signed | `braso (n, v), target` or `brane (n, v), target` |
| O1 ≥ O2, signed | `brae (n, v), target` |

## Example

The spreadsheet's block move: copy W bytes, a multiple of 8, from V + 8 to A + 8, a 64 bit
value at a time.  The loop runs from offset W down to 8 and stops when W reaches 0.

```asm
	wth A, 32
	wth B, 64
	pul (A, V, W), S
Loop:
	cpy B, [V + W]
	cpy [A + W], B
	sub W, 8
	brac z, Loop		; brac z, -8
```

## Encoding

The branches are instruction code 12: `bras`, `brac`, `brasa` and `braso` are extend 0 with an
embedded offset and extend 1 with an immediate, and `brae` and `brane` are extend 2.  See [Opcode
Encoding](../opcode-encoding.md) for the bits, and [Sizes and Timing](../timing.md) for the words
and cycles.
