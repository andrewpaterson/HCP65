# Interrupts

The HCP65's boards tell the processor they have finished with IRQ, and the kernal sends each interrupt to the
board's driver, so no driver polls a board to learn that a block has arrived or a buffer has room.  This page says
how the kernal takes and dispatches interrupts, why it is built the way it is, and what isn't done.  The hardware
is in [On-board Registers](../registers.md) and [Interrupts from user
mode](../memory-map.md#interrupts-from-user-mode); the rest of the kernal is in [The Kernal](kernal.md).

The dispatch was planned in [`Kernal/KernalInterrupts.txt`] on 2026-11-05, when the user asked that the SD Card
Board's end of a block signal by IRQ rather than be polled, and every step of it was built on 2026-11-06.  It is
folded into this page.

## What the hardware gives

- IRQ is low while any device's interrupt line is, and level triggered: a handler must quieten its board before it
  returns, or the IRQ is taken again at once.  Registers 0 and 2 say which devices are interrupting.
- The processor board can't mask one device's interrupt.  Each board masks its own (its interrupt enable), the I
  flag masks them all, and a device held in reset (registers 3 and 5) stops signalling.
- Kernel mode is a non-zero interrupt depth (register 9), counted up by each vector pull and down by each `RTI`.
  An interrupt taken in user mode pushes its frame on the process's stack before the vector pull; then bank 0 is
  MPU RAM, so S holds the process's stack address in the wrong memory, and nothing may be pushed until S is moved.
- NMI is separate: the processor board's timer and break sources, latched in register 7.
- Most boards (SD Card, Sound, PS2, Video, Graphics, Blitter) use the Flags & Interrupt component board: reading
  Flags gives D6 Enable and D7 interrupt pending, and writing it sets D7 interrupt enable.  Writing back what was
  read with D7 clear turns only the interrupt off, and a latched interrupt stays latched while it is off.  The VIA,
  UART and LCD boards don't have it.

## Main memory contention

There is no arbiter on the main memory bus.  While the SD Card Board's DMA has it (a 512 byte block is about 1,100
processor clocks), a processor access to main memory latches Contention: main memory is shut off until reset and
NMI is held low.  Main memory holds every process's memory, stack and interrupt frames, the kernal's main memory
heap (the SD bounce buffer) and the maths tables that the compiler's multiply and divide routines read.  So:

- A DMA runs only while the processor is in kernel mode, in MPU RAM and ROM, and no process runs during it.
- An interrupt taken during a DMA may only run code that never touches main memory.
- The kernal and the boot loader are built without the maths tables (`MathTables="false"`), so their own multiplies
  and divides don't read main memory.

The Blitter Board's copies have the bus the same way ([`KernalMainCopy`]), and set the same flag while they run.

The emulator models it ([`CHCP65::SetMainMemoryBusy`], [`CHCP65::IsContention`]): main memory reads `$FF`, writes
are lost and NMI fires, so a kernal that breaks the rule is caught.  TestHCP65Contention checks the model with a
hand assembled program, and the kernal's boot and Blitter tests assert that it was never latched.

## Entry and exit

There is no compiler support for interrupt handlers (the `interrupt` attribute is step 8 of
[`CppCompilerLib/W65C816.txt`], not built), so the entry and exit are assembly, `KernalIrq` in
[`Kernal/Start.asm`]; the C++ is reached by `jsl` once the registers are saved.  Compiled code sets its own D and
doesn't use DBR, so the entry only saves and restores them.

- **From user mode** (the depth is 1: this vector pull counted it up from 0) A, X, Y, S and D go to
  [`gauiKernalIrqSave`] with long stores, as the `COP` handler does with [`gsKernalCall`], and S moves to where
  [`KernalEnterUser`] left the kernal's stack.  No call can be running, because `COP` sets I.  The exit puts S back
  last and `RTI`s from the process's stack.
- **From kernel mode** (a wait let the IRQ in) the registers are pushed on the kernal's stack as usual.

`KernalNmi` ([`Kernal/StartRuntime.inc`]) pushes nothing but its frame, since an NMI can come while an IRQ or a
call is changing stacks; it saves A in [`guiKernalNmiSave`], counts the timer's tick in [`guiKernalTicks`] and
clears it.  The timer runs at speed 7, a tick every 3.90625 ms, 256 a second ([`KernalInitInterrupts`]).

The boot loader has its own entry, `BootIrq` in [`Boot/BootStart.asm`], and the same dispatch: it only runs in
kernel mode.  Before it jumps to OS.SYS it puts every board back in reset, which clears their handlers
([`KernalStopDrivers`]), then sets I and turns the timer's NMI off ([`KernalStopInterrupts`]), so nothing
interrupts while the vectors are replaced.

## The dispatch

The dispatch, `_KernalIrqDispatch` in [`Kernal/KernalInterrupts.asm`], is assembly (the user, 2026-11-05), so no
multiply or divide can creep in from the compiler and it stays small; the handlers it calls are C++.  It reads
registers 0 and 2 once and calls the handler of every interrupting slot, then returns.  If a line is still low the
processor takes the IRQ again after the `RTI`; one pass an IRQ keeps the dispatch short.

The handlers are a table of 20 slots in MPU RAM ([`gasKernalInterrupts`], [`SKernalInterrupt`], 16 bytes each, read
by offset from the assembly): the handler, its context, the board's Flags register, whether it is trusted and
whether it is pending.  [`KernalSetInterruptHandler`] and [`KernalClearInterruptHandler`] change them with I set.
A driver registers through [`CKernalDriver::SetInterrupt`] and overrides [`CKernalDriver::Interrupt`].

| A slot that is interrupting | What the dispatch does |
|---|---|
| Has a handler, the main bus is free | Calls it |
| Has a trusted handler, the main bus is busy | Calls it |
| Has an untrusted handler, the main bus is busy | Writes its Flags back with D7 clear and marks it pending |
| Has no handler | Resets the device (its enable bit written 0 then 1) and sets its bit in the status's `uiStrayInterrupts` |

**Trusted handlers.**  A trusted handler never touches main memory: no process memory, no heap, no multiply or
divide.  It may run while [`gbKernalMainBusBusy`] says a DMA has the bus.  An untrusted one is put off: its board
is quietened through its Flags register, and [`KernalRunPendingInterrupts`] calls it once the read is over; it
turns its board's interrupt back on.  Nothing spins in the dispatch while the bus is busy.

**Stray devices.**  A device that interrupts with no handler would hold IRQ low for ever.  A board built on the
Flags & Interrupt board comes out of reset with Enable clear, so a reset pulse silences it until a driver enables
it again.  Resetting loses the board's state, which is why it is used only for strays, never to defer a board.

**The handler's contract.**  Quieten the board first (interrupt enable off, or Clear Interrupt), record what
happened (a flag, a counter), return.  No [`malloc`], no file system, no kernel calls.

## Waiting

The kernal runs with I set, and opens it only to wait.  [`KernalWaitForInterrupt`] is `wai`, `cli`, `nop`, `sei`:
`WAI` ends on an IRQ even with I set and goes on with the next instruction, so an interrupt that arrives between
the caller's test of its flag and the `wai` isn't missed; the `cli` then lets it be dispatched.  The timer's NMI
wakes it too, so a wait can time out: callers loop on their flag and [`KernalTicks`].  [`CKernalDriver::WaitFor`]
is that loop for drivers, and a kernel call that waits for a board, a key or the vertical blank sleeps in it.

Processes run with interrupts on (the user, 2026-11-05): they start with `P = $30`, so a board interrupts while a
program runs.  A kernel call starts with I set, because `COP` sets it.

## The SD Card Board

[`CKernalSDCard::ReadSectors`] writes the DMA address and counts, turns on the board's interrupt and sends CMD17 or
CMD18.  For each block it sets [`gbKernalMainBusBusy`], writes Read Start and waits for the board's interrupt,
three ticks at most (7.8 to 11.7 ms).  The handler, [`CKernalSDCardBoard::Interrupt`], is trusted: it quietens the
board (interrupt enable off, Clear Interrupt) and only then clears the busy flag and sets the block's done flag.
After the last block the read runs the deferred handlers ([`KernalRunPendingInterrupts`]).  A read into MPU RAM
copies out of the bounce buffer only after its DMA is over.

## Who uses it

| Board | Driver | Handler | Flags register |
|---|---|---|---|
| SD Card | [`CKernalSDCardBoard`] (built in) | trusted | yes |
| Sound | [`Drivers/SoundBoard/SoundBoard.cpp`] | trusted | yes |
| Video | [`Drivers/VideoBoard/VideoBoard.cpp`] | trusted | yes |
| PS2 | [`Drivers/PS2Board/PS2Board.cpp`] | trusted | yes |
| VIA | [`Drivers/VIABoard/VIABoard.cpp`] | trusted | none |
| UART | [`Drivers/UARTBoard/UARTBoard.cpp`] | trusted | none |

The sound board's interrupt first went to a kernel call of its own, `KernalAudioWait` (`COP 10`, 2026-11-06); since
the board has a driver, modplay sleeps in [`KernalDeviceWait`] on the board's counter instead, and calls 6 to 10
are gone.

TestHCP65KernalBoot runs modplay with interrupts on, puts a stray device in socket 3 that starts interrupting once
modplay plays, and checks it was reset and that main memory never saw contention.

## Decisions

The plan's five questions, all settled:

1. Other interrupts during an SD DMA are dispatched if their handlers are trusted and deferred if not, with a
   main-bus-busy flag the SD driver sets (the user, 2026-11-05).  The recommendation had been to keep them all
   masked during the DMA; the user chose this.
2. Processes run with interrupts enabled (the user, 2026-11-05).
3. A device that interrupts with no handler is reset, a pulse, not held in reset (the user, 2026-11-05).
4. The dispatch is assembly, the handlers C++ (the user, 2026-11-05).
5. The sound board's handler only signals, and the process sleeps until it does, rather than the kernal refilling
   the board from the process's buffer.  It was still open when the user asked for the dispatch to be built
   (2026-11-06: "do 1, 2, 3 and 5"), and was built as the recommendation.

And:

- Every driver quietens its board first, the SD Card Board's handler included (the user, 2026-11-05).
- A board with the Flags & Interrupt board is quietened by the dispatch itself from its Flags address, so deferring
  needs no driver code (the user, 2026-11-05).
- A deferred board is quietened and marked pending, not left interrupting (the user, 2026-11-05).
- One pass over the devices an IRQ; nothing spins in the dispatch (2026-11-05).

## Not done

- No driver has an untrusted handler, so the deferral path ([`KernalRunPendingInterrupts`], the Flags quietening)
  is used by nothing and tested by nothing.  The plan's quiet routines for the VIA, UART and LCD boards, which have
  no Flags register, weren't built: their handlers are all trusted, so they are never deferred.  An untrusted
  handler registered without a Flags register would make the dispatch write through a null pointer while the bus is
  busy.
- Only the SD read runs the pending handlers; a Blitter Board copy sets the busy flag but doesn't, so a handler
  deferred during one would wait for the next SD read.  The copy also polls the board's count rather than waiting
  for its interrupt.
- NMI's break sources are left alone (they are the emulator's breakpoints' business); `BRK` and `ABORT` return at
  once.
- C interrupt handlers in the compiler (W65C816.txt step 8) aren't built; the entry and exit stay assembly.
- The LCD and SNES Board's driver doesn't use interrupts.
