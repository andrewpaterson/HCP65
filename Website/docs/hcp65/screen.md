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
| `KernalScreenSetColours(palette, record)` | A palette's colours from an SFT palette record (below): its colours at its indices, the rest left as they were. |
| `KernalScreenSetPalette(palette)` | The palette each pixel drawn from now on takes. |
| `KernalScreenAddImage(pixels, width, height, flags, palette, &image)` | A handle to an image in the program's memory: see [Images](#images). |
| `KernalScreenRemoveImage(image, &pixels)` | Takes the handle back, and says where the image is. |

A pixel is a byte.  The colour it shows is that entry of its palette, and there are 128 palettes.

The screen calls are one kernel call (COP 14) with a block of arguments.

`sprites` in the shell (`HCP65/Sprites`) draws with them: a stage (`\SPRITES\STAGE.SFT`, an opaque
640 by 400, made an [image](#images)) and Maki standing on it (`\SPRITES\MAKI.SFT`, a container of
her palette, which goes in palette 1, and her transparent frames: standing and six walking, each
way), with letters blitted from the console's font.  With a Graphics Board it draws her small
above the stage too, facing each way: [scaled and mirrored blits](#scaled-and-mirrored-blits) of an
image of her first frame.  The first SNES
controller of an LCD and SNES Board walks her left and right.  Each frame it draws the stage again
only where she was in the frame being drawn: of each of her rows, from her first pixel to her last
(a blit with `KERNAL_BLIT_SPANS`, below).  Then it draws her and flips the frames at the vertical
blank.  It draws a frame in about 10ms on the emulator's 25MHz processor, so she walks a step (3
pixels) every frame, 60 a second.  The flower at the stage's bottom right
(`\SPRITES\FLOWER.SFT`, cut from the stage by `Images/MakeFlower.py`) is in front of her: it is
drawn again over her while she is across it.  Escape ends it.

## Blits

A blit draws an SFT image from the program's memory with its top left at an x and y, which can be
off the picture.

SFT is the image format CommonLibraries' SupportLib reads and writes (`SFTReader`, `SFTWriter`);
`HCP65SFT.h` has it for a program.  A file is a record: an image, a palette, or a container of
them.  Every record starts with the same four bytes: the magic number `SF`, the layout's version
(`SFT_VERSION`, 1) and its type.  A record of another version isn't read, and isn't drawn.

- **Opaque** images are a width and height, then their rows.
- **Transparent** images are a width and height, then one byte codes: 1 to 127 draws that many
  pixels, which follow; `$80` + *n* - 1 skips *n* pixels across (up to 64); `$C0` + *n* - 1 moves
  *n* rows down (up to 32); `$E0` + *n* - 1 is a mixed run of *n* pixels (up to 32), which follow,
  of which those that are 0 aren't drawn; 0 ends the image.  What isn't drawn stays as it was, so
  they are sprites, pointers and glyphs.  A mixed run is a run with short gaps in it, which a
  blitter draws as one copy that leaves a colour out; the console's font has one for each row of a
  glyph.
- **Palettes** are a first index and a count of colours (1 to 256, none past index 255), a hash,
  then that many reds, greens and blues.  Most palettes are smaller than 256: several small ones
  can share one of the board's palettes at different indices.  The hash is FNV-1a of the first
  index, the count and the colours (`SFTPaletteHash`): palettes with the same hash are very likely
  the same, so images that use them can share one of the board's palettes.
- **Containers** hold many records in one file.  A palette is the palette of the images after it,
  up to the next.  `SFTContainedImage` finds an image by its number (palettes aren't counted) and
  `SFTContainedPalette` its palette.

`PNGToSFT` (CommonLibraries' `Tools/PNGToSFT`) makes a container from pictures on the computer
the HCP65 is programmed from (PNG, BMP and the others SupportLib reads): one palette for all of
them, then each picture as an image.  The palette is the pictures' own colours if there are few
enough, else as many as `--colours` allows (256, or 255 when a picture has transparent pixels),
chosen by median cut: the colours are split at the middle of the channel they spread over most
until there are enough, each palette colour the mean of its share, and each pixel is the nearest.
There is no dithering.  A pixel less than half opaque is transparent, which is index 0, so the
colours are from index 1 then; `--merge` is the widest gap a transparent image's runs are joined
across.

```text
PNGToSFT --merge 8 MAKI.SFT MakiStand.png MakiWalk.png
```

What the blit takes (`SKernalBlit`):

| Field | What |
|---|---|
| `uiImage`, `uiSize` | The image and how many bytes it is.  No more than that is read. |
| `iX`, `iY` | Where its top left goes.  What is off the picture isn't drawn. |
| `uiFlags` | `KERNAL_BLIT_CLIP` draws only inside the clip rectangle; `KERNAL_BLIT_SPANS` as well draws an opaque image only inside a span of each of its rows. |
| `iClipLeft` to `iClipBottom` | The clip rectangle, up to but not including its right and bottom. |
| `uiSpans` | With `KERNAL_BLIT_SPANS`, two words for each row of the clip rectangle from its top: the first pixel across it drawn and the one just past the last (the same for none). |

The spans are how a sprite's old place is drawn again from a picture: of each of its rows, only from
its first pixel to its last, one copy a row rather than the sprite's whole rectangle.

An SFT pixel is an index into its palette.  An image with no palette before it has pixels that are
colours, with red in bits 0 to 2, green in bits 3 to 5 and blue in bits 6 and 7: `SFTColours` makes
the palette record that shows them as they are.

What isn't an image, or is cut short, is `KERNAL_DEVICE_BAD_ARGUMENT`.

The Video Board's driver draws a blit as copies in main memory, one for each run of the image's
pixels inside the clip rectangle.  With a Blitter Board in the computer (its driver is
`\OS\Drivers\BLITTER.DRV`) the longer copies are the board's: it copies a byte in two processor
cycles, and leaves a mixed run's transparent pixels out itself, but its registers take time to
write, so the processor makes the copies under 15 pixels (9 of a mixed run).  Without a board the
processor makes them all, and what is drawn is the same.  Then a transparent image that is all on
the picture and in one bank of the program's memory, and spans, are drawn by the driver's
assembly (`VideoBoardBlit.asm`), a run or a row of 8 pixels or more with one `MVN`.

The console's text uses the board the same way: the processor draws a glyph's runs, which are
short (the board would draw one of eight pixels or more), and the board clears a row, or the
screen, a line at a time from a line of zeros the driver keeps in main memory.

## Images

An image is a picture of a width by a height of pixels, a byte each, a row after another, in the
program's own memory.  The program gives the kernel a handle to it (`KernalScreenAddImage`) and
blits it by the handle: `KERNAL_BLIT_IMAGE` in the blit's flags, the handle in `uiImage`.  Its rows
are flat, which is what the boards copy from: a row inside the clip rectangle is one copy.  Clipping
and spans are as for an SFT image.

`SFTImageMake` (`HCP65SFT.h`) makes one from an SFT image: it allocates the image's bytes, unpacks
the SFT into them and gives the kernel the handle.  The program can free the SFT then, and blits
from the image from then on.  `SFTImageFree` takes the handle back and frees the image.

| Flag | What |
|---|---|
| `KERNAL_IMAGE_TRANSPARENT` | Its pixels that are 0 aren't drawn (an SFT's transparent image's undrawn pixels are 0). |

An image has the palette its pixels are drawn with, or `KERNAL_SCREEN_NO_PALETTE` for the one set
by `KernalScreenSetPalette`, which a blit of an image with its own palette puts back after it.

The image stays the program's memory: the kernel only keeps where it is, its size, its flags and its
palette.  It is found again at each blit, so its rows can cross from one of the program's banks to
the next, which needn't be next to it in main memory: such a row is a copy from each.  If the
program frees or changes the memory while it has the handle, what is drawn is whatever is there; if
the program no longer has the bank, the blit is `KERNAL_DEVICE_BAD_ARGUMENT`.

There are `KERNAL_SCREEN_MAX_IMAGES` (64) handles for all the programs together; the next is
`KERNAL_DEVICE_NO_HANDLES`.  A program can have images without having the picture, and its handles
go when it ends.  A handle that has been taken back, or is another program's, is
`KERNAL_DEVICE_BAD_HANDLE`: each handle is the entry's number and a count of the times it has been
used, so an old handle doesn't reach a newer image.

Without a Blitter Board a transparent image is slower than the same picture as an SFT: each of its
rows is a copy that looks at every byte, where the SFT's runs copy only what is drawn.  On the
emulator Maki's 84 by 182 frame takes about 455,000 cycles as an image and 144,000 as an SFT, so
`sprites` draws her frames as SFTs.  With a Blitter Board a row of an image is one of the board's
copies, where an SFT's row is one for each of its runs.  An opaque picture costs the same either
way.

## Scaled and mirrored blits

An image can be drawn at another size: `KERNAL_BLIT_SCALED` in the blit's flags, and the size in
`uiWidth` and `uiHeight` (1 to 32767 each).  Each pixel drawn is the image's nearest one: pixel *d*
of *n* drawn from *s* is (2*d* + 1) × *s* / 2*n*, rounded down, across and down.  So twice the size
draws each pixel twice, half the size every other one.  `KERNAL_BLIT_MIRROR_X` draws the image right
to left and `KERNAL_BLIT_MIRROR_Y` bottom to top, scaled or not.  Clipping is as for any blit;
spans aren't used.  A transparent image's 0s aren't drawn.

They are the [Graphics Board](#the-graphics-board)'s: without one (or its driver) the blit is
`KERNAL_DEVICE_NOT_FOUND`.  Each drawn row inside the clip rectangle is one copy, from the image row
nearest it, through the board's offsets for the drawn width.  The driver writes the offsets only
when the image's width, the drawn width or the mirroring changes, so a sprite drawn many times at
one size costs the table once.  A row of the image that crosses from one of the program's banks to
the next is two copies.  An SFT can't be scaled (`KERNAL_DEVICE_BAD_ARGUMENT`): make it an
[image](#images) first.

On the emulator a scaled blit costs about 9,000 processor cycles, the kernel call itself included,
and about 1,000 more for each row drawn, plus the board's two cycles for each pixel (the processor
waits for it).  A row that crosses a bank costs about 3,000.

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

The board's driver, `GRAPHICS.DRV`, enables it for the kernel, and the Video Board's driver draws
[scaled and mirrored blits](#scaled-and-mirrored-blits) with it.

As the Blitter Board's, the emulator's Graphics Board is the board as it is meant to work.
