# Registers

| Register | Width | Usual role |
|---|---|---|
| A | 8, 16, 32 or 64 bits | Arithmetic and logic (accumulator) |
| B | 8, 16, 32 or 64 bits | Arithmetic and logic (accumulator) |
| X | 8, 16, 32 or 64 bits | Indexed addressing |
| Y | 8, 16, 32 or 64 bits | Indexed addressing |
| S | 32 bits | Stack pointer |
| V | 32 bits | Frame address (offset) |
| W | 32 bits | Frame address (offset) |
| C | 32 bits | Program counter |
| P | 16 bits | Processor status |

The roles are only the usual ones.  With a few restrictions (named on each instruction's page),
any of A, B, S, V, W, X and Y can be used as an accumulator, an offset, an index or a stack
pointer.

C, the program counter, is the address of the next instruction while an instruction runs.  It
can't be used as an ordinary operand.  It can only be pushed, and written as `C + offset` or
`C - offset` in a branch target.

## The zero register

Register code 0 is the zero register.  It reads as 0, and as an address register it adds
nothing.  The assembler uses it for the value 0 (`cpy A, 0`) and for addresses without a
register (`[addr]`).

| Code | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|---|---|---|---|---|---|---|---|---|
| Register | zero | A | B | S | V | W | X | Y |

## Register widths

A, B, X and Y are 64 bit registers whose width is set in P to 8, 16, 32 or 64 bits.  An
instruction on a register works at its width:

- Reading a register reads only the bits in its width.
- Writing a register changes only the bits in its width.  The bits above it keep their
  values.
- A register used as an address is its value at its width, zero extended to 32 bits.

```asm
	wth A, 16		; A is 16 bits
	wth B, 64		; B is 64 bits
	cpy B, -1		; B = $FFFFFFFFFFFFFFFF
	wth B, 8		; B is 8 bits
	cpy B, 5		; B = $FFFFFFFFFFFFFF05, only the low byte is written
```

`wth` and `wtth` set the widths, `gwth` reads one and `wthx` widens a register, sign or zero
extending it.  S, V, W and C are always 32 bits.

## Status register P

| Bits | 15–14 | 13–12 | 11–10 | 9–8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Flag | a | b | x | y | e | l | f | g | n | v | z | c |

| Flag | Name | Meaning |
|---|---|---|
| a, b, x, y | Widths of A, B, X and Y | 0 = 8, 1 = 16, 2 = 32, 3 = 64 bits |
| e | Edge interrupt disable | 1 disables the four edge triggered interrupts |
| l | Level interrupt disable | 1 disables the level triggered interrupt |
| f | User flag F | Free for programs to use |
| g | User flag G | Free for programs to use |
| n | Negative | Set when the result's top bit (at the operation's width) is set |
| v | Overflow | Signed overflow; done, for the repeating instructions |
| z | Zero | Set when the result is zero |
| c | Carry | Carry out of an add, not borrow from a subtract or compare |

In assembly, flags are written in lower case (`c`, `z`, `n`, `v`, `f`, `g`) and registers in
upper case, so `v` is the overflow flag and `V` is the register.

## Internal registers

DI, D and DJ are 64 bit internal data registers.  Programs can't reach them.  They appear in
the instruction descriptions: D holds the value a block instruction loads from memory, DI and
DJ the two values of a vector instruction.

## After reset

| State | Value |
|---|---|
| A, B, X, Y, V, W | 0 |
| S | `0001:FFFF` |
| Widths of A, B, X and Y | 8 bits |
| e and l | Set (interrupts disabled) |
| f, g, n, v, z, c | Clear |
| Interrupt depth | 1 (kernel mode) |

The processor then reads the reset vector at `FFFF:FFFC`.
