# Instructions

Every 65C64 instruction, grouped by what it does.  The operations use the
[notation](../notation.md) of the architecture spreadsheet, and the operands are described in
[Operands and Addressing](../operands.md).

Many instructions have a zero extending and a sign extending form.  They share a mnemonic: the
sign extending form is written with `:s` on the second operand (`cpy A, B:s`).

Flags not listed are unchanged.  The W65C816 column names the W65C816 instructions each one
replaces.

## Copy, logic and arithmetic

These take register, value and memory [operands](../operands.md).

--8<-- "index-copy.txt"

`add`, `sub` and `cmp` don't use the carry as an input.  `cpyx` copies a value of a given
width (8, 16, 32 or 64 bits) between memory and A, B, X or Y.

## Register operations

--8<-- "index-register.txt"

`mul` does a byte of B a step and `div` a nybble of Y.  They do one step each time they run
and leave C on their opcode until the result is done (v set), so they repeat on their own and
can be interrupted between steps.

## Flow

--8<-- "index-flow.txt"

Branch offsets are in bytes, relative to the next instruction.  `jmp`, `jsr`, `bra` and `brsr`
can push P after C on any stack.

## Stack

--8<-- "index-stack.txt"

Registers are pushed in the order A, B, S, V, W, X, Y, each at its width, and pulled in the
reverse order.  The stack can be any of A, B, S, V, W, X and Y.

## Processor state

--8<-- "index-state.txt"

## Block and vector

These repeat on their own, one element a step, until the count in A goes negative (v set) or,
if stopping is allowed, a stop condition on z or n holds.  They can be interrupted between
steps.

--8<-- "index-block.txt"

The *pre* forms set n and z from the element as it was before it's changed, so a stop
condition sees the original value.  The `vcpy` instructions replace the W65C816's `mvn` and
`mvp`.

## Hardware

--8<-- "index-hardware.txt"

The hardware register instructions are in the spreadsheet but aren't assembled or simulated
yet.

## Encoding

Every instruction is one 16 bit opcode, followed by up to four 16 bit operand words.  The low
four bits of the opcode are the *instruction code*.  Codes 0 to 7 are one instruction each, with
the rest of the opcode for their operands.  The others share a code between several
instructions, told apart by an *extend* or *sub instruction* field.  The
[Opcode Encoding](../opcode-encoding.md) page has every bit.

| Code | Instructions |
|---|---|
| 0 | `cpy` |
| 1 | `and` |
| 2 | `or` |
| 3 | `xor` |
| 4 | `tst` |
| 5 | `add` |
| 6 | `sub` |
| 7 | `cmp` |
| 8 | The block and vector instructions |
| 9 | `scp`, `scb` |
| 10 | `jmp`, `jsr`, `bra`, `brsr`, and `psh` of P and C |
| 11 | `ret`, `rti`, `pul P`, `sep`, `cep`, `wth`, `wtth`, `gwth`, `wthx`, `brk`, `stp` |
| 12 | `bras`, `brac`, `brasa`, `braso`, `brae`, `brane`, the one word `bra`, `nop`, `cpyx` |
| 13 | `psh`, `pul` |
| 14 | `mul`, `div`, `neg`, `#!text abs`, the finds, `shl`, `shr` |
| 15 | `hws`, `hwg`, `hwsp`, `hwcp`, `bist`, `stab`, `stat`, the sign extending `cpyx` |

An opcode that isn't an instruction is *illegal*.  The simulator runs it as a one cycle
instruction that does nothing.

## W65C816 instructions without a match

Some W65C816 instructions have no single 65C64 instruction, but can be built from a few:

| W65C816 | 65C64 | How |
|---|---|---|
| `rol` | `stat f, c`<br>`shl A, 1`<br>`stab (A, 0), f` | Copy c into f, shift A left, copy f into bit 0. |
| `ror` | `stat f, c`<br>`gwth X, (A, -1)`<br>`shr A, 1`<br>`stab (A, X), f` | Copy c into f, put A's width less one in X, shift A right, copy f into bit X. |
| `mvn` | `vcpy W, V, +1, A, 8` | Copy bytes from [W] to [V], moving up. |
| `mvp` | `vcpy W, V, -1, A, 8` | Copy bytes from [W] to [V], moving down. |
| `trb` | `xor A, -1`<br>`and [M], A` | Invert A, then and it into memory. |

`cld` and `sed` (decimal mode) have to be written in software.  `phb`, `phk`, `plb` and `xce`
have no meaning outside the W65C816.
