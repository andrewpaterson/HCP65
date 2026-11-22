# Sprites

A sprite is a picture that moves over another: a character walking across a stage, a pointer, a
letter of text.  The HCP65 has no sprite hardware.  A program draws its sprites into the frame being
drawn with [the screen's calls](screen.md), then shows that frame, and the kernel has the copying
done by a Blitter Board or a Graphics Board when there is one.  This page is how to do it: making the
pictures, loading them, their palettes, the two ways to draw them, drawing each frame and scaling.
`HCP65/Sprites` (`sprites` in the shell) does all of it.

## Pictures

Sprites are SFT images ([Blits](screen.md#blits) has the format): a header, a width and a height,
then the pixels.  An opaque image is its rows; a transparent one is runs, codes that say where its
pixels are, so what isn't drawn takes no room and no time.  A container holds many records in one
file, such as a character's frames and their palette.

A pixel is a byte: an index into one of the Video Board's 128 palettes of 256 colours.  The picture
on the computer the HCP65 is programmed from has 16 million colours, so making an SFT from it is
choosing a palette and putting each pixel at the nearest colour in it.  `PNGToSFT`
(CommonLibraries' `Tools/PNGToSFT`) does that:

```text
PNGToSFT --merge 8 MAKI.SFT MakiStand.png MakiWalk.png
```

- All the pictures given share one palette, which is the container's first record, then each
  picture is an image, in the order given.
- The palette is the pictures' own colours when there are few enough: none is changed.  Otherwise
  it is as many as `--colours` allows, chosen by median cut, with no dithering: dithering a sprite
  makes it shimmer as it moves.
- A pixel less than half opaque is transparent.  Transparent is index 0, so the colours start at 1
  and there are 255 of them; a picture with no transparent pixel can have 256.
- A picture with a transparent pixel is a transparent image.  `--merge` is the widest gap its runs
  are joined across: a mixed run is one copy for a board that leaves the gaps out, and fewer, longer
  copies are quicker when a board makes them.

A palette takes only as many colours as it needs.  Several small palettes can share one of the
board's: one sprite's at indices 1 to 16, another's at 17 to 32, both in palette 5.

## Loading

A program reads the file with the [file calls](kernel-calls.md#files) and finds what is in it with
`HCP65SFT.h`:

```c++
uint32  uiSize;
uint8*  puiMaki;

uiSize = KernalFileSize("\\SPRITES\\MAKI.SFT");
puiMaki = (uint8*)malloc(uiSize);
KernalFileLoad("\\SPRITES\\MAKI.SFT", puiMaki, uiSize);

frame = SFTContainedImage(puiMaki, uiSize, 3, &frameSize);    // The fourth image: palettes aren't counted.
palette = SFTContainedPalette(puiMaki, uiSize, 3);             // The palette before it, NULL if none.
```

An image with no palette before it has pixels that are colours themselves, three bits of red, three
of green and two of blue; `SFTColours` makes the palette that shows them as they are.

## Palettes

`KernalScreenSetColours(palette, record)` loads a palette record's colours into one of the board's
palettes, at the record's own indices, and leaves the other entries as they were.  Every pixel in a
frame has its palette's number beside its index, so pictures in different palettes show side by side.
An SFT blit's pixels take the palette set with `KernalScreenSetPalette`.  An image made with a
palette is drawn in it whatever is set, and the one set is put back after.

A palette record has a hash of its colours and where they go (`SFTPaletteHash`).  Two records with
the same hash are almost certainly the same, so a program loading many files can load a palette into
the board once and use it for all of them.

## Two ways to draw

A sprite can be drawn from its SFT, or from an image: its flat pixels, a row after another.

| | SFT blit | Image blit |
|---|---|---|
| What is drawn from | The SFT in the program's memory | Flat pixels in the program's memory, which the kernel has a handle to |
| A transparent sprite, no Blitter Board | Only its runs are copied: quicker | Every byte of every row is looked at |
| A transparent sprite, with a Blitter Board | A copy for each run | A copy for each row |
| An opaque picture | The same either way | The same either way |
| Scaled or mirrored | No | Yes, with a Graphics Board |
| Its palette | The one set | Its own, or the one set if it was made without one |

On the emulator, Maki's 84 by 182 transparent frame without a Blitter Board takes about 144,000
processor cycles as an SFT blit and 455,000 as an image blit.  So `sprites` draws her frames from her
SFT, and makes images of the stage (an opaque 640 by 400, drawn again under her every frame), the
flower and the picture it scales.

### Images

`SFTImageMake(sft, size, palette, &image)` makes an image of an SFT image: it allocates the image's
width times height bytes with `malloc`, unpacks the SFT into them and gives the kernel a handle to
them, to be drawn with `palette` (`KERNAL_SCREEN_NO_PALETTE` for the one set).  The SFT can be freed
then.  `SFTImageFree(image)` takes the handle back and frees the pixels.

```c++
uint16  uiStage;

SFTImageMake(puiStageSft, uiStageSize, 0, &uiStage);
free(puiStageSft);
```

An image is the program's memory, so it counts against the program's heap and goes when the
program ends; the kernel keeps only where it is.  There are 64 handles for every program together.
[Images](screen.md#images) has the rest: what is refused, and why an image whose memory was freed
draws whatever is there.

## Drawing a frame

The picture is two frames.  The program draws into the one that isn't shown, then
`KernalScreenFlip` shows it at the next vertical blank and the other becomes the one drawn into.
`KernalScreenWaitBlank` waits for that blank, so a program that draws a frame each time it returns
draws as many frames as the screen shows, 60 a second, if it can draw one in time.

A sprite has to be taken away from where it was before it is drawn where it is.  The frame being
drawn holds what was drawn into it two frames ago, not the last frame, so a program remembers where
it drew the sprite in each frame: `sprites` keeps one place for each, and draws the stage again over
the place the frame being drawn had.  Each frame:

1. Draw the background again where the sprite was in this frame.
2. Draw the sprite where it is now, and anything in front of it (`sprites`' flower).
3. Remember where it was drawn in this frame.
4. `KernalScreenFlip()`, `KernalScreenWaitBlank(ticks)`.

Drawing the whole background each frame is simplest but slowest: `sprites`' stage takes about
3,400,000 cycles, about 140ms.  Drawing only the sprite's rectangle again is far less, and
only where its pixels were is less still: that is what spans are for.

### Spans

A blit with `KERNAL_BLIT_CLIP` and `KERNAL_BLIT_SPANS` draws an opaque image only inside a span of
each row of its clip rectangle: from the first pixel across the rectangle to just before the one
given ([Blits](screen.md#blits)).  So the background goes back only where the sprite's frame had
pixels, one copy a row.  `sprites` works out each of Maki's frames' spans as it loads them, from her
SFT's runs: for each row, its first drawn pixel and the one after its last.  Then each frame:

```c++
SKernalBlit  sBlit;

memset(&sBlit, 0, sizeof(SKernalBlit));
sBlit.uiImage = uiStage;                       // The stage's image.
sBlit.iY = 40;                                 // Where the stage is.
sBlit.uiFlags = KERNAL_BLIT_IMAGE | KERNAL_BLIT_CLIP | KERNAL_BLIT_SPANS;
sBlit.iClipLeft = iWasX;                       // Her rectangle in this frame.
sBlit.iClipTop = iWasY;
sBlit.iClipRight = iWasX + 84;
sBlit.iClipBottom = iWasY + 182;
sBlit.uiSpans = (uint32)puiSpansOfHerFrame;    // Two words a row of the rectangle.
KernalScreenBlit(&sBlit);
```

The stage's rows inside her rectangle take about 99,000 cycles that way, and her frame 144,000: a
frame in about 10ms, so she walks 60 steps a second.

### Clipping

A blit draws nothing off the picture, so a sprite can be partly or wholly off any edge.
`KERNAL_BLIT_CLIP` draws only inside a rectangle as well, for a sprite that mustn't go over a panel,
or (with spans) to put back part of a background.

## Scaling and mirroring

An image can be drawn at another size with `KERNAL_BLIT_SCALED`, its size in `uiWidth` and
`uiHeight`, and right to left or bottom to top with `KERNAL_BLIT_MIRROR_X` and
`KERNAL_BLIT_MIRROR_Y` ([Scaled and mirrored blits](screen.md#scaled-and-mirrored-blits)).  Each pixel
drawn is the nearest of the image's, so twice the size is each pixel twice and half is every other
one; there is no smoothing.  A character facing the other way needs no frames of its own:
`sprites` draws Maki small above the stage, facing right and then mirrored left, from one image.

They are the Graphics Board's: it reads each row of the image through a table of offsets, one for
each pixel drawn, which the Video Board's driver writes only when the image's width, the width it
is drawn at or the mirroring changes.  So a sprite drawn many times at one size costs the table once.
A scaled blit costs about 10,000 processor cycles and about 550 more a row, plus the board's two
cycles a pixel.  Without a Graphics Board a scaled or mirrored blit is `KERNAL_DEVICE_NOT_FOUND`: a
program that may run without one draws something else when it gets that.

## The boards

| Board | What it does for sprites |
|---|---|
| None | The processor copies every run or row: a byte in about 12 cycles opaque, 22 transparent. |
| Blitter Board (`BLITTER.DRV`) | Copies of 15 bytes or more (9 transparent), two processor cycles a byte, the processor waiting. |
| Graphics Board (`GRAPHICS.DRV`) | Scaled and mirrored image blits, a copy a row. |

A program doesn't open either board: the kernel uses them for its blits when their drivers are
loaded ([The Blitter Board](screen.md#the-blitter-board), [The Graphics Board](screen.md#the-graphics-board)).
What is drawn is the same with or without them.

## `sprites`

`HCP65/Sprites/Sprites.cpp` puts this together:

- It loads the console's font for its text, an SFT container of 256 glyphs, which it blits as SFTs.
- It makes images of the stage and the flower in palette 0, which it loads with `SFTColours`, and
  frees their SFTs.
- It loads Maki's container, puts her palette in palette 1, and keeps her frames as SFTs, each in
  one bank of its memory so the driver's quickest path draws it; it works out her spans, and makes
  an image of her first frame for the scaled blits.
- It sets 640x480, draws the background into both frames, and then each frame puts the stage back
  where she was, draws her, draws the flower over her when she is across it, and flips.
- The first SNES controller of an LCD and SNES Board walks her; Escape ends it.  Her images and
  memory go when it does.
