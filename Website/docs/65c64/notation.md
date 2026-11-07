# Notation

The instruction pages describe each operation in this notation, taken from the architecture
spreadsheet.

## Values

| Symbol | Meaning |
|---|---|
| A, B, S, V, W, X, Y | The contents of that register |
| a, b, x, y | The width of A, B, X or Y in bits |
| C | The program counter |
| P | The status register |
| R | The contents of a register |
| r | The width of a register |
| H | The contents of a hardware register |
| h | The width of a hardware register |
| DI, D, DJ | The internal data registers |
| O | An operand: a register, an address, or an embedded or immediate value (see [Operands and Addressing](operands.md)) |
| O1 | The first (left) operand, the destination |
| O2 | The second (right) operand, the source |
| O:s | The operand, sign extended |
| o | The length of an operand in bytes |
| N | An operand count or index |
| i | The length of the instruction in bytes |
| p | A status flag |

## Memory and parts of registers

| Symbol | Meaning |
|---|---|
| [Q] | The contents of memory at address Q |
| R:[q] | Byte q of register R |
| R:b[q] | Bit q of register R |
| R:n[q] | Nybble q of register R |

Byte, bit and nybble 0 are the least significant.

## Operators

| Symbol | Meaning |
|---|---|
| Q1 → Q2 | Q1 is assigned to Q2 |
| + | Addition |
| - | Subtraction |
| ∧ | Bitwise AND |
| ∨ | Bitwise OR |
| ⊻ | Bitwise XOR |
| ! | Bitwise NOT |
| ‹‹ | Shift left |
| ›› | Shift right |
| ››› | Signed shift right |
| { … } | Optional: done only in some forms of the instruction |
| ( … ) | Grouping |

## Assembly syntax

In the assembly on these pages:

- Mnemonics and directives are in lower case (they aren't case sensitive).
- Registers are in upper case and status flags in lower case: `V` is the register, `v` the
  overflow flag.
- Square brackets are memory, so `[$1000]` is the contents of address `$1000`.  Brackets
  inside brackets are indirection.
- Round brackets group two or more operands into one: `psh (A, B), S`.  A group of one is an
  error.
- Numbers are decimal (`123`), hexadecimal (`$7B` or `0x7B`), binary (`0b1111011`) or
  characters (`'a'`).  Underscores may separate digits: `$1234_5678`.
- `;` starts a comment.
