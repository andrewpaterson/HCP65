# HCP65

The HCP65 home computer (a W65C816 machine) and the 65C64 processor proposal: hardware, kernal, programs,
emulators, the assembler and C library, and the project's website.  It needs CommonLibraries checked out
beside it (`../CommonLibraries`); programs are built with HCP65-IDE's `hcp65build`.

## Layout

- `Kicad/`: the boards.  `Datasheets/`: third-party datasheets, not part of the documentation.
- `X65C64-Proposal/`: the 65C64.  `65C64 Instruction Architecture.xlsx` is the source of truth; the website's
  65C64 pages are derived from it.  Edit it with LibreOffice (headless UNO for scripted edits), changes in red.
- `Kernal/`, `Boot/`, `Shell/`, `ModPlay/` and the other top-level program folders: HCP65 programs, each a
  Visual Studio solution with an `.hcp65build` file beside it.
- `Libraries/`: Assembler65Lib (assembler and linker for the 65C64 and W65C816), Libc65 (the C library and
  runtime), UserLib (program start-up and kernal calls), DebugLib, DeviceRomLib.
- `Tests/`: TestAssembler65Lib, TestLibc65.
- `HCP65-Emulator/` (the HCP65Emulator app, HCP65Lib, TestHCP65Lib), `X65C64-Emulator/`, `HCP65-Tester/`.
- `Website/`: the documentation site (MkDocs Material).

## Documentation

- `Website/docs`: `65c64/`, `compiler/`, `hcp65/`, `ide/`, `libraries/`.  `Website/web.sh build` runs the
  checks and a strict build; `Website/web.sh` builds and serves it.  `Website/DocsPlan.txt` is the plan for
  the documentation (pages moving beside their code, references to code checked by the C++ index).
- 65C64 pages: `opcode-encoding.md` is generated from the spreadsheet (`Website/README.md` has the command);
  `Website/tools/check_instruction_tables.py` checks the instruction index against the instruction pages;
  TestAssembler65Lib assembles every ```` ```asm ```` block under `docs/65c64` (```` ```text ```` for syntax).
- Notes beside code: `Libraries/Assembler65Lib/Assembler65C64Syntax.txt`, `AssemblerW65C816Syntax.txt`,
  `W65C816Names.txt`; `Kernal/KernalInterrupts.txt`, `Kernal/ImagesPlan.txt`.

## Building and testing

- Programs: `hcp65build` (built with HCP65-IDE, at `../build/HCP65IDERelease/hcp65build`), for example
  `hcp65build Kernal/HCP65Kernal.hcp65build -config "Release|W65C816"`.  See
  `Website/docs/compiler/building.md` and `Website/docs/compiler/testing.md`.
- Test suites build with CMake as in CommonLibraries (`cmake -S Tests/TestAssembler65Lib -B
  ../build/TestAssembler65Lib64 -DBASELIB_64=ON`, then `cmake --build` and `ctest --test-dir`).
  TestAssembler65Lib reads `Website/docs`.  A test rewrites `Output/` in its project folder: run from a copy
  of that folder when another session may be running it.
