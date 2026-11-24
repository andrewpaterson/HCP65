# Memory Management

The HCP65 has two kinds of memory.  The processor board's 512KB of MPU RAM is the kernel's: its code,
its data, the drivers and the remap table.  Main memory is the memory boards', up to 16MB, and it is
what programs run in.  Both are counted in banks of 64KB, the W65C816's.  This page is how the kernel
shares them out: what it keeps, what each program is given and how a program's heap grows and
shrinks.  [Memory Map](memory-map.md) is where everything is.

## Main memory's banks

At start up the kernel finds the memory boards and gives each a window of main memory
([Devices and Main Memory](devices.md#main-memory)).  It then has a table of main memory's 256
physical banks, each one of:

| Kind | What |
|---|---|
| Absent | No chip answers it.  Physical bank 0 is always taken as absent: a remap entry of 0 means "not mapped". |
| Tables | The four banks of the [maths tables](runtime/w65c816.md#the-maths-tables), which every process shares. |
| The kernel's heap | Banks the kernel's main memory heap has taken. |
| For processes | The rest: each either given to a process or free. |

The kernel keeps a count of the banks for processes and of those free, and the lowest free one, so
giving a bank out doesn't search the table.  Processes are given banks from the bottom of main
memory up, the lowest free first; the kernel's heap takes them from the top down.  With one Memory
Board (window 1, `$400000`–`$7FFFFF`) the tables are banks `$40`–`$43`, the kernel's heap starts at
`$7F`, and processes have the banks from `$44` up.

**One bank is always kept free.**  No process is given main memory's last free bank: the kernel's
heap may need it.  A mapping that would take it fails with `KERNAL_RESULT_NO_MEMORY`.

## The kernel's memory

The kernel has two heaps.

**MPU RAM's heap** is bank 6, `$060000`–`$06FFFF`, below the remap table: the kernel's own [`malloc`]
(Libc65's, over that one bank), which is always there.  [`KernalMemoryInfo`] gives its size and how
much of it is used.  The drivers the kernel loads go in MPU RAM from the end of its own data up to it, and the
remap table is bank 7.

**The main memory heap** is for what must be in main memory: what a board reads or writes there (the
SD Card Board's DMA, the Blitter Board's copies), and what a driver keeps that is too big for MPU
RAM (the Video Board's driver's font).  Drivers have it as `KernalMainMalloc(bytes, oneBank)` and
[`KernalMainFree`].  It is BaseLib's [`CBankMallocator`]: when it hasn't room it takes free banks from
the top of main memory, and when a bank has nothing in it any more it gives it back, but for one it
keeps as a spare.  What is asked for with `KERNAL_MAIN_ONE_BANK` is all in one bank, for a board or
a 16 bit index that doesn't count past one; anything else may cross banks, and more than a bank is
a run of neighbouring banks.

## A process's memory

Each process sees its own 16MB: in user mode every bank of an address is looked up in the process's
256 byte page of the remap table ([Bank remapping](memory-map.md#bank-remapping)).  A process is given:

| Banks | What it is given |
|---|---|
| `$00` | A bank of its own: its stack, its direct page, the kernel's vectors and two canaries ([A process's bank 0](memory-map.md#a-processs-bank-0)) |
| `$01`–`$1F` | The banks its code and data are in, at the addresses they were linked at |
| `$20`–`$3F`, `$44`–`$FE` | Its heap, as it asks for it, and `UserHeapSize` of it at once if the program has that symbol |
| `$40`–`$43` | The maths tables: the same physical banks in every process |
| `$FF` | A bank of its own, with the heap block at `$FF0300` |

The kernel reads the executable's sections straight from the SD card into the process's banks.  A
section anywhere else (in bank 0, or from `$200000`) is refused, `KERNAL_RESULT_BAD_ADDRESS`, before
anything is mapped: the linker's `-limit` normally stops a program that big being made at all.
Banks aren't cleared before a process has them; a program's start up clears its own `bss`.

### The heap

Libc65's [`malloc`] (on the W65C816, BaseLib's [`CBankMallocator`]) runs the heap inside the program.
Every bank of the heap is a region of its own, so no block crosses a bank.  Small blocks come from
the banks at the bottom of the heap.  A block too big for a bank (from [`malloc32`], or [`malloc`]'s
largest sizes) is whole neighbouring banks from the top, rounded up to 64KB.  [`free`] and [`realloc`]
take both kinds.

The heap is mapped as it is used.  At its first [`malloc`], Libc65 reads the heap block the kernel
wrote at `$FF0300` ([`SKernalHeapInfo`]: what was mapped at load, the `HEAP` marker, and the heap's
first byte and the byte past its last), and from then on:

- a bank is mapped ([`KernalMapBanks`]) the first time the heap puts a block in it;
- a bank is given back ([`KernalUnmapBanks`]) when its last block is freed, but one empty bank is kept
  for what comes next;
- the maths tables' banks are never used;
- [`malloc`] returns NULL when main memory has no free bank left but the one the kernel keeps.

Only taking or giving back a bank is a kernel call; every other allocation is as fast as anywhere
else.  A program run without the kernel (on the compiler's test machine, which has no heap block) has its
whole heap there from the start.

### Mapping and unmapping

`KernalMapBanks(bank, count)` gives the program `count` banks of its heap from `bank`, the address
of the first one's first byte:

- every bank must be the heap's: from `$200000` to `$FEFFFF` but the tables', or the call is
  `KERNAL_RESULT_BAD_ADDRESS`;
- it maps all of them or none: if that would leave main memory with no free bank,
  `KERNAL_RESULT_NO_MEMORY`, and nothing changes;
- a bank the program already has is left as it is;
- each new bank is the lowest free physical bank, so a program's banks needn't be next to each
  other in main memory.

`KernalUnmapBanks(bank, count)` takes back those of the banks the program has and leaves the rest.

On the emulator at 25MHz mapping one bank takes about 3,500 processor cycles from the `COP` to the
program running again, and unmapping one about the same; each bank more in the same call adds about
800 cycles to a mapping and 150 to an unmapping.

## When a process ends

Every physical bank the process had goes back, and its page of the remap table is cleared.  Its
device handles are closed (and the boards let go), the screen goes back to the console if it had
the picture, and the kernel drops its [images](screen.md#images)' handles; the images themselves
were in its heap, so they went with it.  The process ID is free for the next.

## Mistakes

The kernel catches two common ones, with the [canaries](memory-map.md#a-processs-bank-0) in a
process's bank 0: a write through a NULL pointer, and a stack that grows past its room.  Each kernel
call looks at them (about 200 cycles of the call); a changed one ends the program, and
[`KernalExecute`] returns `KERNAL_RESULT_NULL_WRITE` or `KERNAL_RESULT_STACK_OVERFLOW` to the program
that ran it.  They are noticed at the next call, not when they happen.

What nothing catches is a pointer into a bank the program hasn't.  Its remap entry is 0, which is
physical bank 0: no process's and not the kernel's (with fewer than four Memory Boards there is
nothing there at all), so such a write harms no one but the program that makes it, which reads back
whatever the bus has.  The same is true of a heap block used after it was freed, which the heap may
have given to something else.

## On the emulator

In the emulator's fill mode every bank the kernel gives a process is filled with the fill byte
first, so a program that reads memory it never wrote sees the same byte each time, and a debugger
can tell what hasn't been written.  The kernel tells the emulator of each bank it gives, through
the debug port, for this.
