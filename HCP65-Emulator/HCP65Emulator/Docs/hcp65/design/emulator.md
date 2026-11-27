# The Emulator

[HCP65 Emulator](../emulator.md) is the user page.  This page says how the emulator program is put together: the
window, the thread the HCP65 runs on, sound, the reference build, the debugger's end of the connection, and the
test suite that builds what it boots.  How the boards themselves are modelled is on [The Hardware](hardware.md).

## What it is built from

| Part | Where | Gives the emulator |
|---|---|---|
| HCP65Lib | `HCP65-Emulator/HCP65Lib` | The machine: [`CHCP65`], its boards, [`CHCP65Mainboard`], [`CHCP65DebugTarget`] ([The Hardware](hardware.md)). |
| HCP65Emulator | `HCP65-Emulator/HCP65Emulator` | The program: [`CHCP65EmulatorApp`], its window, input, sound and dialogs. |
| MicroprocessorLib | CommonLibraries | [`CFastW65C816`], the instruction level W65C816. |
| WindowLib | CommonLibraries | The window, its components and dialogs, and the sound mixer. |
| WinGdiLib, WinWaveoutLib, WinInputLib | CommonLibraries | Windows: the window, waveOut sound and raw input. |
| X11Lib, PulseAudioLib | CommonLibraries | Linux: the window and its input, and PulseAudio sound. |
| WinRefLib | CommonLibraries | The reference screen `HCP65EmulatorRef` draws on. |
| EmulatorDebugLib | CommonLibraries | [`CEmulatorDebugger`]: the debugger's channel and server. |
| RasterLib, RasterRenderLib, RenderLib | CommonLibraries | The Board Configuration dialog's picture of the mainboard. |
| DeviceRomLib | `HCP65/Libraries` | [`CDeviceROM`]: each emulated board's device ROM; [`CDeviceROMSchematic`] reads one from a schematic, with KicadLib, for TestHCP65Lib. |

## The program

[`CHCP65EmulatorApp`] is one rooted object that owns the HCP65, its mainboard, the debugger, the runner and the
window.  Each platform has a small `main` that makes the native factories and calls [`HCP65EmulatorRun`]:
[`HCP65Emulator/HCP65Emulator_Windows.cpp`] (WinGDI, waveOut, raw input), [`HCP65Emulator/HCP65Emulator_Linux.cpp`]
(X11, PulseAudio, X11's key and mouse events given to the input as Windows raw input events) and
[`HCP65Emulator/HCP65EmulatorRef.cpp`] (the reference screen, no sound).

[`CHCP65EmulatorApp::Setup`] reads the command line: the debugger's arguments, `--machine` (a mainboard
description; `Machines/SmallMainboard.xml` without it), `--card` (an image for the first SD Card Board's socket 0)
and `--fill-memory` ([`CHCP65::SetMemoryFill`]).  It loads the boot loader's ROM from TestHCP65Lib's
`Output/Boot.rom`, which, with the card images the descriptions name, the test suite builds: the emulator has no
copy of its own.  A debugger that will load the ROM itself (`--debug-wait`) lets it start without one.

The HCP65 runs on a thread of its own, [`CHCP65EmulatorRunner`], not in the window's ticks: a modal dialog stops
the ticks, and the machine must keep running under it.  The runner runs the HCP65 to where its clock should be by
now, at most 100 ms of its time at once, and sleeps when it is there; when the host falls behind, the HCP65 slows
down rather than catching up in a burst.  Anything else that changes the HCP65 (a reset, a board change, a debugger
request) pauses the runner first ([`CHCP65EmulatorRunner::Pause`]), and its clock starts again from where it
stopped.  With a debugger attached the debugger runs it, and the HCP65's time stands still while it is stopped.

What the window shows comes from the boards across that thread boundary, under each board's lock:

- The screen ([`CHCP65ScreenDraw`]) is the first Video Board's last whole frame, scaled by a whole number so its
  pixels stay square.  It has the focus, and gives each host key going down and up to the [`CHCP65PS2Keyboard`] on
  port 1 of the first PS2 Board, and to the SNES controller whose mapping has the key ([`CHCP65SNESKeys`]).
- Under it, while there is an LCD and SNES Board, its display ([`CHCP65LCDDraw`]) and a combo box per controller
  choosing its key mapping.  The mappings are the emulator's, in `SNESMappings.xml` beside it, edited in the SNES
  Key Mappings dialog ([`CSNESMappingDialog`]).
- The HCP65 menu's Board Configuration ([`CBoardConfigurationDialog`]) changes the boards in the sockets, over a
  turning 3D picture of the Small Mainboard with the chosen boards standing in it ([`CBoardSceneDraw`]).  The
  models are KiCad's VRML exports, which take seconds to read, so each is read on one of four loader threads when
  first wanted and cached as a scene file in `Output/Models`; the dialog answers meanwhile.  OK writes the
  description and resets the HCP65.

The window redraws only what changed; `HCP65_EMULATOR_VERIFY_REDRAW` checks each partial paint against a whole one.
`HCP65_EMULATOR_TRACE` prints how far the HCP65 has got each second, and `HCP65_EMULATOR_SECONDS` closes the window
after that many seconds (no use when someone wants to listen).

## Sound

The first Sound Board is heard.  The board keeps the frames its DAC was given, unsigned 8 bit stereo, under a lock
([`CHCP65Audio::ReadFrames`]); [`CHCP65SoundVoice`] is a stream voice in the window's sound mixer that takes them
at the board's sample rate, and the mixer plays on a thread of its own, for the same reason the HCP65 does.  While
the board is disabled it makes no frames and the voice is silent.

The host's sound is a native sound factory: waveOut on Windows ([`CWinWaveoutSoundFactory`]), and on Linux
PulseAudio's simple API, which PulseAudioLib loads at run time (`dlopen` of `libpulse-simple.so.0`), so the
emulator builds and runs without PulseAudio's development files (the user's choices, 2026-10-31).  The reference
build has no sound factory, and so no mixer.

The emulator runs at the board's 25MHz ([`CHCP65::GetClockRate`]) in real time.  On 2026-10-31 it played Space
Debris through modplay at that clock on Linux in real time; that it keeps up is what TestHCP65Lib's Boot test at
`TEST_HCP65_CLOCK=25000000` is meant to check ([below](#testhcp65lib)).

## The reference build

`HCP65EmulatorRef` is the emulator on WinRefLib's reference screen: drawn into memory, nothing on a display, no
sound, and no input from outside.  It is to the emulator what `HCP65IDERef` is to the IDE: the IDE's debugger
checks start it (`HCP65_IDE_EMULATOR` names it) and debug programs in it.  It takes the same arguments and
`HCP65_EMULATOR_*` variables and runs from the same folder.  Only the Linux CMake build makes it: there the
emulator's sources are a library that both mains link.

## The debugger's end

[The Debugger](../../ide/design/debugger.md) is the protocol and the IDE's side.  The emulator's side is:

- [`CEmulatorDebugger`], shared with the X65C64 Emulator: the channel from the command line (`--debug-shm`,
  `--debug-tcp`, `--debug-wait`, `--exit-on-detach`, [`EmulatorDebugLib/DebugLaunch.h`]) and the server.
- [`CHCP65DebugTarget`], the HCP65 as a [`CDebugTarget`]: the W65C816's registers and the process the remap is for;
  two breakpoint spaces, "Kernel" and "User", and user mode's breakpoints for one process; data watches per space,
  with a process's user address aliased to kernel mode's physical one ([`CHCP65DebugTarget::GetWatchAlias`]) so the
  kernal writing a process's memory stops too; instructions decoded by what the assembler knows
  ([`CAsmW65C816DebugDecoder`]).  Memory is read and written without touching devices: a device address reads `$FF`
  and a write stops at it.  User mode's memory is read through the remap table of the process the processor board
  names, so a stop in a kernal call can read its caller's stack.
- The processes and modules come from the kernal itself: it tells [`CHCP65DebugPort`] of each process it starts and
  ends and each driver it loads, with `WDM` ([The Hardware](hardware.md#debugging-aids)).  While a debugger
  watches, a process started stops the run before its first instruction, and a driver loaded before any of its code
  runs, so the debugger can bind their breakpoints first.
- Reset from the File menu goes through the debugger when there is one, so the debugger can tell its client.

## TestHCP65Lib

TestHCP65Lib tests HCP65Lib and builds everything the emulator boots.  It runs four groups:

| Group | Tests |
|---|---|
| TestHCP65Font | The console font's SFT against its bitmap. |
| TestHCP65Machine | The boot and the modes, the timer, contention, fill mode, every board, mainboard descriptions and board changes, and each board's device ROM against what [`CDeviceROMSchematic`] works out from its schematic. |
| TestHCP65Debugger | The debug port and target: the kernal's `WDM` messages in either mode, the stops they make, process breakpoints, data watches, and the protocol over them. |
| TestHCP65Kernal | Builds the boot loader's ROM, the kernal (`OS.SYS`), the drivers and the programs with the compiler in the test ([`CHCP65KernalBuilder`], [`CHCP65DriverBuilder`], [`CHCP65ExeBuilder`]), puts them on copies of the blank card, and boots machines described in its `Machines` folder. |

The kernal tests are Boot, UART, VIA, PS2, Shell, Console, Blitter, LCD, Video, NoMainMemory, Exceptions and Walk;
`TEST_HCP65_KERNAL_ONLY=<name>` runs one of them.  What they write to `Output` is what the emulator boots
(`Boot.rom`, `SDCard.img`) and what the IDE's debugger checks run on (`BootCard.img`), so the suite is run before
either.  The machine tests read the boards' schematics from `Kicad/Computer`, so a private copy of the test folder
needs that folder linked at the same place relative to it, as it needs the program folders, `Libraries` and
CommonLibraries.

The kernal tests run the HCP65 at 4GHz (`TEST_HCP65_CLOCK_RATE` in [`TestHCP65Lib/TestHCP65Kernal.cpp`]), so
modplay never runs out of samples and what the sound board plays is exactly what [`CModPlayer`] renders on the
host.  `TEST_HCP65_CLOCK`, `TEST_MODPLAY_SECONDS` and `TEST_HCP65_RUN_CYCLES` override the clock, the seconds
modplay plays and the cycles of each run, to time modplay at the board's clock.  `TEST_HCP65_VERBOSE` prints each
run's state, `TEST_HCP65_MAP` writes the link maps to files, and `TEST_HCP65_TIME_COP` times the bank mapping
calls.

## Decisions

- Two Visual Studio projects in a solution of their own (`HCP65Emulator.sln`), the app HCP65Emulator built as
  WindowInputTester is and the machine a library, HCP65Lib (the user, 2026-10-31).  The memory and device map are
  the KiCad schematics' (the user, 2026-10-31).
- The emulator runs at the real clock, 25MHz, and stays there although the small mainboard being built runs at
  about 12.5MHz (the user, 2026-10-31 and 2026-11-05).
- A Linux build beside the Windows one, with X11, and host sound by waveOut on Windows and PulseAudio's simple API
  loaded at run time on Linux (the user's choices, 2026-10-31).
- The HCP65 runs on a thread of its own, so it keeps running under modal dialogs; the mixer plays on its own thread
  for the same reason.
- `HCP65EmulatorRef` for the IDE's checks, as `HCP65IDERef` is the IDE.
- TestHCP65Lib builds the ROM and the cards the emulator boots; the emulator keeps no copy of them (a stale kernal
  ROM in `HCP65Emulator/Input` was deleted on 2026-11-22).

## Not done

- TestHCP65Lib's Boot test doesn't finish its boot at `TEST_HCP65_CLOCK=25000000`.  Checked on 2026-11-26 (HCP65
  8fc6ad1, CommonLibraries 70b41db52, Release x64, `TEST_HCP65_KERNAL_ONLY=Boot`, `TEST_HCP65_VERBOSE=1`):
  modplay's sound starts after about 8 s of the HCP65's time, and at the end of every run after that the processor
  is in kernel mode at `$0107E0` while the Sound Board plays its buffer round and round.  The test uses up its 2000
  runs (40G cycles, 3.5 minutes) and fails with the kernal still at state 6 (mounted), not 8 (the start up program
  exited), then stops on the failed assert.  modplay doesn't get past that wait, so this is a hang, not slowness.
  At the default 4GHz the same run passes (all 1683 asserts).  It is the kernal's and modplay's to find; until then
  nothing checks that modplay keeps up at the board's clock.
- Only one processor: the Full Mainboard is refused ([The Hardware](hardware.md#not-done)).
- The Windows build is built by the user in Visual Studio; it isn't built or run on this Linux machine.
  `HCP65EmulatorRef` has no Visual Studio project, which the IDE's debugger checks on Windows would need.
- The emulator has no copy of the ROM or the card: it boots TestHCP65Lib's `Output`, so a fresh checkout must run
  the suite first.
- Only the first Sound Board is heard, only the first Video Board shown, and only the first LCD and SNES Board's
  display and controllers are wired to the window.  Port 2 of the PS2 Board has nothing plugged in (no mouse).
- The UART Board's far end is the host's calls or a terminal ([`CHCP65Terminal`]); the window shows no terminal.
