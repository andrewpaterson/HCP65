# Starting Up

The HCP65 starts in two stages, so that the processor board's flash ROM rarely needs
reprogramming.  The ROM holds only a **boot loader**.  Everything that is likely to change (the
kernel, its drivers and the programs it runs) is on the SD card.

```text
ROM boot loader  ->  \OS\OS.SYS  ->  the start up program  (\OS\STARTUP.TXT names it)
```

## The boot loader

The boot loader is the MPU ROM (`HCP65/Boot`).  It is linked to run from MPU RAM banks 4 and 5,
out of the kernel's way, and the ROM image has it at the same offsets.

1. At reset the processor is in emulation mode with the ROM at `$000000`
   ([RAM and ROM swap at boot](memory-map.md#ram-and-rom-swap-at-boot)).  The reset vector's stub
   jumps to the boot loader in ROM, which goes native, swapping RAM and ROM.  It then copies ROM
   banks 4 and 5 to MPU RAM banks 4 and 5, and the page of stubs and vectors at `$FF00` to
   `$00FF00`, and goes on in RAM.
2. It brings up the machine as the kernel does: it scans the device sockets, gives the memory boards
   their [main memory windows](devices.md#main-memory), starts the NMI timer and the SD Card Board's
   driver, and mounts the first card with a FAT volume.
3. It reads `\OS\OS.SYS`: the first 256 bytes into a buffer, and the rest straight to MPU RAM from
   `$010000`.  The boot loader isn't there, so nothing needs moving.
4. It puts every board back in reset, disables interrupts and turns the NMI timer's NMI off.  Then
   it copies the first 256 bytes to `$00FF00`, the kernel's stubs and vectors, and jumps to
   `$00FF00`.

If there is no main memory, SD Card Board, card or volume, or `OS.SYS` is missing or isn't an OS,
the boot loader stops and idles, and its status says why.

## OS.SYS

`\OS\OS.SYS` is the kernel (`HCP65/Kernal`).  It is the bytes for `$00FF00` to `$03FFFF`:

| In the file | In MPU RAM | What |
|---|---|---|
| `$00000`–`$000FF` | `$00FF00`–`$00FFFF` | The stubs and native vectors.  The first byte is the entry, a `jml` to the kernel's start up. |
| `$00100`–`$300FF` | `$010000`–`$03FFFF` | The kernel's code and data. |

A file shorter than this holds only what it has.  The kernel's bss follows its data, and may go up
to the remap table.

The kernel starts from the beginning again: it clears the remap table's pages for processes `$FC`
to `$FF`, its bss and its heap, then does what the boot loader did, so that it never depends on what
the boot loader left.  Then it loads each board's [driver](devices.md#drivers) from `\OS\Drivers\`
and runs the start up program.

## STARTUP.TXT

`\OS\STARTUP.TXT`'s first line names the start up program and, after spaces, its argument as a
decimal number (what `KernalGetArgument` gives it, 0 if there isn't one):

```text
\boottest.exe 1
```

Without the file the kernel runs `\OS\INIT.EXE` with argument 0.  The program runs as process
`$FF` in user mode.

## Memory while starting

| MPU RAM | The boot loader running | The kernel running |
|---|---|---|
| `$000000`–`$00BFFF` | Stack and direct page | Stack and direct page |
| `$00C000` | The status | The status |
| `$00FF00`–`$00FFFF` | The boot loader's stubs and vectors | The kernel's |
| Banks 1–3 | `OS.SYS` is read into them | The kernel |
| Banks 4–5 | The boot loader | Drivers, after the kernel's bss |
| Bank 6 | The heap | The kernel's heap |
| Bank 7 | | The remap table |

## The status

Both stages keep what they found in one block at `$00C000` (`SKernalStatus` in
`Kernal/Kernal.h`), which tests and debuggers read.  Each clears it as it starts, so a reader
needn't know which stage is running.  Its state is one of:

| State | Meaning |
|---|---|
| 1 | Starting |
| 2 | The devices are scanned |
| 3 | There is no SD Card Board |
| 4 | No card came up |
| 5 | No card's volume mounts |
| 6 | The volume is mounted |
| 7 | The start up program couldn't be run |
| 8 | The start up program ran and exited |
| 9 | No memory board gave enough main memory |
| 10 | The boot loader found no usable `OS.SYS`: the boot result says why (1 not found, 2 the wrong size, 3 not read, 4 no entry) |
| 11 | The boot loader has jumped to `OS.SYS` |

States 3 to 6, 9 and 10 can come from either stage, and 7 and 8 only from the kernel.
