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

- Pages live beside the code they describe, in each project's `Docs` folder, laid out as the site's paths and
  listed as items of the project: `X65C64-Proposal/Docs` (the 65C64's architecture and instructions, beside the
  spreadsheet), `Libraries/Assembler65Lib/Docs`, `Libraries/Libc65/Docs`, `Kernal/Docs`, `Boot/Docs`,
  `Shell/Docs`, `HCP65-Emulator/HCP65Lib/Docs` (the hardware), `HCP65-Emulator/HCP65Emulator/Docs`.  Other
  repositories' projects have theirs (the compiler's in CommonLibraries' CppCompilerLib, the IDE's in HCP65-IDE).
  `Website/docs` keeps only the home page, the overviews and the stylesheet.
- `Website/` builds the site: `mkdocs.yml`'s `extra.sources` lists every `Docs` folder, `Website/web.sh build`
  gathers them, runs the generators and checks, then a strict build; `Website/web.sh` builds and serves it.
  `Website/Website.sln` is the site as a Visual Studio project.  `Website/DocsPlan.txt` is the plan for the
  documentation.  A new page goes in its project's `Docs` folder and `.vcxproj` (the build checks it is in
  exactly one project); a new `Docs` folder goes in `extra.sources`.
- Name code in a page with a reference: [`CTextArea`], [`CTextArea::ReplaceRanges`], [`CShape::Init(int)`] or
  [`BaseLib/Chars.h`].  The site build links it to its line on GitHub and fails on one that names nothing; a plain
  `CTextArea` naming a real class is a warning.  A span that isn't C++ but looks it is written `#!text abs`.
- A ```` ```cpp ```` block in a page compiles (the site build checks it, `hcp65index -examples`, as the project the
  page is an item of compiles): a hidden comment under the page's title, `<!-- cpp`, then the `#include`s and the
  declarations its examples use without declaring, then `-->`, goes before each block; a block of statements is
  compiled in a function.  ```` ```{.cpp .no-check} ```` is for one that shows an edit, not an API.
  References resolve against the symbol table `hcp65index` (HCP65-IDE, built in `../build/HCP65IDERelease`) makes
  of the solutions in `mkdocs.yml`'s `extra.solutions`; it is kept in `Website/.symbols` until a repository's HEAD
  changes (about two minutes to make again), and `Website/web.sh index` remakes it.  `extra.repositories` says
  where each repository is on GitHub (`Website/tools/code_references.py`, `symbol_table.py`).
- 65C64 pages: `opcode-encoding.md` and the instruction tables are generated from the spreadsheet by `web.sh`
  (`Website/tools/instruction_tables.py` writes `X65C64-Proposal/Generated`, as
  `X65C64-Proposal/InstructionTables.yml` says; the pages include the tables by snippet, and
  `Generated/Instructions.txt` is every instruction's codes).  A table's wording is changed in the spreadsheet, in
  red; a new table or row in `InstructionTables.yml`.  `operands.md`'s modes and `registers.md`'s registers and status
  bits are checked against the sheets drawn as diagrams.  TestAssembler65Lib assembles every ```` ```asm ```` block
  in the site's `65c64` pages (```` ```text ```` for syntax), and checks the assembler's and the simulator's
  instructions against `Generated/Instructions.txt`.
- Design pages (how it is built and why, its decisions and what isn't done) are in the `hcp65/design` folders:
  `Kernal/Docs` (the kernal, interrupts), `HCP65-Emulator/HCP65Lib/Docs` (their index, the hardware),
  `HCP65-Emulator/HCP65Emulator/Docs` (the emulator), `HCP65-Tester/HCP65Tester/Docs` (the board tester).
- Notes beside code: `Libraries/Assembler65Lib/Assembler65C64Syntax.txt`, `AssemblerW65C816Syntax.txt`,
  `W65C816Names.txt`; `Kernal/KernalInterrupts.txt` (finished, kept as the record the code cites).

## Building and testing

- Programs: `hcp65build` (built with HCP65-IDE, at `../build/HCP65IDERelease/hcp65build`), for example
  `hcp65build Kernal/HCP65Kernal.hcp65build -config "Release|W65C816"`.  See the site's `compiler/building.md`
  (`../HCP65-IDE/HCP65Build/Docs`) and `compiler/testing.md` (`../CommonLibraries/Libraries/CppCompilerLib/Docs`).
- Test suites build with CMake as in CommonLibraries (`cmake -S Tests/TestAssembler65Lib -B
  ../build/TestAssembler65Lib64 -DBASELIB_64=ON`, then `cmake --build` and `ctest --test-dir`).
  TestAssembler65Lib reads `Website/mkdocs.yml` and the `Docs` folders it lists.  Run TestLibc65 from
  `Tests/TestLibc65/TestLibc65` (as `ctest` does), else it can't find BaseLib.  TestHCP65Lib reads `Kicad/Computer`,
  the program folders, `Libraries` and `../CommonLibraries` by their relative paths (a private copy needs them linked
  at the same paths); `TEST_HCP65_KERNAL_ONLY=<Boot|UART|VIA|PS2|Shell|Console|Blitter|LCD|Video|NoMainMemory|
  Exceptions|Walk>` runs one of its kernal tests, and `TEST_HCP65_CLOCK`, `TEST_HCP65_RUN_CYCLES` and
  `TEST_MODPLAY_SECONDS` time `modplay.exe` (the other switches are read in `TestHCP65Kernal.cpp`).

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
- The SD cards run 4 bits wide: keep the kernal's SD driver and the emulated SD board 4 bit, never SPI or 1 bit.

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
- Every tracked file says what it is (`HCP65/Website/tools/check_files.py`, in the website's build): an item of its
  project (plans and notes too), test data, or a row of the `README.md` of a folder above it, whose table
  (`| Files | Are | From |`) says source, generated (with the command and its inputs) or third-party (with where from
  and which version).
- Code files are LF.  `.sln`, `.vcxproj` and `.filters` are CRLF with a BOM: edit them as bytes and check `git diff
  --stat` shows only your lines.  Test `Input` files are kept byte for byte.
