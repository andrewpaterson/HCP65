# Devices and Main Memory

## Device sockets

Devices are boards plugged into the mainboard's device sockets.  In
[kernel mode](memory-map.md#kernel-mode) each socket has its own slot of the address space
(*HCP65 MPU Address Decode* sheet).  User mode cannot reach the devices at all.

| Device | Address | Size |
|---|---|---|
| 0 | `$100000`–`$107FFF` | 32KB: the processor board's [on-board registers](registers.md) |
| 1 | `$108000`–`$10FFFF` | 32KB |
| 2 | `$110000`–`$117FFF` | 32KB |
| 3 | `$118000`–`$11FFFF` | 32KB |
| 4 | `$120000`–`$127FFF` | 32KB |
| 5 | `$128000`–`$12FFFF` | 32KB |
| 6 | `$130000`–`$137FFF` | 32KB |
| 7 | `$138000`–`$13FFFF` | 32KB |
| 16 | `$180000`–`$19FFFF` | 128KB |
| 17 | `$1A0000`–`$1BFFFF` | 128KB |
| 18 | `$1C0000`–`$1DFFFF` | 128KB |
| 19 | `$1E0000`–`$1FFFFF` | 128KB |

Devices 8–15 do not exist.

Each device also has a reset line and an interrupt line, controlled and read through the on-board
[Device Reset/Enable](registers.md#3-and-5-device-resetenable) and
[Device Interrupt](registers.md#0-and-2-device-interrupt) registers.  Every device is held in reset
until the kernel enables it.

On the small mainboard, device 1 is the [main memory channel](#main-memory) and device 7 is the
processor reset device.  The other device boards (VIA, UART, PS/2, SD card, SNES controller, LCD,
sound, video, graphics, blitter, coprocessor) can go in any socket.

## Inside a device's slot

Most device boards use the standard two stage device decoder (*Device Decode stage A* and
*Device Decode stage B* sheets).  It divides the device's slot like this:

| Offset | Size | What |
|---|---|---|
| `$00000`–`$003FF` | 1KB | [Device ROM](#device-rom): what the board is and what it needs |
| `$00400`–`$007FF` | 1KB | Device registers |
| `$00800`–end of slot | Up to ~126KB | Device RAM, if the device has any |

A 32KB slot ends at `$07FFF` and a 128KB slot at `$1FFFF`.

The device registers are decoded from A0–A4: A0–A2 pick one of 8 registers, and A3–A4 pick one of
up to four groups of 8, so a device can have up to 32 registers.  They repeat through the 1KB.  Each
register can have separate read and write meanings.

## Device ROM

The first 1KB of every device's slot is its ROM, which describes the board.  The kernel reads each
socket's ROM at start up, while the devices are still held in reset.  The device bus is pulled high,
so an empty socket reads `$FF` throughout.

Words are little endian.

| Offset | Size | Field |
|---|---|---|
| `$000` | 4 | Magic: `$C0DAFE1A` (the bytes `1A FE DA C0`) |
| `$004` | 1 | Format: `1` for this layout |
| `$005` | 1 | Board revision |
| `$006` | 1 | Device bus: `0` needs a 32KB socket, `1` needs a 128KB socket (16–19) |
| `$007` | 1 | Main bus: bit 0 can drive the main bus (DMA); bit 1 provides a 2MB memory window from chip A; bit 2 from chip B; bit 3 the windows are private, the board's own memory; `0` doesn't use the main bus |
| `$008` | 8 | Reserved (`$FF`) |
| `$010` | 64 | Device name |
| `$050` | 64 | Manufacturer |
| `$090` | 32 | Driver file, in `\OS\Drivers\` on the SD card; empty if the board has none |
| `$0B0` | 2 | Number of registers listed |
| `$0B2` | 3 each | Registers: the offset in the slot (a word), then `0` read or `1` write |
| … | | Unused (`$FF`) |
| `$3FF` | 1 | Checksum: the 1KB's bytes sum to 0 (mod 256) |

The three strings are zero terminated and zero padded, so the last byte of each is always 0.

The register list has one entry per register and direction.  A register that is both read and
written is listed twice.  Entries are sorted by offset, with the read before the write.  There is
room for 281 entries.

The kernel finds a board by its manufacturer and device name.  A board is ignored, and left in reset,
in any of these cases:
- its magic, format or checksum is wrong, or a string isn't terminated;
- it needs a 128KB socket but is in a 32KB one.

A board's ROM image is made by the device ROM tool, `DeviceROM <board.xml> [<image.bin>]`, from an XML
description of the board (the HCP65's boards' descriptions are in `DeviceROM/Boards`):

```xml
<?xml version="1.0" encoding="utf-8"?>
<DeviceROM Name="SD Card Board" Manufacturer="HCP65" Revision="0" DeviceBus="32KB" MainBus="Master" Driver="SDCARD.DRV">
	<Register Name="DMA Address Low" Offset="0x400" Access="Write"/>
	<Register Name="Flags" Offset="0x40C" Access="ReadWrite"/>
</DeviceROM>
```

`DeviceBus` is `32KB` or `128KB`.  `MainBus` lists any of `Master`, `WindowA`, `WindowB` and `Private`;
leave it out if the board doesn't use the main bus.  `Private` says the windows are the board's own
memory, such as a coprocessor's, put on the main bus only to load and collect it: the kernel gives the
board a window, but its memory isn't main memory.  `Driver` and `Revision` (0) can be left out.  `Access` is
`Read`, `Write` or `ReadWrite`.  `Count` lists that many registers one after another from `Offset`.  A
register's `Name` is only for the reader.

The tool can also work the description out from the board's KiCad schematic:
`DeviceROM <board.kicad_sch> [<board.xml> [<image.bin>]]` writes the description (as `<board>.xml` in
the current directory unless it is named) and makes the image from it.  From the schematic:

- The registers come from each *Device Decode Stage B*.  What its ~E2 and E3 are wired to (A3, A4,
  ~A3, ~A4 or a supply) gives the group of eight it decodes, from `$400 + 8 × group`.  Each of its
  Y*n*R and Y*n*W outputs that is wired to something is a read or write register at *n*, named by the
  net it drives (`Write Flags` is `Flags`).
- The board needs a 128KB socket if A16 goes anywhere but *Device Decode Stage A* and the *Device
  Buffer*.
- It drives the main bus if `~Main Access` goes past the connector, and provides memory window A or B
  if `~Memory A` or `~Memory B` does.
- The name and revision are the title block's title and revision (`V0` is 0).  The manufacturer and
  the driver file are properties of the device ROM chip (the chip on *Device Decode Stage A*'s
  ~Device ROM): `ROM_Manufacturer` and `ROM_Driver`.  `ROM_Name` and `ROM_Revision` override the
  title block.  `ROM_MainBus`'s words are added to what the wiring shows: the Coprocessor Board's is
  `Private`.  Wiring that leaves through a connector isn't followed, so the Video Board, whose memory
  is on its *Video Board - Memory* board, gives `WindowA WindowB Private`.

A board whose registers aren't decoded by a *Device Decode Stage B* needs its description written by
hand: the UART Board's two UARTs are selected by gates on its *UART Signals* board
(`DeviceROM/Boards/UARTBoard.xml`), and the VIA Board's eight VIAs by its *VIA Control* board
(`DeviceROM/Boards/VIABoard.xml`).

`DeviceROM/MakeImages.ps1` makes every board's image into `DeviceROM/Images/`: from its description
in `DeviceROM/Boards/` if it has one, else from its schematic.  A programmer burns an image into the
board's SST39VF010 at address 0, the rest of the chip left erased (`$FF`).

The HCP65's own boards have the manufacturer `HCP65`:

| Device name | Socket | Main bus | Driver file |
|---|---|---|---|
| `SD Card Board` | 32KB | Drives it (DMA) | `SDCARD.DRV` |
| `Sound Board` | 128KB | Doesn't use it | `SOUND.DRV` |
| `UART Board` | 32KB | Doesn't use it | `UART.DRV` |
| `Memory Board` | 32KB | Windows A and B: it is 4MB of main memory | none: the kernel sets its window |
| `Coprocessor Board` | 32KB | Windows A and B, private: the coprocessors' 4MB | `COPROC.DRV` |
| `Video Board` | 128KB | Windows A and B, private: its video memory | `VIDEO.DRV` |
| `Debug Port` (the emulator's) | 32KB | Doesn't use it | none: the kernel drives it itself |

### Drivers

The kernel has the SD Card Board's and the debug port's drivers in its ROM: it needs the SD card to
load anything else.  Once it has mounted the SD card, it loads the driver of each other board from
`\OS\Drivers\`, the file its device ROM names (the sound board's is `SOUND.DRV`).

A driver is a relocatable W65C816 module, built from its project in `HCP65/Drivers`.  The kernel
puts it in free MPU RAM after its own code and data, within one bank, and fills in the addresses
in it.  A driver calls the kernel by name: the kernel's *exports* (`Kernal/KernalExports.asm`) are
the names a driver may use, and a driver that uses any other name isn't loaded.  The kernel then
runs the driver's constructors and calls its `KernalCreateDriver(index)`, which returns the driver
(a `CKernalDriver`) the kernel starts for the board.  A driver file is loaded once however many
boards use it: each board gets the next index (the sound board's driver drives up to four, the UART
Board's up to two).  The ROM drives up to two SD Card Boards.

On the emulator, the kernel tells the debug port of each driver it loads (its file, where it is and
its first board's socket) after filling in its addresses and before any of it runs.  While a
debugger watches, that stops the run, so breakpoints can be bound in the driver first.

### Using a board from a program

A program uses a board through its driver with the device calls (`HCP65User.h`):

- `KernalDeviceInfo(index, &info)` lists the boards in socket order: their names, revisions, how
  many units their drivers have and whether a program has them open.
- `KernalDeviceOpen(manufacturer, name, index, unit, &handle)` opens a unit of the index'th board
  of that kind.  A unit is the program's until it closes its last handle on it, or exits.  Most
  drivers give the program the whole board while it has any unit open, so no other program can
  open one; a driver whose units are separate gives each unit its own program (the UART Board's two
  ports).
- `KernalDeviceRead`, `KernalDeviceWrite`, `KernalDeviceControl` and `KernalDeviceWait` take the
  handle.  Read and write say how many bytes they moved: a unit that is a stream (a UART port) reads
  what has arrived, which may be nothing.  What a unit, an offset, a control code or a wait value
  means is the driver's: `HCP65SoundBoard.h` has the sound board's and `HCP65UARTBoard.h` the UART
  Board's.
- `KernalDeviceClose(handle)`.

They are one kernel call (COP 12) with a block of arguments.

## Main memory

Main memory is a 16MB space.  It is made of 2MB memory chips on memory boards, which plug into device
sockets: two chips per socket, so 4MB per socket.

- In [user mode](memory-map.md#user-mode) every address is main memory, after
  [bank remapping](memory-map.md#bank-remapping).
- In kernel mode main memory is `$200000`–`$FFFFFF`; the first 2MB of main memory is only reachable
  from user mode.

### Main memory channel

Which 4MB of main memory a socket's two chips answer is set by the main memory channel, which is
device 1 (*Main Memory Channel* and *Main Memory Select* sheets).  Each socket gets a 2 bit window
number:

| Window | Main memory |
|---|---|
| 0 | `$000000`–`$3FFFFF` |
| 1 | `$400000`–`$7FFFFF` |
| 2 | `$800000`–`$BFFFFF` |
| 3 | `$C00000`–`$FFFFFF` |

Within the window, the socket's first chip is the lower 2MB and its second chip the upper 2MB.

The window numbers are set by writing two registers at the start of device 1's slot.  These are
written directly, not through the standard device decoder, and they repeat every 8 bytes through
the slot.

| Register | Address | D1–D0 | D3–D2 | D5–D4 | D7–D6 |
|---|---|---|---|---|---|
| 0 | `$108000` | Socket S | Socket T | Socket U (device 19) | Socket V (device 18) |
| 1 | `$108001` | Socket W (device 4) | Socket X (device 3) | Socket Y (device 17) | Socket Z (device 16) |

Sockets S and T are not wired on the small mainboard.  In each pair the lower data bit is the low
bit of the window number, so writing `%01` to a pair selects window 1.  Nothing can be read back,
and nothing resets them: they are latches, so a reset leaves every socket in the window it was in,
and at power on each socket is in any window.  Nothing may use main memory before the kernel has
written both registers.

### How the kernel uses it

The kernel finds the memory boards by their device ROMs (`MainBus` `WindowA` and `WindowB`, the
*Memory Board*, `DeviceROM/Boards/MemoryBoard.xml`), in socket order (devices 3, 4, 16, 17, 18
then 19), and gives the first window 1, the next 2, then 3, then 0: window 0's lower 2MB is only
reachable from user mode.  Private boards (the Coprocessor Board and the Video Board) in the memory
sockets get the windows after the memory boards', in the same order; their memory isn't main
memory, and their drivers find their window in their device's `uiWindow`.  With both, two windows
are left for memory boards; no more than four boards with windows are expected.  Of the banks they make, from `$200000` up, it keeps four for the
[maths tables](runtime/w65c816.md#the-maths-tables) and one for its main memory heap (what the SD Card
Board's DMA must reach), and gives the rest to processes.  With one board the tables are banks
`$40`–`$43`, the heap `$44` and processes `$45`–`$7F`.  Without a memory board the kernel stops
once it has scanned the devices: the SD Card Board has nothing to read into.

The kernel's own heap is MPU RAM's bank 6, `$060000`–`$06FFFF`, below the remap table.  The
drivers it loads go in MPU RAM from the end of its data up to it.

