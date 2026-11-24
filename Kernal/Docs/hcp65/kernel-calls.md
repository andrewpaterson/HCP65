# Kernel Calls

A program asks the kernel for everything it can't do itself: running another program, reading and
writing files, more memory, the boards in the device sockets and the screen.  Each of these is a
kernel call, made with the W65C816's `COP` instruction.  This page is how a call is made and what
every one does.  The calls are declared in `HCP65/Libraries/UserLib/HCP65User.h`, and a program
links UserLib's files for the ones it uses.

## How a call is made

A kernel call is `COP n`, with A holding the address of the call's arguments.  `n`, the
instruction's signature byte, says which call it is.

| `COP` | Call | Arguments |
|---|---|---|
| 0 | Exit | The exit code in A (`UserStart.asm`: returning from `main` makes it) |
| 1 | Put value | A word |
| 2 | Execute | A path, an argument and where the exit code goes |
| 3 | Get argument | None |
| 4 | File size | A path |
| 5 | File load | A path, where to and the most to read |
| 11 | File read | A path, an offset, where to and how many |
| 12 | Device call | A block ([`SKernalDeviceCall`]) |
| 13 | System call | A block ([`SKernalSystemCall`]) |
| 14 | Screen call | A block ([`SKernalScreenCall`]) |

Calls 6 to 10 were the sound board's, before it had a driver; they are no longer used.

UserLib does this for a program: each call is a C function (`UserCalls.asm` for the simple ones,
`UserFiles.cpp`, `UserDevices.cpp` and `UserScreen.cpp` for the ones made of a block).  The
function's arguments are pushed in order on the program's stack, A is set to where the first of them
is, and the `COP` is made; the result comes back in A, or A (low word) and X (high word) for a long.
The three block calls take one block in the program's memory, which the kernel reads and writes the
answers into: one `COP` number covers many calls of a kind.

### What the kernel does with a call

`COP` is an interrupt, so it takes the processor into kernel mode
([Memory Map](memory-map.md#interrupts-from-user-mode)).  The kernel saves the program's registers,
runs the call on its own stack, and returns to the program with `RTI` with the result in its
registers.  A call runs with interrupts off, but where it waits (for a board, for a key, for the
screen's vertical blank) it lets them in while it does.

Before anything else, each call looks at the program's
[canaries](memory-map.md#a-processs-bank-0): if the program has written over either, the call
ends it instead (`KERNAL_RESULT_STACK_OVERFLOW` or `KERNAL_RESULT_NULL_WRITE` for the program that
ran it).

A call is given addresses in the program's memory, as the program sees it.  The kernel reads and
writes them through the program's page of the remap table, a bank at a time, so what it is given can
be anywhere the program has memory, across banks too.  An address in a bank the program hasn't is
refused (`KERNAL_RESULT_BAD_ADDRESS`, or `KERNAL_DEVICE_BAD_ARGUMENT` for the device and screen
calls): a call can't reach another program's memory or the kernel's.

On the emulator at the board's 25MHz the shortest call takes about 3,100 processor cycles from the
`COP` to the program running again.  Most of that is getting into the kernel and back out; a call
that reads a file or waits for a board takes as long as that does.

### Results

The program, file and memory calls return a `KERNAL_RESULT_*`:

| Result | Value | What |
|---|---|---|
| `KERNAL_RESULT_SUCCESS` | 0 | |
| `KERNAL_RESULT_NOT_FOUND` | 1 | No such file (or directory) |
| `KERNAL_RESULT_BAD_FILE` | 2 | Not an executable, or a file that couldn't be read or written |
| `KERNAL_RESULT_NO_MEMORY` | 3 | Not the memory: for a program to run, or a bank to map |
| `KERNAL_RESULT_NO_ENTRY` | 4 | An executable without `UserStart` |
| `KERNAL_RESULT_EXITED` | 5 | [`KernalExecute`]: the program ran and exited |
| `KERNAL_RESULT_NO_PROCESS` | 6 | [`KernalExecute`]: all four process IDs are in use |
| `KERNAL_RESULT_BAD_ADDRESS` | 7 | Memory the program hasn't, or an executable with code where its stack or heap goes |
| `KERNAL_RESULT_END` | 8 | [`KernalDirectoryEntry`]: there are no more entries |
| `KERNAL_RESULT_STACK_OVERFLOW` | 9 | [`KernalExecute`]: the program's stack grew past its room |
| `KERNAL_RESULT_NULL_WRITE` | 10 | [`KernalExecute`]: the program wrote through a NULL pointer |

The device and screen calls return a `KERNAL_DEVICE_*`:

| Result | Value | What |
|---|---|---|
| `KERNAL_DEVICE_SUCCESS` | 0 | |
| `KERNAL_DEVICE_NOT_SUPPORTED` | 1 | The board's driver doesn't do that |
| `KERNAL_DEVICE_BAD_UNIT` | 2 | The board hasn't that unit |
| `KERNAL_DEVICE_IN_USE` | 3 | Another program has the board (or the unit, or the screen) |
| `KERNAL_DEVICE_NOT_OPEN` | 4 | A screen call that needs the picture, from a program that hasn't it |
| `KERNAL_DEVICE_BAD_ARGUMENT` | 5 | |
| `KERNAL_DEVICE_FAILED` | 6 | The board didn't do it |
| `KERNAL_DEVICE_TIMEOUT` | 7 | [`KernalDeviceWait`]: what it waited for didn't happen in time |
| `KERNAL_DEVICE_NOT_FOUND` | 8 | No such board, no driver for it, or no board at that index (a scaled blit's Graphics Board) |
| `KERNAL_DEVICE_NO_HANDLES` | 9 | The kernel has no handles left (device handles, or images) |
| `KERNAL_DEVICE_BAD_HANDLE` | 10 | Not a handle of this program's |
| `KERNAL_DEVICE_NO_MEMORY` | 11 | [`SFTImageMake`]: not the memory for the image |

## Programs

A program is an executable on the SD card, made by the linker
([Building Programs](../compiler/building.md)).  The first is the start up program
([Starting Up](boot.md)); the shell runs the rest ([The Console and the Shell](console.md)).

| Call | What it does |
|---|---|
| `KernalExecute(path, argument, &exitCode)` | Runs the executable at `path` as a new process and waits for it to end.  `KERNAL_RESULT_EXITED` with its exit code, or why it couldn't run (or that the kernel ended it). |
| [`KernalGetArgument()`] | The argument the program was run with: the start up program's is the number after its path in `\OS\STARTUP.TXT`, the shell gives the number after a program's name. |
| Exit | Returning from `main`, or [`exit`] (Libc65), with the exit code. |
| `KernalPutValue(value)` | Gives the kernel a word it keeps in its status, for tests to read. |

A process runs while the one that ran it waits in its [`KernalExecute`]: processes are nested, not run
side by side.  The start up program is process `$FF`, and a process run by another gets the next ID
down, to `$FC`: four at once.  Each has its own memory ([Memory Management](memory.md)).

The kernel loads the executable into banks of the new process's own, maps its heap, puts its
argument where [`KernalGetArgument`] finds it, and starts it at `UserStart` in user mode.  When it
ends, everything it had goes back: its memory, its device handles (and the boards it had open), the
screen if it had the picture, and its images.  The emulator's debugger is told of each process
starting and ending.

## Files

The SD card's first FAT32 volume.  A path is from the volume's root, with backslashes
(`\OS\INIT.EXE`), up to 127 characters; names are FAT's, so case doesn't matter.

| Call | What it does |
|---|---|
| `KernalFileSize(path)` | The file's size in bytes, or `KERNAL_FILE_NOT_FOUND` (`$FFFFFFFF`). |
| `KernalFileLoad(path, dest, maxSize)` | Reads the file to `dest`, up to `maxSize` bytes.  `KERNAL_RESULT_SUCCESS` if it read the whole file. |
| `KernalFileRead(path, offset, dest, size)` | Reads `#!text size` bytes of the file from `offset`.  `KERNAL_RESULT_BAD_FILE` if the file hasn't them all. |
| `KernalFileWrite(path, source, size)` | Writes the file whole: made if it isn't there, emptied first if it is. |
| `KernalFileDelete(path)` | Deletes the file. |
| `KernalDirectoryEntry(path, index, &entry)` | The `index`th entry (from 0, without `.` and `..`) of the directory: its name, size and attributes (`KERNAL_FILE_*`).  `KERNAL_RESULT_END` after the last. |
| `KernalMakeDirectory(path)` | Makes the directory. |

Reading a whole file at once wants the memory for it: [`KernalFileSize`], then [`malloc`] that much and
[`KernalFileLoad`].  [`KernalFileRead`] reads a part of one, so a program can take a big file a piece at
a time.  The last four calls are system calls (`COP 13`, [`SKernalSystemCall`]).

## Memory

| Call | What it does |
|---|---|
| `KernalMemoryInfo(&info)` | Main memory's banks: all of them, those programs can have and those no program has; and the size and use of the kernel's own heap. |
| `KernalMapBanks(bank, count)` | Gives the program `count` banks of its heap from `bank`, all or none. |
| `KernalUnmapBanks(bank, count)` | Takes those of them it has back. |

Libc65's [`malloc`] makes the mapping calls itself as its heap grows and shrinks, so a program seldom
does.  [Memory Management](memory.md) says how a program's memory is arranged and what the calls
do.  They are system calls too.

## Devices

The boards in the device sockets, through their drivers: [`KernalDeviceInfo`], [`KernalDeviceIsOpen`],
[`KernalDeviceOpen`], [`KernalDeviceClose`], [`KernalDeviceRead`], [`KernalDeviceWrite`],
[`KernalDeviceControl`] and [`KernalDeviceWait`] ([Devices and Main Memory](devices.md#using-a-board-from-a-program)).
A program opens a unit of a board and gets a handle, which the other calls take; the kernel has 32
handles for all the programs together.  Waits are in ticks, 256 to a second.

Every program has the console's two handles, 0 for what is typed and 1 for text, without opening
them ([The Console and the Shell](console.md#the-console)).

## The screen

The picture the computer shows, which the kernel draws on the Video Board: setting a mode, the
frames, palettes, writing and reading pixels, blits of SFTs and images, and images' handles
([The Screen](screen.md)).  [Sprites](sprites.md) is how to draw a moving picture with them.

UserLib's `HCP65SFT.h` has what isn't a call but works with them: finding an image or a palette in
an SFT container, the palette of the colours an SFT pixel is, a palette's hash, and making an image
from an SFT ([`SFTImageMake`], which allocates and unpacks it, then gives the kernel a handle to it).
