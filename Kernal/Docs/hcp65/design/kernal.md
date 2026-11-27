# The Kernal

The kernal is the HCP65's operating system: it brings up the boards, reads the SD card, loads and runs programs,
answers their kernel calls and owns the screen.  The user pages say what it does: [Starting Up](../boot.md),
[Kernel Calls](../kernel-calls.md), [Memory Management](../memory.md), [The Screen](../screen.md),
[Sprites](../sprites.md) and [The Console and the Shell](../console.md); the hardware it drives is in [Memory
Map](../memory-map.md), [On-board Registers](../registers.md) and [Devices and Main Memory](../devices.md).  This
page says how it is built and why.  Its interrupts have their own page, [Interrupts](interrupts.md).  The FAT code
it reads the card with, the compiler's runtime and the heap are Libc65's and BaseLib's ([Libc65
Design](../../compiler/design/libc65.md)).

The flat images, scaled blits and canaries were planned in ImagesPlan.txt, finished and folded into this page (the
plan is in HCP65's history up to 27b9f9d).

## Two images: the boot loader and OS.SYS

The kernal was started on 2026-10-31 as the processor board's ROM.  Since 2026-11-16 the ROM holds only a boot
loader ([`Boot/Boot.cpp`], [`Boot/BootStart.asm`]) and the kernal is `\OS\OS.SYS` on the SD card, so the kernal
changes without the flash being reprogrammed.  The boot loader is linked to run from MPU RAM banks 4 and 5, out of
the way of OS.SYS, which it reads into `$00FF00` to `$03FFFF`.

The boot loader brings the machine up exactly as the kernal does because it is built from the kernal's own units:
`HCP65Boot.vcxproj` lists [`Kernal/KernalStartUp.cpp`], [`Kernal/KernalMemory.cpp`], [`Kernal/KernalDevices.cpp`],
[`Kernal/KernalDriver.cpp`], [`Kernal/KernalSDCard.cpp`] and its assembly, [`Kernal/KernalInterrupts.cpp`] and its
assembly, and [`Kernal/StartRuntime.inc`], the start up they share.  The kernal then starts from the beginning
again, so it never depends on what the boot loader left; each clears the status block ([`SKernalStatus`] at
`$00C000`) as it starts, so a test or debugger reads one place whichever is running.

Both images are built by `hcp65build` from their solutions, as their projects' `HCP65Link` groups say, and by
TestHCP65Lib's [`CHCP65KernalBuilder`] for the tests, which give each test card its own OS.SYS and `STARTUP.TXT`.  OS.SYS is the bytes from `$00FF00` to
the end of bank 3, 196,864 bytes at most; the kernal's code and data have banks 1 to 3, 192KB.  On 2026-11-23 a
Release kernal was 186,540 bytes and a Debug one 189,491, so room is still the constraint that shapes it: its bss
follows, then the drivers it loads, then its heap in bank 6.

### Assembly start up, then C++

[`Kernal/Start.asm`] clears the remap table's pages for processes `$FC` to `$FF` and goes on in
[`Kernal/StartRuntime.inc`]: the stack at `$BFFF`, D at 0, bss cleared (a word at a time when it is under 64KB),
[`KernalInitHeap`], each global's initialiser in link order, then `main` ([`Kernal/Kernal.cpp`]); when `main`
returns the kernal idles in `wai`.  Everything after the start up is C++ compiled by the project's W65C816
compiler.  Bank 0 holds only the stack, the direct page, the status and the page of `jml` stubs and vectors at
`$FF00`: the code is in bank 1 onwards and runs at the address it is linked at.

The kernal has no floats: `NoFloats="true"` makes a float in its code, or in a library function it links, a build
error.  It is also built without the maths tables (`MathTables="false"`), so its multiplies and divides never read
main memory, which matters while the SD Card Board's DMA has it
([Interrupts](interrupts.md#main-memory-contention)).  The start up program loads the tables into main memory
instead ([Libc65 Design](../../compiler/design/libc65.md#the-maths-tables-and-the-runtime)).

What else keeps it small:

- Only BaseLib's system allocator is set up ([`CMallocators::InitSystem`]): BaseLib's global memory would add about
  24KB.
- Executables and drivers are chunk files read without hashes ([`CChunkFile::Init`]; [`CChunkFile::InitHashed`] is
  the writer's), so the kernal links no MD5.
- BaseLib's FAT code is W65C816 assembly ([Libc65
  Design](../../compiler/design/libc65.md#fat16-and-fat32-in-w65c816-assembly)): the kernal had grown to 194.8KB of
  its 192KB on 2026-10-31, and `FatVolume.cpp` alone compiled to 63.9KB.

### Telling the emulator

[`Kernal/KernalDebug.cpp`] tells a debugger when a process starts and ends, when a main memory bank is given to a
process and where a driver was loaded ([`KernalDebugProcessStarted`], [`KernalDebugBank`] ...).  It fills an
[`SHCP65DebugMessage`] and executes `WDM` with its address in X:A.  A real W65C816 runs `WDM` as a two byte no-op,
so the kernal tells on every machine without checking that anyone listens; the emulator's debug port hears it.
This replaced a Debug Port board on 2026-11-08, freeing a socket.

## Starting up

[`KernalStartUp`] is the part both images run: it clears the status, sets up the system allocator, scans the
sockets' device ROMs ([`KernalScanDevices`], [`SKernalDeviceROM`]), gives the memory boards their main memory
windows ([`KernalInitMainMemory`]), starts the NMI timer ([`KernalInitInterrupts`]), starts the built in drivers
and mounts the first SD card with a FAT volume.  Each step that works says so in the status, so a start up that
stops says where ([The status](../boot.md#the-status)).  The only built in driver is the SD Card Board's,
[`CKernalSDCardBoard`], for up to two boards: the boot loader needs it before any file can be read.

The kernal's `main` then loads the other boards' drivers from the card, starts the console and the screen, reads
`\OS\STARTUP.TXT` and runs the program it names as process `$FF`.  When that program exits it records its heaps'
peaks in the status for the tests.  The boot loader instead puts every board back in reset and stops the interrupts
([`KernalStopDrivers`], [`KernalStopInterrupts`]) before it copies OS.SYS's vectors over its own and jumps.

## Processes

Processes are nested, not run side by side: a program runs another with [`KernalExecute`] and waits in that call
until it ends.  That made a scheduler unnecessary and lets one kernal stack serve them all.  Each process has a 256
byte page of the remap table at the top of MPU RAM bank 7 (`$07FC00` for `$FC` to `$07FFFF` for `$FF`), so there
are four process IDs: the start up program is `$FF` and each process it runs gets the next ID down.

[`KernalRunExecutable`] saves the caller's [`SKernalCall`] (the registers its call saved), loads the executable,
writes the process ID register and calls [`KernalEnterUser`], which saves the kernal's stack pointer in
[`gsKernalCall`] and `RTI`s into the process.  The exit call, or a changed canary, makes the next `COP` return from
[`KernalEnterUser`] instead of to the process; the kernal then closes the screen, the handles and the boards the
process had ([`CKernalScreen::ProcessEnded`], [`KernalCloseHandles`], [`KernalCloseDrivers`]), frees its banks
([`KernalFreeProcess`]) and puts the caller's [`SKernalCall`] back.  A process starts with `P = $30`: 8 bit
registers and interrupts on.

### Loading an executable

An executable is an Assembler65Lib object ([`CAsm65Object`]) whose sections are all placed, with a `UserStart`
symbol and, optionally, `UserHeapSize` (what to map at `$200000` before it starts).  The user chose this on
2026-10-31: the linker's output needed no new format.  [`KernalLoadExecutable`] reads it as a stream: the chunk
file reads through a [`CFatAbstractFile`] over the open file, and [`CAsm65Object::SetSkipSectionBytes`] leaves the
sections' bytes in the file.  Once the banks are mapped, [`KernalReadToUser`] reads each section straight from the
card into the process's banks, a bank's worth at a time.  An executable can be much bigger than the kernal's 64KB
heap (modplay.exe was 756,363 bytes on 2026-11-22); the first loader read the whole file into the heap, and a 726KB
modplay.exe ran the kernal out of it (2026-10-31).

Every section is checked before anything is mapped ([`KernalIsProcessSection`]): code and data go in banks 1 to
`$1F`, so a program linked without `-limit` can't be loaded over its stack or heap (`KERNAL_RESULT_BAD_ADDRESS`).

[`KernalMapBank`] gives the lowest free physical bank and doesn't clear it: a program's start up clears its own
bss.  Clearing every 64KB bank cost about 20M cycles of modplay's start up (88M to 68M to sound on, 2026-11-06,
emulator at 25 MHz).  The emulator's fill mode ([`CHCP65::SetMemoryFill`]) fills each bank the kernal reports with
`$CC` instead, so a program that reads memory it never wrote sees the same wrong byte each time.

The loader copies the kernal's vectors (`$FFE0` to `$FFFF`) into each process's bank 0.  The vector pull is what
puts the board in kernel mode, and only from its second cycle: the first vector byte is read in user mode, from the
process's bank 0 ([Interrupts from user mode](../memory-map.md#interrupts-from-user-mode)).  A program that writes
over its copy breaks only itself.

### Canaries

A wild pointer into a bank a program hasn't reaches physical bank 0, which is no one's, so it does no harm, but two
common mistakes stay inside the program's own bank 0, which is always mapped.  The loader writes eight bytes of a
pattern at `$0000` (where a NULL pointer's reads and writes land; the start up puts the direct page at `$0100`,
above it) and at `$0200` (the bottom of the stack's room).  [`KernalCheckCanaries`] compares them at every kernel
call and ends the process with `KERNAL_RESULT_NULL_WRITE` or `KERNAL_RESULT_STACK_OVERFLOW` if one has changed.  It
says only that it happened since the last call.  The check took the cheapest call from 2,888 to 3,086 cycles
(2026-11-22, `TEST_HCP65_TIME_COP`).  A hardware way to catch bad accesses is sketched in the processor board's
`Bad Access Detection.txt`; it isn't built.

## Kernel calls

A call is `COP n`, the call's number in the instruction's signature byte, with A the address of its arguments on
the process's stack ([How a call is made](../kernel-calls.md#how-a-call-is-made)).  `BRK` is kept for breakpoints
(the user, 2026-10-31); the kernal's `BRK` and `ABORT` handlers return at once.

The `COP` frame is pushed on the process's stack, but by the time the handler runs the board is in kernel mode and
bank 0 is MPU RAM, so S points at the right offset in the wrong memory.  `KernalCop` in [`Kernal/Start.asm`]
therefore saves the registers with long stores into [`gsKernalCall`], not on the stack, then moves S to where
[`KernalEnterUser`] left the kernal's stack for this process; a process run during another's call has its calls
below the other's.  [`KernalCopCall`] reads the signature byte through the remap table, checks the canaries and
switches on the call.

Calls read and write the process's memory through its remap page, a bank at a time ([`KernalUserPointer`],
[`KernalCopyToUser`]); an address in a bank it hasn't is refused, so a call can't reach another process's memory or
the kernal's.  [`KernalSetKernalCopy`] lets the console hand drivers its own buffers through the same calls.  The
device, system and screen calls are one `COP` each with a block in the process's memory ([`KernalCallDevice`],
[`KernalCallSystem`], [`CKernalScreen`]), so new calls don't use up signature bytes.  Calls 6 to 10 were the sound
board's until it had a driver, and are no longer used.

## Drivers

Each board in a socket has a [`CKernalDriver`]: [`CKernalDriver::Init`] takes it out of reset and
[`CKernalDriver::Kill`] holds it in reset again.  A driver is told which process opens each unit; a process has the
whole board unless the driver lets units be opened separately (the UART Board's two ports).  The kernal has 32
handles for all processes together.  The console opens the boards it uses as process 1, which no program is, so it
keeps them across programs.

Every driver but the SD Card Board's is a file on the card, `\OS\Drivers\` plus the name its device ROM gives
([Drivers](../devices.md#drivers)), so a new board needs no new kernal.  [`KernalLoadDriver`] loads a file once,
however many boards name it, and asks its `KernalCreateDriver(index)` for a driver for each.  A driver file is a
relocatable module: an Assembler65Lib object with one section, `module`, whose imports are kernal names.
[`KernalPlaceDriver`] puts it in MPU RAM after the kernal's bss and the drivers before it, within one bank and
below the heap ([`KernalDriverMemory`]; drivers are never unloaded), reads its bytes straight from the card,
relocates it and runs its globals' constructors.

The names a driver may import are the table in [`Kernal/KernalExports.asm`], searched by [`KernalFindExport`]; a
name that isn't there stops the driver loading (`KERNAL_LOAD_MISSING_EXPORT`).  A C++ function has two entries, one
taking its arguments on the stack and one with a suffix for those it takes in registers, and both are exported when
drivers call it directly.  [`CKernalDriver`]'s virtual functions are exported because a driver's vtables hold the
ones it doesn't override.  The table's length is worked out by the assembler, so adding an entry is enough.

## The file system

The kernal's one volume, [`gcKernalVolume`], is BaseLib's [`CFatVolume`] on the first card that mounts, out of the
four sockets of the first SD Card Board.  Each card is a [`CKernalSDCard`], a [`CFileDrive`] of 512 byte sectors.

- **Commands** are bit banged through the board's GPIO register.  Until the card has its address (CMD3) the clock
  must stay under 400kHz, so the C++ path (one register write per edge) is used; after it, the assembly in
  [`Kernal/KernalSDCard.asm`], which clocks at about 1MHz at 25MHz.  The CRC7 is a table lookup a byte
  ([`gauiKernalCRC7`]).
- **Reads** are the board's DMA into main memory: CMD17 for a sector, CMD18 and CMD12 for several.  When the
  destination is main memory (kernel addresses from `$200000`) the sectors go straight there; only a read into MPU
  RAM, which the board can't reach, goes through a 512 byte bounce buffer from the main memory heap
  ([`KernalMainMalloc`]).  Each block's end is the board's interrupt; the wait is in [Interrupts](interrupts.md).
- **Writes** are the CPU storing the 521 byte nybble stream to Write Data, the four lines' CRC16s worked out in
  C++, then the card's CRC status and busy clocked out through the GPIO register in C++
  ([`CKernalSDCard::WriteSector`]).  One sector at a time.

The emulated board ([`HCP65Lib/HCP65SDCard.h`]) works as the schematics are meant to; the faults found in them are
listed in the SD Card Board's `SD Card Board Faults.txt` beside its schematic.  Emulating the intended board was
Claude's choice on 2026-10-31; the user hasn't ruled on it.

## Memory

[Memory Management](../memory.md) says how memory is shared out.  The reasons:

- The kernal's own heap is MPU RAM bank 6 (since 2026-11-07; it was main memory before), so it is there before the
  memory boards are found, and a machine without main memory can still say so in its status.  Its peak was about
  16KB then.
- What a board must reach goes in the main memory heap ([`KernalMainMalloc`]), a [`CBankMallocator`] that takes
  banks from the top of main memory down while processes take theirs from the bottom up.  One free bank is never
  given to a process, so the heap can always grow by one.
- The maths tables are four banks mapped at `$400000` in every process wherever they physically are, so a program's
  runtime reads them at a fixed address ([`UserLib/UserMathTables.asm`]).

## The screen and sprites

The screen is the kernal's ([`CKernalScreen`]), not a board a program opens: the kernal holds the Video Board as
the console does, and a program that sets a mode has the picture until it sets text again or ends.  Draws are the
Video Board driver's; a blit's runs are copies in main memory ([`KernalMainCopy`]), which a Blitter Board makes
when the copy is long enough to be worth writing its registers and the processor makes otherwise.

**Images.**  The Graphics Board stretches a sprite by indexing a row with its offsets, which SFT's runs aren't, so
a program unpacks an SFT once into a flat image in its own heap ([`SFTImageMake`]) and gives the kernal a handle to
it.  The image's memory stays the program's and goes when it ends; the kernal keeps only the handle.  The images
are a table of 64 in MPU RAM ([`gasKernalScreenImages`], [`SKernalImage`]), because the drivers read it while a
board has main memory.  A handle is the entry's index (six bits) and a generation counted each time the entry is
used again, so a handle that has gone, or another process's, is refused.  A program's banks aren't next to each
other in main memory, so the image's address is looked up again at each blit ([`KernalUserPointer`]) and a row that
crosses into the next bank is two copies.  SFT blits stay for the console's font and pictures drawn once.

**Scaled blits** ([`KernalMainScaledCopy`]) are a Graphics Board copy a row, its offsets mapping each pixel drawn
to the nearest image pixel; the offsets are written only when the drawn width or the clip changes.  The row loop is
assembly ([`KernalMainScaledRows`], [`SKernalScaledRows`]): about 9,000 cycles a blit and 1,000 a row in C++, about
10,000 and 550 in assembly (2026-11-22).

In the Sprites demo Maki's frames stay SFT blits: without a Blitter Board her transparent frame was 455,000 cycles
as an image and 144,000 as an SFT (2026-11-22), and the user chose to keep the SFT.  With a Graphics Board she is
drawn scaled and mirrored from an image.

## Start-up time

On 2026-11-05 the HCP65 took 14.42 s at 25 MHz from reset to modplay's first sound (360.6M cycles, a sampling build
of TestHCP65Lib on the emulator):

| Part | Seconds |
|---|---|
| Loading modplay.exe (718,721 bytes) | 10.42: the FAT chain 5.90, the SD driver 2.64, copies 1.72 |
| The kernal's file load of the song | 1.38 |
| modplay parsing the song ([`LoadMOD`]) | 2.28 |

The DMA itself was about 1k cycles a sector against about 28k for the bit-banged command around it.  What was done
the same week is in [Libc65 Design](../../compiler/design/libc65.md#fat16-and-fat32-in-w65c816-assembly) (the CRC7
table, the bit banging in assembly, the forward seek, 4KB clusters, CMD18, DMA straight to the destination: 349M to
85M cycles to sound on).  On the kernal's side, files are read straight into the process's banks with no 4KB
staging buffer, the song's patterns are read a pattern at a time ([`MODReadPatterns`]), and the banks a process is
given aren't cleared (88M to 68M, 2026-11-06), about 2.7 s at 25 MHz.

## Decisions

- The start up in W65C816 assembly, the rest C++ through the project's compiler (the user, 2026-10-31).
- Kernel calls are `COP n` with the call in the signature byte; `BRK` is kept for breakpoints (the user,
  2026-10-31).
- Four processes, IDs `$FC` to `$FF`, with the remap table at the top of MPU RAM bank 7 (the user, 2026-10-31).
- Bank 0 holds the stack, the direct page and the vectors and stubs, no code (the user, 2026-10-31).
- Executables are Assembler65Lib objects with placed sections and an absolute `UserStart` (the user, 2026-10-31).
- Chunk files may be read and written without hashes, so the kernal needs no MD5 (the user, 2026-10-31: "Allow
  Chunkfiles without hashes").
- The kernal links only the system allocator, not BaseLib's global memory: about 24KB (2026-10-31).
- No floats in the kernal: a build error since 2026-11-06.  The kernal and the boot loader are built without the
  maths tables, which the start up program loads (the user, 2026-11-06).
- Executables are read straight from the card into the process's banks, not into the kernal's heap (2026-10-31):
  an executable can be bigger than the heap.
- Process banks aren't cleared; the emulator's fill mode shows memory nothing wrote instead, done by the emulator
  rather than the W65C816 (the user, 2026-11-06: "Stop zeroing memory on startup").
- Every process's bank 0 has a copy of the kernal's vectors, because the vector pull's first byte is read in user
  mode (the user, 2026-11-20).
- The ROM is a boot loader and the kernal is OS.SYS on the card, so the kernal changes without reprogramming the
  flash; the boot loader shares the kernal's start up units (2026-11-16).
- Drivers other than the SD Card Board's are relocatable modules on the card, named by the board's device ROM, and
  link against an export table (2026-11-06).
- The kernal tells the emulator by `WDM`, a no-op on a real W65C816, not through a debug board (2026-11-08).
- Images are flat, in the program's own heap, with kernal handles; SFT blits stay for the console and one-off
  pictures (planned 2026-11-21, built 2026-11-22).  Maki stays an SFT blit in the Sprites demo (the user,
  2026-11-22).
- Stack and NULL canaries in the program's bank 0, checked at every call (planned 2026-11-21, built 2026-11-22).
- The emulated SD Card Board works as intended, not as drawn (Claude's choice, 2026-10-31; not ruled on).

## Not done

- When [`CChunkFile::ReadOpen`] succeeds part way, or the object's chunks fail to read, [`KernalLoadExecutable`]
  and [`KernalLoadModule`] skip [`CChunkFile::ReadClose`] (they call it only `if (bLoaded)`), so the chunk stack
  and names it allocated stay in the kernal's 64KB heap; [`CChunkFile::Kill`] doesn't free them.  A bad executable
  or driver file leaks a little each time.
- A driver that fails after [`KernalDriverMemory`] gave it room (a short file, a missing export) keeps that MPU
  RAM.
- Start-up time hasn't been measured since the boot loader (2026-11-16) and the drivers on the card changed the
  boot path.  The levers left: a smaller modplay.exe (756,363 bytes on 2026-11-22: it links BaseLib, StandardLib
  and SupportLib) and a cluster map for seeks ([Libc65 Design](../../compiler/design/libc65.md#not-done)).
- BaseLib's small FAT units ([`BaseLib/FatCommon.cpp`], [`BaseLib/FatTime.cpp`], [`BaseLib/FatFormat.cpp`],
  [`BaseLib/FatDebug.cpp`], [`BaseLib/FatAbstractFile.cpp`]) are still compiled C++ in the kernal, about 2KB; the
  user had asked for all of the FAT code in assembly (2026-10-31).
- SD card writes are a sector at a time with the CRC16s and the status bits done in C++; no CMD25.
- `BRK` and `ABORT` return at once: no breakpoint or bad access handling in the kernal.
- Processes are nested only; there is no scheduler, and four process IDs.
- The kernal knows nothing of a bad access into a bank a program hasn't; the processor board's `Bad Access
  Detection.txt` describes a hardware way, not built.
