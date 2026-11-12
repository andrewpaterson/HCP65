# Sizes and Timing

How long each instruction is and how many clock cycles it takes.  The numbers on this page were
measured by running the instructions on the simulator.

## The rules

Opcodes and operand words are 16 bits.  An instruction is:

- 1 opcode word,
- plus 1 to 4 words for an immediate (a value takes its bytes ÷ 2, rounded up),
- plus 1 word for an offset or 2 for an absolute address,
- plus 1 word for the instructions that keep an index or a count in an operand word
  (`scp`, `scb`, and some forms of `bist` and `stab`).

An instruction takes:

- 1 cycle to read the opcode,
- 1 cycle for each operand word,
- 2 cycles to read a pointer (3 if it's at an odd address), for each
  [indirect](operands.md#memory-addresses) addressing mode,
- 1 cycle for each word of data read, and
- 1 cycle for each word of data written.

Address calculation takes no cycles, and neither does the operation itself: an instruction on
registers alone is one cycle.

Data takes a cycle for each 16 bit word it touches, so the cycles depend on the width of the
operation and on whether the address is even:

| Width | Even address | Odd address |
|---|---|---|
| 8 bits | 1 | 1 |
| 16 bits | 1 | 2 |
| 32 bits | 2 | 3 |
| 64 bits | 4 | 5 |

In the tables below, **d** is that number: the cycles to read or write the data once.  With
aligned data, d is 1 at 8 or 16 bits, 2 at 32 bits and 4 at 64 bits.

## Copy, logic and arithmetic

For `cpy`, `and`, `or`, `xor`, `tst`, `add`, `sub`, `cmp`, `shl` and `shr`.

| Operands | Example | Words | Cycles |
|---|---|---|---|
| Register, register | `add A, B` | 1 | 1 |
| Register, embedded value | `add A, 1` | 1 | 1 |
| Register, 8 or 16 bit immediate | `add A, 100` | 2 | 2 |
| Register, 32 bit immediate | `cpy A, $12345678` | 3 | 3 |
| Register, 64 bit immediate | `cpy A, $1122334455667788` | 5 | 5 |
| Register, memory | | | |
| &nbsp;&nbsp;`[R]`, `[R1 + R2]` | `cpy A, [V]` | 1 | 1 + d |
| &nbsp;&nbsp;`[offs]`, `[offs + R]` | `cpy A, [$40]` | 2 | 2 + d |
| &nbsp;&nbsp;`[addr]`, `[addr + R]` | `cpy A, [$12345678]` | 3 | 3 + d |
| &nbsp;&nbsp;`[[R]]`, `[[R1 + R2]]`, `[[R1] + R2]` | `cpy A, [[V]]` | 1 | 3 + d |
| &nbsp;&nbsp;`[[offs]]`, `[[offs + R]]`, `[[offs] + R]` | `cpy A, [[$40]]` | 2 | 4 + d |
| &nbsp;&nbsp;`[[addr]]`, `[[addr + R]]`, `[[addr] + R]` | `cpy A, [[$12345678]]` | 3 | 5 + d |

With memory as the first operand the words are the same.  What changes is how often the data
crosses the bus:

| Instruction | Data cycles | Example at 32 bits | Cycles |
|---|---|---|---|
| `cpy [address], R` | d: it only writes | `cpy [$40], A` | 4 |
| `cpy [address], value` | 2: always 32 bits | `cpy [$40], 1` | 4 |
| `tst [address], R`, `cmp [address], R` | d: they only read | `cmp [$40], A` | 4 |
| `and`, `or`, `xor`, `add`, `sub`, `shl`, `shr` | 2d: they read, then write | `add [$40], A` | 6 |

A 64 bit `add [$40], A` is 10 cycles: the opcode, the offset, four to read and four to write.

## Other register instructions

| Instruction | Example | Words | Cycles |
|---|---|---|---|
| `cpyx` | `cpyx A, [$40]:8` | As `cpy` with memory | As `cpy`, with d for the width of the value in memory |
| `neg`, `abs` | `neg A` | 1 | 1 |
| `scp`, `scb` | `scp (A, 0), (B, 1), 1` | 2 | 2 |
| The finds | `fsbi A, X` | 1 | 1 |
| `mul` | `mul` | 1 | 1 a step: 1 for each byte of B |
| `div` | `div` | 1 | 1 a step: 1 for each nybble of Y |
| `sep`, `cep`, `stat` | `sep c` | 1 | 1 |
| `wth`, `wtth`, `gwth`, `wthx` | `wth A, 16` | 1 | 1 |
| `bist`, index 0 to 5 | `bist c, (A, 3)` | 1 | 1 |
| `bist`, another index or a register | `bist c, (A, 40)` | 2 | 2 |
| `stab`, index 0 to 5, no count | `stab (A, 0), c` | 1 | 1 |
| `stab`, otherwise | `stab (A, 2, 10), 1` | 2 | 2, or 3 for 32 or more bits |

A 64 bit multiply is 8 cycles and a 64 bit divide 16.

## Flow

The cycles for the stack are for S at its usual alignment: an odd address, so that the value
pushed ends on a word boundary.

| Instruction | Example | Words | Cycles |
|---|---|---|---|
| Branch, embedded offset | `bras z, Loop` | 1 | 1 |
| Branch, immediate offset | `bras z, Far` | 2 to 3 | 2 to 3 |
| `bra`, within -64 to 63 bytes | `bra Loop` | 1 | 1 |
| `bra`, further | `bra Far` | 2 to 3 | 2 to 3 |
| `brsr` | `brsr Sub` | 2 to 3 | 4 to 5 |
| `jmp` to a register | `jmp X` | 1 | 1 |
| `jmp` to an address | `jmp Target` | 2 or 3 | 2 or 3 |
| `jmp` through a pointer | `jmp [$40]` | 2 or 3 | 4 or 5 |
| `jsr` to a register | `jsr X` | 1 | 3 |
| `jsr` to an address | `jsr Target` | 2 or 3 | 4 or 5 |
| `jsr` through a pointer | `jsr [$40]` | 2 or 3 | 6 or 7 |
| `ret` | `ret` | 1 | 3 |
| `ret P`, `rti` | `rti` | 1 | 4 |
| `brk` | `brk 5` | 1 | 6 |
| An interrupt | | | 5 |
| `nop` | `nop 3` | 1 | 1 + the wait: `nop` is 2 |
| `stp` | `stp` | 1 | 1, then the wait |

A branch takes the same cycles whether or not it's taken.  Pushing P with a jump adds a cycle.

## Stack

| Instruction | Example | Words | Cycles |
|---|---|---|---|
| `psh`, `pul` registers | `psh (A, B)` | 1 | 1 + d for each register |
| `psh P`, `pul P` | `psh P` | 1 | 2 |
| `psh C` | `psh C` | 1 | 3 |
| `psh (C, P)` | `psh (C, P)` | 1 | 4 |
| `psh` an immediate | `psh $1234` | 2 to 5 | The words + d for the immediate's width |
| `psh [address]` | `psh [$40]` | 2 or more | The address's cycles + 2 to read + 2 to write |
| `pul [address]` | `pul [$40]` | 2 or more | The address's cycles + 2 to read + 2 to write |

Pushing four 32 bit registers with one `psh` is 9 cycles.

## Block and vector

Each step fetches the opcode again, so that the instruction can be interrupted between steps.

| Instruction | Cycles a step | At 8 bits | At 64 bits |
|---|---|---|---|
| `bcpy` | 1 + d: it only writes | 2 | 5 |
| `btst`, `bcmp` | 1 + d: they only read | 2 | 5 |
| The other block instructions | 1 + 2d | 3 | 9 |
| `vcpy`, `vnot` | 1 + 2d: read [W], write [V] | 3 | 9 |
| `vtst`, `vcmp` | 1 + 2d: read [W] and [V] | 3 | 9 |
| The other vector instructions | 1 + 3d | 4 | 13 |

A `vcpy` of 64 bit elements moves 8 bytes in 9 cycles, which is 1.13 cycles a byte.  A `bcpy`
fill at 64 bits writes 8 bytes in 5 cycles.
