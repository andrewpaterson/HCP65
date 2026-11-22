# Memory Map

The W65C816 has a 24 bit address bus: 256 banks of 64KB, 16MB in all.  What those addresses reach
depends on whether the processor is in kernel mode or user mode.

## Kernel mode and user mode

The processor board counts how deeply it is nested in interrupts (*HCP65 Kernal Mode* sheet).

- The count goes up by one each time the processor fetches an interrupt vector: IRQ, NMI, BRK, COP,
  ABORT or RESET.
- It goes down by one each time an `RTI` (opcode `$40`) is executed.
- A count of **zero is user mode**.  Any count **above zero is kernel mode**.

Reset clears the count, and fetching the reset vector counts it up to one, so the processor starts
in kernel mode.  The kernel starts a user process by returning to it with `RTI`.

The count can be read and written through the [Interrupt Depth](registers.md#9-interrupt-depth)
register.

## Kernel mode

In kernel mode the bottom 2MB holds the processor board's own memory and the devices, and main
memory starts at 2MB (*HCP65 MPU Address Decode* sheet).

| Address | Size | What |
|---|---|---|
| `$000000`–`$07FFFF` | 512KB | MPU RAM |
| `$080000`–`$0FFFFF` | 512KB | MPU ROM |
| `$100000`–`$107FFF` | 32KB | [On-board registers](registers.md) (device slot 0) |
| `$108000`–`$13FFFF` | 7 × 32KB | [Devices 1–7](devices.md#device-sockets) |
| `$140000`–`$17FFFF` | 256KB | Nothing |
| `$180000`–`$1FFFFF` | 4 × 128KB | [Devices 16–19](devices.md#device-sockets) |
| `$200000`–`$FFFFFF` | 14MB | [Main memory](devices.md#main-memory) |

The kernel cannot reach the first 2MB of main memory; that part is only reachable from user mode.

The MPU RAM is an AS7C34096 static RAM.  The MPU ROM is an SST39LF040 flash chip whose write line is
connected, so it can be reprogrammed in place using the flash chip's command sequences.

### RAM and ROM swap at boot

The processor starts in emulation mode, and its reset vector is read from the top of bank 0.  For
that to come from ROM, the RAM and ROM chip selects start out swapped:

| | Before the first switch to native mode | After it |
|---|---|---|
| `$000000`–`$07FFFF` | MPU ROM | MPU RAM |
| `$080000`–`$0FFFFF` | MPU RAM | MPU ROM |

A latch on the *W65C816 Breakout* sheet is set when the processor's E output first goes low (the
first `XCE` into native mode) and is only cleared by reset.  So the boot loader in the ROM switches
to native mode, then copies itself from the ROM into RAM and carries on running there
([Starting Up](boot.md)).

The [NMI timer](registers.md#6-timer-speed-select) is also held stopped until this latch is set.

## User mode

In user mode there are no devices, no MPU RAM and no MPU ROM: every address goes to
[main memory](devices.md#main-memory), all 16MB of it.

### Bank remapping

Each user process sees its own banks.  On every access the processor board replaces the bank
byte (address bits A16–A23) with a byte looked up in a table in MPU RAM:

```
physical bank = MPU RAM [ $070000 + ProcessID × $100 + bank ]
```

The table is the top 64KB of MPU RAM: one 256 byte page per process, one byte per bank.  The
[Process ID](registers.md#8-process-id) register picks the page.  For example, if process 5's
page holds `$42` at offset 0, then process 5's bank 0 is physical bank `$42` of main memory.

The lookup happens in the first half of each bus cycle, while PHI2 is low.  No remapping is done in
kernel mode; the kernel sees physical addresses.

### Interrupts from user mode

An interrupt taken in user mode does all of its stack work in user mode, on the process's own stack
in its own bank 0.  The processor pushes the program bank, the return address and the status first;
only the vector pull (VPB low) counts the [kernel mode count](#kernel-mode-and-user-mode) up.  `RTI`
counts it back down as soon as its opcode is fetched, so its pulls come back from the same stack.

The count is decoded straight away, but the rest of the processor board takes kernel mode from a
latch that changes at the end of each cycle.  So the vector's first byte is still read in user mode,
from the process's own bank 0, and only its second byte comes from the kernel's bank 0.  For that
reason **every process's bank 0 holds the same vectors as the kernel's**: when the kernel loads a
program it copies `$FFE0`–`$FFFF` of its bank 0 into the process's bank 0.  A process's stack ends
below them, at `$EFFF`.

A program can overwrite its own copy of the vectors, since its bank 0 is its own memory.  That only
breaks the program that does it: the next interrupt it takes goes wrong, and no other process is
affected.

### A process's bank 0

| Address | What |
|---|---|
| `$0000`–`$0007` | The NULL canary |
| `$0100`–`$01FF` | The direct page the program starts with |
| `$0200`–`$0207` | The stack canary |
| `$0208`–`$EFFF` | The stack, from `$EFFF` down |
| `$FFE0`–`$FFFF` | The copy of the kernel's vectors |

The kernel writes the two canaries, eight bytes of a pattern, when it loads the program, and looks
at them at each kernel call the program makes.  A NULL pointer's reads and writes land at the bottom
of the bank, so a write through one changes the first; a stack that grows past its room writes over
the second before anything else.  Either way the kernel ends the program at its next call, and
`KernalExecute` returns `KERNAL_RESULT_NULL_WRITE` or `KERNAL_RESULT_STACK_OVERFLOW` (with an exit
code of 0) instead of `KERNAL_RESULT_EXITED`.  It doesn't say which instruction did it, only that it
happened since the program's last call; and a write past a canary that misses its eight bytes, or a
NULL pointer's read, isn't noticed.  The direct page starts above the NULL canary so that nothing a
program does on purpose touches it.  The check costs about 200 cycles of each call, which takes
about 2,900 without it.
