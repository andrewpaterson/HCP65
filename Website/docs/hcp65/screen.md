# The Screen

The screen is the picture the computer shows.  It belongs to the kernal, which draws it on the
Video Board.  A program asks the kernal for it with the screen calls in `HCP65User.h`, and doesn't
open the Video Board itself.

## Text and pictures

The screen shows the [console](console.md)'s text until a program sets a mode.

- `KernalScreenSetMode(mode)` makes the picture the program's: 640x480, 800x600 or 1024x768.
- `KernalScreenSetText()` gives it back, and the console's text starts again, blank.
- The picture also goes back to the console when the program ends.

While a program has the picture, what is written to the console isn't shown.  The keyboard still
works.  Only one program has the picture at a time: another gets `KERNAL_DEVICE_IN_USE`, and a call
that needs a mode before one is set gets `KERNAL_DEVICE_NOT_OPEN`.

## Drawing

The picture is two frames: one is shown while the other is drawn.

| Call | What |
|---|---|
| `KernalScreenGetSize(&width, &height)` | The mode's size. |
| `KernalScreenBlit(&blit)` | Draws an image: see [Blits](#blits). |
| `KernalScreenWrite(offset, source, length)` | Pixels into the frame being drawn.  The pixel at (x, y) is at y x width + x. |
| `KernalScreenRead(offset, dest, length)` | Pixels back from it. |
| `KernalScreenFlip()` | Shows the frame that was drawn, at the next vertical blank. |
| `KernalScreenWaitBlank(ticks)` | Waits for the next vertical blank. |
| `KernalScreenSetColours(palette, colours)` | A palette's 256 reds, then greens, then blues. |
| `KernalScreenSetPalette(palette)` | The palette each pixel drawn from now on takes. |

A pixel is a byte.  The colour it shows is that entry of its palette, and there are 128 palettes.

The screen calls are one kernel call (COP 14) with a block of arguments.

`sprites` in the shell (`HCP65/Sprites`) draws with them: a stage (`\SPRITES\STAGE.SFT`, an opaque
640 by 400) and Maki standing on it (`\SPRITES\MAKI.SFT`, a container of transparent frames:
standing and six walking, each way), with letters blitted from the console's font.  The first SNES
controller of an LCD and SNES Board walks her left and right.  Each frame it draws the stage again
only where she was in the frame being drawn (a blit clipped to there), then her, and flips the
frames at the vertical blank.  Escape ends it.

## Blits

A blit draws an SFT image from the program's memory with its top left at an x and y, which can be
off the picture.

SFT is the image format CommonLibraries' SupportLib reads and writes (`SFTReader`, `SFTWriter`);
`HCP65SFT.h` has it for a program.

- **Opaque** images are a width and height, then their rows.
- **Transparent** images are a width and height, then one byte codes: 1 to 127 draws that many
  pixels, which follow; `$80` + *n* - 1 skips *n* pixels across (up to 64); `$C0` + *n* - 1 moves
  *n* rows down (up to 32); `$E0` + *n* - 1 is a mixed run of *n* pixels (up to 32), which follow,
  of which those that are 0 aren't drawn; 0 ends the image.  What isn't drawn stays as it was, so
  they are sprites, pointers and glyphs.  A mixed run is a run with short gaps in it, which a
  blitter draws as one copy that leaves a colour out; the console's font has one for each row of a
  glyph.
- **Containers** hold many images in one file.  `SFTContainedImage` finds one by its number.

What the blit takes (`SKernalBlit`):

| Field | What |
|---|---|
| `uiImage`, `uiSize` | The image and how many bytes it is.  No more than that is read. |
| `iX`, `iY` | Where its top left goes.  What is off the picture isn't drawn. |
| `uiFlags` | `KERNAL_BLIT_CLIP` draws only inside the clip rectangle. |
| `iClipLeft` to `iClipBottom` | The clip rectangle, up to but not including its right and bottom. |

An SFT pixel is a colour with red in bits 0 to 2, green in bits 3 to 5 and blue in bits 6 and 7.
`SFTColours` gives the palette that shows them as they are.

What isn't an image, or is cut short, is `KERNAL_DEVICE_BAD_ARGUMENT`.

The Video Board's driver draws a blit as copies in main memory, one for each run of the image's
pixels inside the clip rectangle.  With a Blitter Board in the computer (its driver is
`\OS\Drivers\BLITTER.DRV`) the longer copies are the board's: it copies a byte in two processor
cycles, and leaves a mixed run's transparent pixels out itself, but its registers take time to
write, so the processor makes the copies under 15 pixels (9 of a mixed run).  Without a board the
processor makes them all, and what is drawn is the same.

The console's text uses the board the same way: the processor draws a glyph's runs, which are
short (the board would draw one of eight pixels or more), and the board clears a row, or the
screen, a line at a time from a line of zeros the driver keeps in main memory.

## The Blitter Board

The board copies bytes from one place in main memory to another, as the main bus's master: a read
then a write for each, two processor cycles a byte.  The processor mustn't touch main memory until
the copy is done.  Its registers, from `$400` in its slot:

| Offset | Write | Read |
|---|---|---|
| 0 | Flags: bit 0 transparency, bit 6 enable, bit 7 interrupt enable | Flags: bits 0 to 5, bit 6 enable, bit 7 interrupt pending |
| 1 | Clear interrupt | |
| 2 | Transparent colour | Transparent colour |
| 4, 5, 6 | Destination address: low, high, bank | |
| 7 | Start | |
| 8, 9, 10 | Source address: low, high, bank | |
| 12, 13 | Count start: low, high | The count: low, high (the high byte is kept as the low is read) |
| 14, 15 | Count stop: low, high | |

Start loads the count with its start and copies bytes, stepping the count and both addresses, until
the count is its stop: stop less start bytes.  The addresses are left just past the last byte.  With
transparency on, a byte that is the transparent colour is read but not written.  The bytes go in
order, lowest first.

The emulator's board is the board as it is meant to work; the schematic as drawn has faults.

## The Graphics Board

The Graphics Board is a Blitter Board whose source bytes needn't be in order, for drawing a sprite
stretched, shrunk or backwards.  The destination steps a byte at a time as the Blitter Board's does,
but each byte is read from the source address plus an offset, which the board looks up by the count
in a table the processor has written.  The offsets 0, 0, 1, 1, 2, 2... draw a row twice as wide;
0, 2, 4... half as wide; 7, 6, 5... backwards.  A table is for a scale, not for a sprite: the source
address is set to each row's first byte, and as the count has a start, several tables can be kept
at once.

Its registers are the Blitter Board's, at the same offsets, with these differences:

- The source address (8, 9, 10) can be read as well as written.  A copy doesn't change it, and a
  reset doesn't clear it.
- The count is 15 bits: bit 15 of its start and stop is ignored, and it wraps from 32767 to 0.

The offsets are 16 bits and unsigned, so a byte is read from at most 65535 past the source address.
They are device RAM: the low byte of the offset for count *n* is at `$08000` + *n* in the board's
slot and its high byte at `$10000` + *n*.  The processor has them while no copy is under way.

The board's driver, `GRAPHICS.DRV`, enables it for the kernel.  Nothing draws with it yet.

As the Blitter Board's, the emulator's Graphics Board is the board as it is meant to work.
