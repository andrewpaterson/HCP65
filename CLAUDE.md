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
  TestAssembler65Lib reads `Website/docs`.

## Rules for the code

- Generated assembly (the compilers' back ends, thunks, linker maps) writes numbers as `0xFF` and `0b1010`, never
  `$FF`; hand-written `.asm` may use `$`.  Peepholes that match emitted text match the `0x` form.
- In 65C64 assembly, a label goes on its own line above the instruction it labels; constants stay on one line.
- The kernal has no floats: flag any `float`, `double` or `math.h` in what the kernal builds (the compiler's no-floats
  setting and the linker reject them too).
- The 65C64's scope leaves out the spreadsheet's pinout, user-mode memory bank remapping, internal 16 MB SRAM and
  programmable CPLD: don't model them or report inconsistencies in them.
- W65C816 code generation: no frameless leaves, and function pointers and vtables keep the stack-argument entry
  (see `../CommonLibraries/Libraries/CppCompilerLib/CodeGenerationHandoff.txt`, Rulings).
- Drill files are written in millimetres.

## Recipes

- `kicad-cli` rewrites the `.kicad_prl` beside every schematic it exports: copy the project into a scratch folder
  and export from the copy.
- Cycle counts from the `65` test programs depend on the run folder's path length (`__FILE__` is compiled in):
  compare before and after from folders whose paths are the same length.

## Working in these repositories

<!-- This section is the same in every Codaphela repository's CLAUDE.md: change them together. -->

- Several Claude sessions work in these checkouts at once, on the main branch (no private branches or worktrees).
  Run `git status` before touching a file and leave other sessions' uncommitted work alone.  Stage and commit only
  your own files, by path: `git add <paths>` and `git commit -m "..." -- <paths>`; never `git add -u`/`-A` or
  `commit -a`, and not `repos.py commit` (it stages everything).  Check `git show --stat HEAD` after.
- Commit messages have no Co-Authored-By line.  Make commits logical: one change each, its tests with it.
- Bring work from a copy into the main checkout with a 3-way merge (`git merge-file`, or `diff -u base mine |
  patch`), never by copying files over it: another session may have changed the target since.
- Before building a step of a shared plan, `git fetch` and read `git log HEAD..origin/<branch>` for the plan's
  files: a session on another machine may have done it.
- The shared build folders (`../build/*`) can hold binaries linked from another session's half-finished edits.  A
  failure seen only there: build the same sources in a private build folder; if that passes, rebuild the shared one
  clean.  Tests rewrite `Output/` in their project folder: run from a copy of that folder when another session may
  be running the same suite.
- Build, run and measure in Release x64 by default; Debug for full test sweeps or chasing a problem.  Say which build
  a number came from.  A speed claim that feeds a decision needs a like-for-like A/B (same commit but the change, in
  the whole test program) judged by instruction counts (callgrind) with the clock beside them.
- A decision the user states while planning settles that question only: plan with them, and wait to be told to
  build.
- Put decisions to the user one at a time, each explained plainly (what changes, what it costs), with the
  recommended option first, especially outside their speciality.
- Show images as full `file://` paths to the real files; don't make enlarged copies.
- Code files are LF.  `.sln`, `.vcxproj` and `.filters` are CRLF with a BOM: edit them as bytes and check `git diff
  --stat` shows only your lines.  Test `Input` files are kept byte for byte.
