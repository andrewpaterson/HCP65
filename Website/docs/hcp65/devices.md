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
| `$007` | 1 | Main bus: bit 0 can drive the main bus (DMA); bit 1 provides a 2MB memory window from chip A; bit 2 from chip B; `0` doesn't use the main bus |
| `$008` | 8 | Reserved (`$FF`) |
| `$010` | 64 | Device name |
| `$050` | 64 | Manufacturer |
| `$090` | 32 | Driver file, in `\DRIVERS\` on the SD card; empty if the board has none |
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

`DeviceBus` is `32KB` or `128KB`.  `MainBus` lists any of `Master`, `WindowA` and `WindowB`; leave it out
if the board doesn't use the main bus.  `Driver` and `Revision` (0) can be left out.  `Access` is
`Read`, `Write` or `ReadWrite`.  `Count` lists that many registers one after another from `Offset`.  A
register's `Name` is only for the reader.

The HCP65's own boards have the manufacturer `HCP65`:

| Device name | Socket | Main bus | Driver file |
|---|---|---|---|
| `SD Card Board` | 32KB | Drives it (DMA) | `SDCARD.DRV` |
| `Sound Board` | 128KB | Doesn't use it | `SOUND.DRV` |
| `Debug Port` (the emulator's) | 32KB | Doesn't use it | none: the kernel drives it itself |

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
bit of the window number, so writing `%01` to a pair selects window 1.

