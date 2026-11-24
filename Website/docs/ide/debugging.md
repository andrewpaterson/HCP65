# Debugging

The IDE debugs HCP65 programs in the [HCP65 emulator](../hcp65/emulator.md), with Visual Studio's Debug
menu and keys.  It knows nothing of the processor itself: the emulator describes its registers and
addresses and disassembles its own code, so the same debugger works for the W65C816 and the 65C64.  It
runs the emulator with a shared memory channel to it, or attaches to one already running over the network.

## The Debug menu

| Item | Keys | What it does |
|---|---|---|
| Start Debugging | F5 | Starts the emulator under the debugger, running to the first breakpoint; while stopped, continues. |
| Start Without Debugging | Ctrl+F5 | [Runs](building.md#running) the run configuration's tool without the debugger. |
| Stop Debugging | Shift+F5 | Ends the emulator. |
| Stop Running | | Ends what Ctrl+F5 started. |
| Restart | Ctrl+Shift+F5 | Starts it again from the beginning. |
| Break All | Ctrl+Pause | Stops it where it is. |
| Attach to Emulator... | | Connects to an emulator already running, by the port it printed (started with `--debug-tcp`). |
| Step Into | F11 | One step, into a call.  Not debugging, starts the emulator stopped at its first instruction. |
| Step Over | F10 | One step, over a call.  Not debugging, as F11. |
| Step Out | Shift+F11 | Runs until the function returns. |
| Toggle Breakpoint | F9 | A breakpoint on the caret's line, or at the disassembly's instruction under the caret. |
| New Breakpoint at Address... | | A breakpoint at an address typed in hexadecimal (a leading `$` is allowed). |
| Delete All Breakpoints | | Takes them all away. |
| Edit Configurations... | | The [run configurations](building.md#run-configurations). |
| Windows > | | Call Stack, Locals, Autos, Watch 1-4, and Hexadecimal Display (see [The debugger's windows](#the-debuggers-windows)). |
| QuickWatch... | Shift+F9 | The expression at the caret, in a dialog (see [QuickWatch](#quickwatch)). |

A step is one instruction when the disassembly has the keys, and one source line in a source file.

F5 debugs what was last built: build first (Ctrl+F9).

## What debugging starts

Start Debugging uses the chosen run configuration's build configuration:

1. It starts the emulator: the run configuration's tool, or the default emulator for a W65C816
   configuration.  An emulator for another processor than the configuration's is refused as soon as it
   says what it is.  MSVC and GCC configurations can't be debugged.
2. It loads each `<Load>` of the configuration (the HCP65's boot loader ROM) into the emulator at its
   address, then the configuration's own built image if it has no card, while the emulator waits at
   its first instruction.
3. A built executable, module or image with a card is written onto the configuration's SD card image
   (Card, at Path on card): a program for the kernal to run, a driver for it to load, or the kernal
   itself, `\OS\OS.SYS`, for the boot loader to load.
4. It checks the kernal on the card (see [The kernal on the card](#the-kernal-on-the-card)).
5. It resets the emulator so it starts from what was loaded, and runs.

What it did, and the emulator's own output, go in the Build tab.

## The Debug tab

The Debug tab under the editor shows the **disassembly** beside the **registers**:

- the disassembly is the instructions round the program address, `=>` at it and `*` at each breakpoint;
- the registers are the processor's state at the last stop, as the emulator describes it, and the
  breakpoints.

F9 in the disassembly toggles a breakpoint at the instruction the caret is on.  When it stops at code a
built program has a line for, that file is opened with its line marked.

## The debugger's windows

Visual Studio's Call Stack, Locals, Autos and Watch windows, docked together beside the editors on the
right.  Call Stack and Locals are shown when debugging starts; the others from Debug > Windows.  Each
shows where the emulator stopped, and is empty while it runs.  They need the program built for the
debugger (a configuration's Debug, see [Building](building.md)): a Release build shows its calls, but not
their parameters or locals.

### Call Stack

A row per call the program is in, found by walking the stack from the stop through the programs running
there (the kernal's calls and interrupts too, on into the process that made them), with each parameter's
type, name and value and the line it is at:

    modplay.exe!Scale(SPoint* psPoint = 0x00eff5, int iBy = 2) Line 5

The innermost has a yellow arrow; code with no debug records (the start up) is `[External Code]`.  Enter
or a double click on a call chooses it: its line is shown in its editor, with a green arrow in the gutter,
and Locals, Autos and the Watches show that call.

### Locals, Autos and Watch

Each is a tree of values, Name | Value | Type:

- **Locals**: the chosen call's parameters and locals in scope at its line, `this` first.
- **Autos**: what the line and the line before use (names and member chains such as `p->x`, not calls),
  and, just after a Step Out, what the function returned (`Sum returned`).
- **Watch 1-4**: expressions typed into the name of the empty last row (or added by the editor's Add
  Watch), changed by typing over one and removed by emptying it.  They are kept with the breakpoints in
  the workspace's `.hcp65user`.

An object, an array or a pointer opens to its members, elements or what it points at, read only when
opened.  A value is shown as its type has it: an `int` in decimal (hexadecimal with Hexadecimal Display),
a `char` as `99 'c'`, an enum by its enumerators (`EF_A | EF_C`), a `char*` with its string, an object
through a base class pointer with its own class (`CShape* {CSquare}`).  A value that changed since the
last stop is red.  Typing into a Value writes it, if what is typed fits its type.

Watch expressions are evaluated with the program's types (an `int` is 16 bits): names (locals, then
`this`'s members, statics and globals, enumerators), `. -> [] * &`, casts, `sizeof`, and the arithmetic,
comparison and logical operators.  Functions aren't called and nothing is assigned.  A suffix formats the
result as Visual Studio's do: `,x` hexadecimal, `,d` decimal, `,s` a string alone, `,10` a pointer as an
array of 10.

### QuickWatch

Shift+F9 (or the editor's QuickWatch) shows the expression at the caret, or what is selected, with its
value: Reevaluate evaluates what is typed, Add Watch adds it to Watch 1.

### Data tips

The pointer resting on a name in the editor while stopped shows its value under it, opened like a Locals
row; a member's (`pcCounter->miCount`) from the name it is a member of.  It goes when the pointer moves off
the name and the tip.

## Breakpoints

F9 in a source file, or a click in the gutter's first column, toggles a breakpoint on that line.  It is
drawn as a red cell; a red `o` means it isn't **bound** yet.  A build writes debug info beside each
program (`<program>.debug`), whose line table binds a line's breakpoint to the addresses its code is at:

- in the configuration's own program, in its [breakpoint space](#breakpoint-spaces);
- in what is loaded with it (the boot loader's ROM), in every space;
- in the kernal on the card, `OS.SYS`, in its own breakpoint space and every process: from another
  solution of the workspace that builds an image onto the card, or from a `<Lines>` of the
  configuration ([Building Programs](../compiler/building.md));
- in a program of another solution of the workspace that the kernal runs from the card, in that
  program's own process, and only while it runs;
- in a driver the kernal loads, once it is loaded, wherever it was loaded.

A breakpoint waits unbound while its program hasn't been built, or isn't running.  Source breakpoints are
kept when debugging stops and saved in `<workspace>.hcp65user`, yours alone.

### Breakpoint spaces

The HCP65's kernal and the programs it runs share addresses, so the emulator keeps breakpoints in
**spaces**: "Kernel" and "User".  A build configuration's Breakpoint space says which its program's
breakpoints go in; empty, they stop in every space.

### A program the kernal runs

When the configuration's program is one the kernal runs as a process (its file on the card, say
`\OS\SHELL.EXE`), its breakpoints stop only in that process.  The emulator tells the IDE as each process
starts and ends; when the program's process starts, the emulator stops before it runs so its breakpoints
can be given, and goes on.  That stop isn't shown.

### The kernal on the card

The kernal isn't loaded by the IDE: the boot loader reads it from the card.  Its line table is only
right if the card's `OS.SYS` is the one that was built, so when debugging starts the IDE compares
them (a `<Lines>` says which file on the card with its `CardPath`).  If they differ, the Build tab
says that the card's `\OS\OS.SYS` isn't the one built and its source lines may be wrong; if the
card hasn't one, that the source lines aren't of what runs.  Debugging goes on either way.
Debugging the kernal's own configuration once puts the one built on the card.

## Attaching

An emulator started with `--debug-tcp` prints the port it listens on.  Attach to Emulator... asks for it
and connects; then it is debugged as if the IDE had started it.
