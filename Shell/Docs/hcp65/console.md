# The Console and the Shell

Once the kernel has started ([Starting Up](boot.md)), a card without `\OS\STARTUP.TXT` runs
`\OS\INIT.EXE`.  It loads the maths tables, then runs the shell, `\OS\SHELL.EXE`, and runs it again
whenever it exits, so there is always a command line.

## The console

Every program has two standard handles, which it doesn't open and can't close:

| Handle | What |
|---|---|
| 0 | The console's input: what is typed |
| 1 | The console's output: text |

They are read and written with the same device calls as any other handle (`HCP65User.h`).  Handles a
program opens start at 2.

The kernel picks the console's boards the first time a program uses one of the handles:

| | First choice | Otherwise |
|---|---|---|
| Output | The Video Board: 80 by 30 text in 640x480 | The UART Board's TTL port, to a terminal |
| Input | A keyboard in the PS2 Board's port 1 | The UART Board's TTL port |

With neither, output is dropped and nothing is ever typed.

**Output** is text.  `\n` starts the next line, `\r` goes back to its start, `\b` back a
character, `\t` on to the next multiple of 8, and `\f` clears the screen.  A terminal on the UART
is sent `\r\n` for each `\n`.

**Input** isn't echoed, and a read doesn't wait: waiting on handle 0 for
`KERNAL_CONSOLE_WAIT_INPUT` does.  The keyboard types what a terminal would send: Enter is `\r`,
Backspace 8, Ctrl with a letter its control character, and the arrows, Home, End and Delete their
VT100 escape sequences.  Shift and Caps Lock work as on a PC, US layout, and Caps Lock's light
follows it.

Controls on handle 1 give the console's columns and rows, clear it, change the Video Board's mode
(0 80x30, 1 100x37, 2 128x48) and set the colour of what is written next: a foreground of a VGA's
sixteen colours on a background of the first eight.

### Text on the Video Board

The Video Board's driver draws the text.  Each cell is a character from the font
`\OS\Fonts\CONSOLE.SFT`: an [SFT container](screen.md#blits) (no palette) of 256 transparent images of 8 by 16
pixels, in Latin-1 order.  It is made from Terminus Font (SIL Open Font License) by
`HCP65/Fonts/MakeConsoleFont.py`.

- **Colours.**  A cell's pixels are 0 (its background) or `$FF` (its foreground, what a glyph's
  pixels are).  Its colour is its palette number, which the board's Palette Write sets as the
  pixels are written.  Palette *p* is foreground *p* & 15 on background (*p* >> 4) & 7, so 127
  colours need no work per pixel.  Palette 127 is the cursor's, which the vertical blank interrupt
  swaps to make it blink.
- **Two frames.**  The processor can only write the frame that isn't shown, so each cell is drawn
  in that frame and the frames flip at the next vertical blank.  The cell is then drawn again in the
  other frame, so the two always match.
- **Scrolling** moves no pixels.  The row table says where each line of the screen comes from, so
  scrolling up a row starts the table one row of cells further on and blanks the row that comes in
  at the bottom.  The interrupt changes the table at the same vertical blank as the flip.
- **Speed.**  A glyph is drawn by walking its image's runs in assembly, so only its own pixels
  are written.  That needs the cell under it blank, so the driver keeps which cells of each frame
  have pixels and clears one first only if it has.  A row that scrolls in is blanked with one
  fill.  A cell that is already what is written isn't drawn, and the cursor is only drawn while a
  program reads or waits for what is typed, not while text is written.  The shell sends its output
  a line at a time.

## The shell

The shell prompts with the current directory and a `$` (`/ $` at first) and reads a line.  Backspace takes
back the last character and Ctrl-C drops the line.  The commands have their Linux names, and
paths are written with `/` as Linux's are (`\` is taken too): they are the card's FAT paths, from the
current directory unless they start with `/`, and case doesn't matter.

| Command | What it does |
|---|---|
| `help` | Lists the commands |
| `ls [path]` | A directory's files with their sizes, and its directories with a `/` after their names |
| `cd [path]` | Changes the current directory; to the root without a path |
| `pwd` | Shows the current directory |
| `cat <file>` | Shows a file |
| `cp <from> <to>` | Copies a file of up to 32KB |
| `rm <file>` | Deletes a file |
| `mkdir <directory>` | Makes a directory |
| `lsdev` | The boards in the device sockets, their drivers and units |
| `lsmod` | The drivers loaded from `\OS\Drivers`, and the boards each drives |
| `#!text free` | Main memory, what programs can have, and the kernel's heap |
| `mode [0\|1\|2]` | The console's size, or the Video Board's mode |
| `clear` | Clears the screen |
| `uname` | The shell's version |
| `#!text exit` | Ends the shell (`INIT.EXE` starts it again) |

Anything else is a program to run.  It is looked for as given and then in `\OS`, with `.EXE` added
if it has no extension.  A number after it is its argument.  The shell waits for it to exit, and
shows its exit code if that isn't 0.

The emulator's default machine (`Machines/SmallMainboard.xml`) has a Video Board and a PS2 Board,
so it starts in the shell on its screen, with the host's keyboard.
