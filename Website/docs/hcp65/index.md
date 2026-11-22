# HCP65 Modern Home Computer

The HCP65 computer, built around the W65C816.

The processor board holds the W65C816, 512KB of RAM, 512KB of flash ROM and a set of on-board
registers.  Everything else (memory boards, video, sound, storage, input) plugs into device sockets.

- [Memory Map](memory-map.md) - kernel and user mode, what is where, and how user processes get
  their own banks.
- [Memory Management](memory.md) - what the kernel keeps, what each program is given, and how a
  program's heap grows and shrinks.
- [Kernel Calls](kernel-calls.md) - how a program asks the kernel for things, and every call:
  programs, files, memory, devices and the screen.
- [On-board Registers](registers.md) - the 16 registers on the processor board: interrupts, the
  NMI timer, device resets, the process ID and the instruction counter.
- [Devices and Main Memory](devices.md) - the device sockets, the layout inside each device's
  space, and how main memory is assigned to the sockets.
- [Starting Up](boot.md) - the boot loader in the ROM, the kernel it loads from the SD card
  (`\OS\OS.SYS`) and the start up program `\OS\STARTUP.TXT` names.
- [The Console and the Shell](console.md) - the standard handles, text on the Video Board, the
  keyboard, and the shell's commands.
- [The Screen](screen.md) - a program's picture: modes, frames, palettes, blits, images, scaling,
  and the Blitter and Graphics Boards.
- [Sprites](sprites.md) - drawing moving pictures: from PNGs to palettised SFTs, SFT and image
  blits, drawing each frame, spans and scaling.

The compiler's runtime routines are in [Drivers and Runtime](runtime/index.md).

These pages were worked out from the KiCad schematics in `658-Computer/Kicad/Computer`.  The
schematic sheet each part comes from is named in its section.
