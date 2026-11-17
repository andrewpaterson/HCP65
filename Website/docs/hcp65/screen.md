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

## Blits

A blit draws an SFT image from the program's memory with its top left at an x and y, which can be
off the picture.

SFT is the image format CommonLibraries' SupportLib reads and writes (`SFTReader`, `SFTWriter`);
`HCP65SFT.h` has it for a program.

- **Opaque** images are a width and height, then their rows.
- **Transparent** images are a width and height, then one byte codes: 1 to 127 draws that many
  pixels, which follow; `$80` + *n* - 1 skips *n* pixels across; `$C0` + *n* - 1 moves *n* rows
  down; 0 ends the image.  What isn't drawn stays as it was, so they are sprites, pointers and
  glyphs.
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

The Video Board's driver draws the blit in software.  The screen call is where a Blitter Board's
driver will be given it instead.
