# The Hardware

[Memory Map](../memory-map.md), [On-board Registers](../registers.md) and [Devices and Main Memory](../devices.md)
say what the HCP65's hardware does.  This page says where that description came from, how HCP65Lib models the
boards, and what it leaves out.  The program that runs the model is on [The Emulator](emulator.md).

## Where the description comes from

There is no specification of the HCP65 other than its boards: the KiCad projects in `Kicad/Computer`, one folder
per board family (Processor, Main, Memory, SD Card, Audio, Video and the rest; `Component Boards` holds the
standard daughter boards they share).  The memory map, the on-board registers, the kernel mode logic, the device
decode and the main memory channel were read from the schematics' netlists (`kicad-cli sch export netlist`) on
2026-10-31, at the user's request, and each section of the user pages names the sheet it comes from.  The device
boards were read the same way as each was emulated (the SD Card Board's on 2026-11-05).  `kicad-cli` rewrites the
project it exports from, so it is run on a copy (the recipe is in `CLAUDE.md`).

Reading the netlists found mistakes in the schematics, which the user corrected there on 2026-10-31:

| Where | Wrong | Corrected |
|---|---|---|
| Processor Board, Address Decode V2 to V3 | The `~Main` labels were the wrong way round | Kernel mode has main memory from `$200000`, user mode all 16MB |
| HCP65 Kernal Mode | The interrupt depth counter's outputs were labelled in reverse order | The user ruled the counter isn't reversed: IC9's Q labels were fixed |
| Main Memory Channel | Each socket's two window bits were swapped | The lower data bit is the low bit of the window |
| Audio Clock | The DAC's `~WR_B` came from the '138's Y3 | Y5, with A0 kept on Q2: Q1 would change A0 on the same edge that ends each write |

No board had been made then, and these boards' PCBs haven't changed in git since.

Later readings put each board's faults in a file beside its schematic, `<board> Faults.txt` (for example
`Kicad/Computer/SD Card/SD Card Board/SD Card Board Faults.txt`, first written on 2026-11-05):  what is wrong, what
it does, and the fix.  The emulator follows the boards **as they are meant to work**, not as they are drawn, and a
header comment says so where the two differ ([`HCP65Lib/HCP65SDCard.h`], [`HCP65Lib/HCP65LCD.h`],
[`HCP65Lib/HCP65Blitter.h`], [`HCP65Lib/HCP65Graphics.h`], [`HCP65Lib/HCP65Video.h`], [`HCP65Lib/HCP65PS2.h`]).
When what is meant isn't clear from the schematic, the user says, and the ruling goes in the faults file: kernel
mode is the clearest case (`Kicad/Computer/Processor/HCP65 Kernal Mode/Kernal Mode Faults.txt`).

## Kernel mode

The [kernel mode](../memory-map.md#kernel-mode-and-user-mode) rule is the user's (2026-10-31, and in full on
2026-11-20): the interrupt depth count is zero in user mode and above zero in kernel mode, and kernel mode is
forced until the Native Latch is set.  The Kernal Mode sheet's gates as drawn do something else (they AND the
count's bits, and hold kernel mode off once the latch is set); the faults file gives the fixes.

[`CHCP65`] decides the mode in one place, [`CHCP65::UpdateMode`]: kernel mode is a non-zero depth or a processor
that hasn't yet been native.  [`CFastW65C816`] tells it of the bus events that change the count or the latch
through its signal callback (the vector pull, the vector's second byte, `RTI`'s opcode, the first switch to native
mode, `WDM`).  The depth counts up on the vector pull, but the mode only changes for the vector's second byte: the
board latches the mode at the end of each cycle, so the vector's first byte is read in the mode interrupted.  That
is why every process's bank 0 holds a copy of the kernal's vectors.  `RTI` counts down and changes the mode at its
opcode fetch, so its pulls come from the stack it is returning to.

## How HCP65Lib models it

### The processor and its memory

[`CHCP65`] is built on a [`CFastW65C816`] (MicroprocessorLib's instruction level W65C816), with the processor
board's memory, its registers, the main memory channel, a debug port and twenty device slots.  The processor's
reads and writes go through two callbacks that decode the address as the board does: the mode, the RAM/ROM swap
before the Native Latch, the device slots, and in user mode the bank remap through the table at `$070000`.  The one
exception is the common case: in kernel mode after the swap, MPU RAM is the processor's own flat block (its "hole"
is closed), so the kernal's accesses to MPU RAM cost no callback.

Main memory is a table of 256 bank pointers ([`CHCP65::MapMainMemory`]), filled from each memory board's chips
([`CHCP65Device::GetMemoryChip`]) at the window the main memory channel gives its socket.  It is mapped again when
a board is plugged in or the channel is written.  A bank no chip answers reads `$FF` and drops writes; two sockets
in one window answer in the channel's socket order.  A chip whose writes its board must see (the Video Board's,
while a pixel write also writes its palette byte) is marked watched and its writes go to
[`CHCP65Device::WriteMemoryChip`].

The MPU ROM is the boot loader's image ([`CHCP65::LoadROM`]); writes to it are ignored.

### Boards

Every board is a [`CHCP65Device`]: [`CHCP65Device::Read`] and [`CHCP65Device::Write`] take the offset in its slot,
[`CHCP65Device::IsInterrupting`] is its interrupt line, [`CHCP65Device::SetHeldInReset`] follows the Device
Reset/Enable registers.  The processor board's registers are slot 0 ([`CHCP65Registers`]) and the main memory
channel slot 1 ([`CHCP65MemoryChannel`]); the rest are plugged in from a mainboard description.

| Board | Class | Notes |
|---|---|---|
| On-board registers | [`CHCP65Registers`] | The NMI timer counts the 32.768kHz crystal once the Native Latch is set. |
| Main memory channel | [`CHCP65MemoryChannel`] | Latches nothing resets; the emulator's start with every socket in window 0, the real ones with anything. |
| Memory Board | [`CHCP65MemoryBoard`] | Two 2MB chips, kept through resets. |
| SD Card Board | [`CHCP65SDCard`] | [Below](#the-sd-card-board). |
| Sound Board | [`CHCP65Audio`] | [Below](#the-sound-board). |
| UART Board | [`CHCP65UARTBoard`], [`CHCP65UART`] | Two TL16C550Ds on their own 1.8432MHz oscillators; the far end is the host, or a [`CHCP65Terminal`]. |
| VIA Board | [`CHCP65VIABoard`], [`CHCP65VIA`] | Up to eight W65C22S, two per optional breakout; PHI2 is half the CPU clock (VIA Control's clock holds aren't emulated). |
| PS2 Board | [`CHCP65PS2Board`], [`CHCP65PS2Port`] | Two ports, the line protocol in microseconds; [`CHCP65PS2Keyboard`] is an MF2 keyboard on scan code set 2. |
| LCD and SNES Board | [`CHCP65LCDBoard`], [`CHCP65LCDController`] | A KS0066 16 by 2 display with its execution times, and two SNES controller latches. |
| Video Board | [`CHCP65VideoBoard`] | Works a line at a time; its memory windows are private. |
| Blitter Board | [`CHCP65BlitterBoard`] | Copies in main memory as the bus master, 2 processor cycles a byte. |
| Graphics Board | [`CHCP65GraphicsBoard`] | A Blitter Board that reads each byte through an offset table. |

Each board makes its own device ROM with DeviceRomLib's [`CDeviceROM`], as the real board's image would be, so the
kernal finds and drives it as it would a real one.  TestHCP65Lib checks each board's ROM against what
[`CDeviceROMSchematic`] works out from its KiCad schematic, so an emulated board and its schematic can't drift
apart unnoticed.

[`CHCP65Mainboard`] reads a mainboard description (an XML file: which board is in which socket, and each board's
own settings such as an SD card's image) and makes and owns the boards.  [`EHCP65Mainboard`] is the mainboard: the
Small Mainboard has sockets 3, 4 and 16 to 19 (1, 2 and 7 are on the board), "Any" has every socket, and the Full
Mainboard (three processors) is refused.  [`CHCP65Mainboard::ChangeBoards`] swaps boards at a system reset and
keeps those taken out, with their memory and cards, for when the same kind is asked for again.

### Time

The processor's clock is the time: every board is driven by CPU cycles.  `HCP65_DEFAULT_CLOCK_RATE`
([`HCP65Lib/HCP65.h`]) is 25MHz, the board's LTC6903 oscillator as set.  A board with its own clock converts with
[`CHCP65::DeviceTick`] and [`CHCP65::CycleOfDeviceTick`] (the UARTs' oscillators, the VIAs' PHI2, the PS/2 lines'
microseconds), or [`CHCP65::CrystalTick`] for the 32.768kHz crystal (the NMI timer and the sound board).

A board keeps its own time lazily.  [`CHCP65Device::CatchUp`] is called with the cycle being accessed before every
access to it, and at the end of every run chunk; [`CHCP65Device::NextEvent`] says when its interrupt will next
change by itself.  [`CHCP65::Run`] cuts the run into chunks that end at the earliest of those, so an interrupt is
raised on the instruction it should be, and a write that can move an event (a timer started, a board enabled) ends
the chunk after the instruction.  Nothing is stepped a cycle at a time.

### Main memory contention

There is no arbiter on the main memory bus.  The user's design (2026-11-05) is that contention shuts main memory
off: Mainboard Small's Main Memory Full Abort compares the `~Main` lines, Main Memory Small Enable latches
Contention until reset, forces main memory off and raises an NMI.  So a board that masters the bus (the SD Card
Board's DMA, the Blitter and Graphics Boards' copies) owns it until its transfer ends, and the kernal must keep the
processor off main memory meanwhile.  The emulator models this: the board says until when
([`CHCP65::SetMainMemoryBusy`]), and a processor access to main memory before then latches
[`CHCP65::IsContention`], after which main memory reads `$FF`, writes are lost and NMI is held low until reset.
The kernal's rules for keeping off main memory are on [Interrupts](interrupts.md#main-memory-contention), and its
tests assert contention never happens.  The boards' own bus drivers aren't stopped on the real board; nothing in
the emulator stands for them.

### The SD Card Board

The board runs its cards in **4 bit mode**, the SD bus with DAT0 to DAT3, not SPI and not 1 bit (the user,
2026-11-05): a sector moves as nybbles on the four lines.  The kernal's driver (`Kernal/KernalSDCard.cpp`, ACMD6
with argument 2 as it brings a card up) and the emulated board are both 4 bit, and must stay so.  The board has
four sockets (two full size, two micro), Flags D0 and D1 choosing which one the bus reaches.  There is no card
detect: the driver probes each socket for a card at start up.

[`CHCP65SDCard`] is the board as it is meant to work, with an SDHC card (block addressed, an image file in memory)
in each socket:

- Commands and responses are bit banged through the GPIO register: SD CLK, CMD, DAT0 and DAT3.  The card samples
  CMD on CLK's rise and changes its outputs on the fall, checks each command's CRC7 and ignores a bad one.
- A block read (CMD17, or CMD18 until CMD12) is started by Read Start; the board clocks the card and stores Data
  Count bytes at the DMA address, then latches its interrupt.  The bytes are stored when the transfer ends, timed
  as SD CLK at the CPU's clock: the card's access time of 8 clocks, then two clocks a byte for the data and CRC
  bytes.  As drawn, SD CLK is the MPU clock (Fast CLK).
- A block write is the DAT stream written a byte (two nybbles) at a time to Write Data: start nybble, 1024 data
  nybbles, each line's CRC16, end nybble.  The card checks the CRC16s and gives its CRC status and busy on DAT0.
- ACMD6's bus width is recorded but a read always moves nybbles on four lines, as the board does.

The user's intent for the board, assuming its faults fixed (2026-11-05): at start up the driver finds the cards in
the four sockets, switches each to high speed 4 bit mode (up to about 50MHz), and a block read uses all four lanes
with Counter 24bit as the DMA address, storing a byte every two SD clocks.  The small mainboard runs the MPU, and
so SD CLK, at about 12.5MHz for testing, and the large mainboard at 25MHz: both within default speed (25MHz at
most), so high speed is only needed above that.  High speed would also need a faster clock and the read latch moved
off the rising edge (item 10 of the faults file).

### The Sound Board

[`CHCP65Audio`] is the board as the user was correcting it on 2026-10-31: RAM A plays on the left and RAM B on the
right through the 8 bit stereo DAC, the interrupt is latched until cleared, and enabling the board loads the
counter from Start.  Each tick of the sample clock (the crystal divided by 1, 2, 4 or 8) writes a frame to the DAC.
The frames are kept, unsigned 8 bit left and right pairs, in a buffer under a lock (64K frames, the oldest dropped
when it is full) for the host to play on its own thread; [The Emulator](emulator.md#sound) plays them.

### Debugging aids

The board has none of these; they are the emulator's:

- Breakpoints and data watches for each mode ([`CHCP65::GetModeBreakpoints`], [`CHCP65::GetModeDataWatches`]), and
  user mode's for one process ([`CHCP65::GetProcessBreakpoints`]): kernel and user mode see different memory at the
  same address.
- A debug port ([`CHCP65DebugPort`]): in kernel mode, `WDM` with the signature `HCP65_DEBUG_WDM_SIGNATURE` hands it
  the message at X:A ([`DebugLib/HCP65DebugMessage.h`]): a process started or ended, a driver loaded, a bank given
  to a process.  A real W65C816 runs `WDM` as a two byte no-op, so the kernal runs the same on the board.
- Fill mode ([`CHCP65::SetMemoryFill`]): MPU RAM and main memory read `$CC` until written, and each bank the kernal
  gives a process is filled again, so a program that reads memory it never wrote doesn't see a lucky zero.

## Decisions

- The memory map, registers and decode are what the KiCad netlists say (2026-10-31); where a sheet and the user's
  intent differ, the user's ruling, recorded in the board's faults file.
- Kernel mode is a non-zero interrupt depth, forced until the Native Latch (the user, 2026-10-31; the Kernal Mode
  sheet's gates are wrong).  The vector's first byte is read in the interrupted mode and every process's bank 0 has
  the kernal's vectors (the user, 2026-11-20).
- The schematic corrections of 2026-10-31 above (the user): the `~Main` labels, the depth counter's labels (not
  reversed), the channel's bit pairs, the Sound Board's DAC write moved to Y5 with A0 kept on Q2.
- The SD cards run in 4 bit mode, in four sockets; never "simplified" to SPI or 1 bit (the user, 2026-11-05).
- Contention is meant to shut main memory off rather than be arbitrated, and does (the user, 2026-11-05); the
  processor must stay off main memory during a DMA.
- The MPU clock is 25MHz (an LTC6903), and the emulator runs at it, though the small mainboard being built runs at
  about 12.5MHz; keep 25MHz for now (the user, 2026-10-31 and 2026-11-05).
- The timer's clock is a 32.768kHz oscillator (the user, 2026-10-31).
- `TestHCP650.asm` was obsolete, and the `startup.asm` once quoted beside the compiler was another W65C816 machine
  (Andre's SFT816), not the HCP65 (the user, 2026-10-31); neither is in the repositories now.

## Not done

- The Full Mainboard: three processors, and sockets for the second and third.  Only processor 0 is emulated.
- The PCBs carry none of the 2026-10-31 schematic corrections.  The Main Memory Channel's PCB had old tracks left
  on IC2's pads 12 to 19 after the user's reroute (DRC shorts), reported on 2026-10-31.
- The faults found in the faults files are the boards' to fix; the emulator models none of them.  For the SD Card
  Board, as listed on 2026-11-05 (the file has their state): Fast CLK undriven, the Read Function counters held in
  reset, the DAT-zero inverter's polarity, `~Main RD` and `~Main WD` crossed, 5V into the cards; and likely timing
  problems (nybble pairs not resynchronised to the start bit, extra SD clocks in a write, no high speed mode).
- SD high speed mode (CMD6), multiple block writes, and card detect.  The kernal's driver and the board run at
  default speed.
- The emulator holds NMI low directly for contention, so NMI State shows no source for it.  On the board Main
  Memory Small Enable raises NMI1 (the user, 2026-11-05); whether NMI State shows that as a break input isn't
  checked.
- The MPU ROM is flash that can be reprogrammed in place ([Memory Map](../memory-map.md#kernel-mode)); the emulator
  ignores writes to it, so its command sequences aren't modelled.
- The device clock's holds (VIA Control's, and the other boards' Clock Hold Standard) aren't emulated: a device
  access takes the processor's cycles and no more.
- Board State's presence bits beyond S0 (the timer) and S4 (the kernel mode board) aren't documented.
