# The Board Tester

HCP65Tester tests the HCP65's real boards, one at a time, before they go into the computer.  It runs on a PC and
drives a tester board over a serial port: the tester powers the board under test, drives some of its connector's
pins and reads the others back.  The tester boards are KiCad projects in `Kicad/Pico` (the HCP65 Board Tester and
the 24, 32, 40, 56 and 64 pin testers, built around a Raspberry Pi Pico); the Pico's firmware is HCP65PicoTester in
the Pico repository beside this one, whose `Source/Commands.cpp` lists its commands.  There is no user page yet:
the program has no user interface beyond an empty window.

## How a board is tested

A board under test is described by its connector ([`HCP65Tester/BoardPins.h`], `CBoardPins`): each pin, numbered
from 1 to at most 128, is a signal (a name, a direction from the board under test's side, normal or inverted), a
power pin (ground or 5V) or not connected, and signals can be grouped into named buses, each pin at a bit offset.
The descriptions are C++, one function per board: the standard 24, 32 and 56 pin boards' power pins
([`HCP65Tester/CommonBoard.cpp`]), the Address Decode board's signals ([`HCP65Tester/AddressDecodeBoard.cpp`]), and
a test board whose every pin is a bus bit ([`HCP65Tester/TestBoard.cpp`]).

From a description the program makes the tester's commands, each a line of text sent over the serial port
([`HCP65Tester/UART.h`], `CUART`, on the Windows COM port calls in [`HCP65Tester/WindowsCOM.cpp`]):

| Command | Does |
|---|---|
| `#!text POW` | Everything off: power, outputs and values. |
| `#!text P` then pairs such as `#!text Gd1` or `#!text 5c1` | Connects the grounds and 5V supplies the board's power pins need (which of the tester's ground and 5V lines a board uses follows from its pin numbers). |
| `#!text R0_n` | Reads bytes 0 to *n* of the pins; `#!text RR` reads the same again. |
| `#!text O` and hex | Which pins the tester drives, the board's inputs. |
| `#!text W` and hex | The values it drives them to. |

A reply to a read is hex, the high nybble first, which `CBoardPins::UpdateRead` turns back into each pin's value.
The test board's loop writes patterns of alternating blocks of ones and zeroes (8 pins wide, at every phase) and
reads them back twice.

## The window

`CTesterApp` ([`HCP65Tester/TesterApp.h`]) is a rooted application object that owns the window, "HCP65 Board
Tester", 1280 by 800, in the dark Jewel theme's panel colour.  It has no components: its tick
([`HCP65Tester/TesterWindowTick.h`]) does nothing.  It redraws only what changed, and `HCP65_TESTER_VERIFY_REDRAW`
checks each partial paint against a whole one.  Each platform's `main` makes the native window factory and runs the
app: [`HCP65Tester/HCP65Tester.cpp`] on Windows (WinGDI) and [`HCP65Tester/HCP65TesterLinux.cpp`] on Linux (X11).
The Linux build (CMake, or `rake build` and `rake run`) is the window alone: the board descriptions and the serial
port are Windows only.

## Decisions

- The tester should look like Logicim, in IntelliJ IDEA's style, with Logicim's components (the user, 2026-10-23).
  A window of that kind (menus, a toolbar, Subcircuits and Simulations tool windows, a circuit canvas, a status bar
  and Logicim's dialogs) was built for it in October 2026 but isn't in this repository: the tester's window has
  been empty since the tester came into it (2026-11-01).  That user interface is now the LogiCim program's, in the
  LogiCim repository (`LogicimWindowContent`).
- The tester is a rooted application object that owns its window (2026-11-03), as the emulator is.

## Not done

- Nothing is tested yet.  The board loop (`TestBoardLoop`, a 64 pin test board on COM3) is commented out of
  `WinMain`; the test board's loop reads the pins back but doesn't compare them with what it wrote; the Address
  Decode board's test (`TestAddressDecode`) isn't called.
- The board under test, the port and the test are chosen in the code: nothing in the window shows or chooses them.
- Boards are described in C++; the KiCad schematics, which already say what is on each connector, aren't read.
- The serial port is Windows only, so the Linux build can't drive a tester.
- `Resources` still holds a font and `PrideAndPrejudice.txt` from the October window, which nothing reads.
