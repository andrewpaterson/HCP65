# HCP65 Design

How the HCP65's hardware, kernal, emulator and board tester are built and why, for whoever changes them: the
decisions behind each part, with their reasons and dates, and what isn't done.  The [user pages](../index.md) say
what the HCP65 does; these pages link to them rather than repeat them.

| Page | Covers |
|---|---|
| [The Hardware](hardware.md) | Where the memory map, registers and decode came from (the KiCad schematics), kernel mode, the boards' faults files, and how HCP65Lib models the boards: memory, time, contention, the SD Card and Sound Boards. |
| [The Kernal](kernal.md) | The HCP65's operating system: its two images, start up, processes, kernel calls, drivers, the file system, memory, the screen and sprites, start-up time. |
| [Interrupts](interrupts.md) | How the boards' interrupts reach their drivers: entry and exit, the dispatch, waiting, and keeping off main memory during a DMA. |
| [The Emulator](emulator.md) | The HCP65Emulator program: its window and threads, sound, the reference build, the debugger's end, and TestHCP65Lib. |
| [The Board Tester](tester.md) | HCP65Tester, which tests the real boards through a Pico based tester over a serial port. |

Elsewhere:

- [Libc65 Design](../../compiler/design/libc65.md): the C library and the W65C816 runtime the kernal and programs
  link, the maths tables the kernal loads, and BaseLib's FAT code in W65C816 assembly.
- [The Debugger](../../ide/design/debugger.md): the IDE's debugger and its protocol, whose emulator end is on [The
  Emulator](emulator.md#the-debuggers-end).
- The 65C64, the 64 bit processor the HCP65 project proposes: its [architecture](../../65c64/index.md) and its
  [emulator](../../65c64/emulator.md), MicroprocessorLib's simulators.  It has no computer of its own yet.
