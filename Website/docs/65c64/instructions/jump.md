# jmp, jsr, bra, brsr - Jump

| Mnemonic | Name | Operation |
|---|---|---|
| `jmp` | Jump absolute | {P → [S]; S-2 → S}; O2 → C |
| `jsr` | Jump absolute subroutine | C+i → C; C → [S]; S-4 → S; {P → [S]; S-2 → S}; O2 → C |
| `bra` | Jump relative (branch always) | C+i → C; {P → [S]; S-2 → S}; C + O2 → C |
| `brsr` | Jump relative subroutine | C+i → C; C → [S]; S-4 → S; {P → [S]; S-2 → S}; C + O2 → C |

`jmp` and `jsr` jump to an address.  `bra` and `brsr` jump to an offset from the next
instruction, so code that uses them can run at any address.  `jsr` and `brsr` push the return
address first: C, the address of the next instruction, 4 bytes.

Any of them can also push P (2 bytes, after C), so a subroutine can be called like an
interrupt and return with [`ret P`](return.md) or [`rti`](return.md).  They push on S, or on V, W
or Y.  No flags are changed.

They replace the W65C816's `jmp`, `jml`, `jsr`, `jsl`, `bra` and `brl`.

## Forms

```asm
	jmp target
	jmp target, P
	jmp target, stack
	jmp target, P, stack
```

The same for `jsr`, `bra` and `brsr`.  The stack is S (the default), V, W or Y.

## Absolute targets

The target of `jmp` and `jsr` is the address jumped to, written as a
[memory operand](../operands.md#memory-addresses) without its outer brackets.  So `jmp $1100`
jumps to `$1100`, and `jmp [$1100]` jumps to the address stored at `$1100`.

| Mode | Target | Jumps to |
|---|---|---|
| 2 or 6 | `addr`, `addr + R` | addr, addr + R |
| 3 or 7 | `[addr]` | [addr] |
| 4 or 8 | `[addr + R]` | [addr + R] |
| 5 or 9 | `[addr] + R` | [addr] + R |
| 10 | `R` | R |
| 11 | `R1 + R2` | R1 + R2 |
| 12 | `[R]` | [R] |
| 13 | `[R1 + R2]` | [R1 + R2] |
| 14 | `[R1] + R2` | [R1] + R2 |

R is any of A, B, S, V, W, X and Y.  In the two register modes R1 is A, B, S or V, and R2 is X
or Y.  A target below `0001:0000` is an offset (one word) unless it's written with `:32`.

```asm
	jmp A
	jmp $1100
	jsr $12345678
	jsr $1100, P
	jmp [$1234]
	jmp [$1234 + X]
	jmp Table + X
	jmp V + X
	jmp [V] + X
	jsr [V], P, Y
	jsr $1100, W
Table:
```

## Relative targets

The target of `bra` and `brsr` is a label, or a byte offset from the next instruction.  C, the
address of the next instruction, can be written in the offset: `bra C - 2` branches to
itself.

- A `bra` with no P and no stack, to within -64 to 63 bytes, is one word: the branch always.
- Otherwise the offset is a signed 8, 16, 24 or 32 bit immediate.

```asm
	bra Loop
	bra C - 2		; branch to itself
	bra 200
	brsr Sub
	brsr C + 200
	bra Here, P
Here:
Loop:
Sub:
```

## Example

A virtual machine's fetch and dispatch (the spreadsheet's VM fetch), with a 32 bit A and the
virtual program counter at `PC`:

```asm
	cpy A, [PC:16]		; fetch the next opcode
	shl A, 1		; times 2
	add [PC:16], 1		; move the program counter on
	jmp A			; jump to the opcode's handler
PC = $40
```
