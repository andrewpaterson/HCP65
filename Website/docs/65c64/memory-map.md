# Memory Map

The 65C64 addresses 4GB with its 32 bit address bus.  Addresses are written as two 16 bit halves,
`0001:FFFF`.  The processor itself fixes only two things in that space: the vectors at the top
and the stack pointer's starting value.

| Address | Purpose |
|---|---|
| `FFFF:FFFC`–`FFFF:FFFF` | The reset vector |
| `FFFF:FFDC`–`FFFF:FFFB` | The [interrupt vectors](interrupts.md#vectors) |
| `FFFF:F000`–`FFFF:FFDB` | The software interrupt vectors, `brk 0` to `brk 1014` |
| `0100:0000`–`FFFF:EFFF` | External memory |
| `0040:FFFF` and below | The user bank map (proposal only) |
| `0001:FFFF` | The stack pointer after reset.  The stack grows down from here. |
| `0000:0000`–`0000:FFFF` | The first 64KB, reached by the one word [offset addressing modes](operands.md#address-offset) |
| `0000:0000`–`00FF:FFFF` | The on-chip SRAM (proposal only) |

## The first 64KB

An address below `0001:0000` fits in one operand word in place of two, through the
[offset](operands.md#address-offset) addressing modes.  It is the 65C64's zero page: a
good place for the variables a program uses most.  The C++ compiler keeps its
[pseudo registers](calling-convention.md#pseudo-registers) in the first 256 bytes.

## The stack

S is `0001:FFFF` after reset, so by default the stack is the second 64KB, growing down
towards `0001:0000`.  A program can move S anywhere, and any other register can be used as a
stack pointer too (see [`psh` and `pul`](instructions/stack.md)).

## The vectors

The top 4KB, `FFFF:F000` to `FFFF:FFFF`, holds 1024 vectors of four bytes each.  A system
needs memory there: usually ROM, since the reset vector must be there at power on.  See
[Interrupts and Modes](interrupts.md#vectors).

## On-chip SRAM

!!! note
    The rest of this page describes parts of the proposal that the simulator doesn't have.  In
    the simulator all memory is external and addresses are never remapped.

In the proposal the bottom 16MB, `0000:0000` to `00FF:FFFF`, is SRAM on the chip, and external
memory starts at `0100:0000`.  The on-chip SRAM is reached through a 64 bit internal bus, where
external memory goes through the 16 bit [data bus](bus.md).  A hardware register enables it.

## User bank remapping

The proposal has a very basic memory management unit for
[user mode](interrupts.md#kernel-mode-and-user-mode).  It gives each user process its own view
of memory by replacing the top 16 bits of every address, the *bank*, with a value from a table
in the on-chip SRAM.

A bank is 64KB.  Each process has a row of the table with one 16 bit entry for each bank it can
reach.  When user mode code uses an address, the processor looks up:

```text
	entry address = map offset + (process ID × bank width + bank) × 2
```

and puts the 16 bit entry on the top half of the address bus in place of the bank.  The bottom
16 bits of the address aren't changed.

| Hardware register | Meaning |
|---|---|
| User mode bank remapping enable | Turns remapping on |
| User mode bank width | The number of banks a process can reach: 256 is 16MB, 512 is 32MB … |
| Memory bank remapping offset | Where the table is |
| User process ID | The table row in use |

For example, with a bank width of 1024, process 1 reading the address `0002:6FE7` looks up
entry (1 × 1024 + 2) × 2 = 2052 bytes into the table.  If that entry holds 5, the address on the
bus is `0005:6FE7`.

The spreadsheet's memory map puts the default table at `0020:0000`, where the entry for
process 0's bank 0 is, and runs it up to `0040:FFFF`.

Kernel mode addresses are never remapped, so the kernel sees all of memory and sets up the
table through the [hardware registers](instructions/hardware.md).
