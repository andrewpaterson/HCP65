# 65C64 Microprocessor Proposal

The 65C64 is a thought experiment: a 64 bit processor that feels like part of the 65 lineage.
It's a W65C816 with a 64 bit ALU and (mostly) 64 bit registers, a 16 bit data bus and a 32 bit
address bus.  It aims to be minimalist and fun to program.

- **Eight registers**, counting the program counter.  A and B are the accumulators, X and Y the
  index registers, V and W are full width offset registers in place of the direct page, and S is
  the stack pointer.  With a few restrictions, any of them can be used as an accumulator, an
  offset, an index or a stack pointer.
- **Register widths.**  A, B, X and Y can each be set to 8, 16, 32 or 64 bits, like the
  W65C816's m and x flags but per register.
- **Two operand instructions.**  Most instructions take a destination and a source, and either
  of them can be memory.  One `cpy` instruction does the work of `lda`, `sta`, `stz`, `ldx`,
  `stx`, `tax` and the rest.
- **Small values in the opcode.**  Small constants (-8 to 8 for most instructions, -16 to 16
  for a copy or a logical operation on a register) are embedded in the 16 bit opcode, so
  `add W, 8` is one word, like `inc`.
- **Block and vector instructions** that repeat on their own until a count runs out or a stop
  condition holds, in place of `mvn` and `mvp`.

The [assembler](assembler.md) (Assembler65Lib) and the [simulators](emulator.md)
(MicroprocessorLib) implement the proposal, and the C++ compiler has a 65C64 back end.

## Pages

- [Registers](registers.md) - the registers, their widths, the status register P and the state
  after reset.
- [Notation](notation.md) - the symbols used in the instruction operations.
- [Operands and Addressing](operands.md) - registers, immediates, embedded values and the 13
  memory addressing modes.
- [Instructions](instructions/index.md) - every instruction, with links to the full
  descriptions.
- [Opcode Encoding](opcode-encoding.md) - the bits of every opcode and operand word.
- [Sizes and Timing](timing.md) - the words and clock cycles each instruction takes.
- [Interrupts and Modes](interrupts.md) - reset, the interrupts, the vectors and kernel mode.
- [Bus and Signals](bus.md) - bus cycles, the pins and the proposal's block diagram.
- [Memory Map](memory-map.md) - the vectors, the stack and the proposal's user bank
  remapping.
- [Assembler](assembler.md) - the source format: labels, expressions and directives.
- [Objects and Linking](linking.md) - sections, symbols and the linker.
- [Calling Convention](calling-convention.md) - how the C++ compiler's code calls functions.
- [Worked Examples](examples.md) - multiply and divide step by step, and eleven whole
  programs.
- [Emulator](emulator.md) - the two simulators and how to run a program on them.

## Sources

These pages are written from:

In the HCP65 repository:

- `W65C64/65C64 Instruction Architecture.xlsx` (V0.1 Prelim), the architecture spreadsheet.
- `W65C64/65C64 Instruction Architecture - Copy.pdf`, the draft instruction reference for
  `cpy`.
- `W65C64/65C64 Proposal.odt`, the original proposal.
- `Libraries/Assembler65Lib/Assembler65C64Syntax.txt`, the assembler's syntax.

In the CommonLibraries repository:

- `Libraries/MicroprocessorLib/65C64Simulator.txt`, the simulator's decisions on the parts the
  spreadsheet leaves open.
- `Libraries/CppCompilerLib/CPP65Generator.txt` and `IR.txt`, the C++ compiler's calling
  convention.

Where they disagree, the simulator is taken as the truth.  Every example on these pages
assembles with the assembler: a test (TestAssembler65Lib) assembles them all.
