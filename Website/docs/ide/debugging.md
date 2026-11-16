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

A step is one instruction when the disassembly has the keys, and one source line in a source file.

F5 debugs what was last built: build first (Ctrl+Shift+B).

## What debugging starts

Start Debugging uses the chosen run configuration's build configuration:

1. It starts the emulator: the run configuration's tool, or the default emulator for a W65C816
   configuration.  An emulator for another processor than the configuration's is refused as soon as it
   says what it is.  MSVC and GCC configurations can't be debugged.
2. It loads each `<Load>` of the configuration (a kernal) into the emulator at its address, then the
   configuration's own built image, while the emulator waits at its first instruction.
3. A built executable or module is written onto the configuration's SD card image (Card, at Path on
   card) for the kernal to run or load.
4. It resets the emulator so it starts from what was loaded, and runs.

What it did, and the emulator's own output, go in the Build tab.

## The Debug tab

The Debug tab under the editor shows the **disassembly** beside the **registers**:

- the disassembly is the instructions round the program address, `=>` at it and `*` at each breakpoint;
- the registers are the processor's state at the last stop, as the emulator describes it, and the
  breakpoints.

F9 in the disassembly toggles a breakpoint at the instruction the caret is on.  When it stops at code a
built program has a line for, that file is opened with its line marked.

## Breakpoints

F9 in a source file, or a click in the gutter's first column, toggles a breakpoint on that line.  It is
drawn as a red cell; a red `o` means it isn't **bound** yet.  A build writes a line table beside each
program (`<program>.lines`), and a line's breakpoint is bound to the addresses its code is at:

- in the configuration's own program, in its [breakpoint space](#breakpoint-spaces);
- in what is loaded with it (a kernal), in every space;
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
`\boot.exe`), its breakpoints stop only in that process.  The emulator tells the IDE as each process
starts and ends; when the program's process starts, the emulator stops before it runs so its breakpoints
can be given, and goes on.  That stop isn't shown.

## Attaching

An emulator started with `--debug-tcp` prints the port it listens on.  Attach to Emulator... asks for it
and connects; then it is debugged as if the IDE had started it.
