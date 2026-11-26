# Assembler and Linker Design

Assembler65Lib holds the assemblers of both processors, the one object format they write, the one linker that
links them and the files a link is made into.  What their source looks like and what they do is described for
users in [Assembler](../../65c64/assembler.md), [Objects and Linking](../../65c64/linking.md),
[W65C816 Assembler](../assembler-w65c816.md) and [Linker and Outputs](../linker.md); this page is for a programmer
who will change them: how they are built, why, and what the user ruled.

Three files beside the code hold the rules, one line each, and are still worked from:

- [`Assembler65Lib/Assembler65C64Syntax.txt`]: the 65C64 assembler's syntax and every choice in it, with the user's
  rulings of 2026-10-23 and after (memory in square brackets, suffixes, round brackets, the program counter C,
  widths in bits), and the section "Objects and linking": sections, relocations, COMDAT, removing unused sections,
  lazy loading and the files.
- [`Assembler65Lib/AssemblerW65C816Syntax.txt`]: the W65C816 assembler's, with the user's rulings of 2026-10-25.
- [`Assembler65Lib/W65C816Names.txt`]: the plan that named everything W65C816-specific W65C816, with its two open
  items.

## The toolchain

One toolchain builds for both processors, in three places:

| Part | Where | Is |
|---|---|---|
| Compiler | CommonLibraries, CppCompilerLib | C++ to each processor's assembly text ([Inside the Compiler](../inside.md), the [compiler's design](index.md)). [`CCPP65Builder`] runs a whole program: units, Libc65, the runtime, linking. |
| Assembler and linker | HCP65, Assembler65Lib | [`CAsm65C64Assembler`], [`CAsmW65C816Assembler`], [`CAsm65Object`], [`CAsm65Library`], [`CAsm65Linker`] and the output files. |
| `hcp65build` | HCP65-IDE, HCP65Build and BuildLib | The command line build ([Building Programs](../building.md)); [`CHCP65Build`] is the same build inside the IDE. |

A build never leaves memory until its program is written:

1. The back end writes a unit's assembly into a [`CCPP65Assembly`], which keeps each function's code in a spill
   store and is read a line at a time as a [`CAsm65Source`]: a unit's assembly is never all in memory at once.
2. The target assembles it: [`CCPPTarget65C64::Assemble`] or [`CCPPTargetW65C816::Assemble`], each with its
   processor's assembler, into one [`CAsm65Object`] per unit.  The `.lst` listings are written beside, not read back.
3. [`CCPPProgramLinker`] adds the objects in order (the start up first), Libc65, the runtime and the maths tables as
   [`CAsm65Library`]s, sets the linker up and links ([`CCPPProgramLinker::LinkObjects`]).
4. [`CHCP65Build`] makes the configuration's [`CAsm65Image`], [`CAsm65Executable`] or [`CAsm65Module`] from the
   link and writes it, with the debug info beside it.

So objects and libraries are only written to files by the tests: no build keeps them between builds.  The object
file format is used on disk as the format of an executable and a module, which the HCP65 kernal reads with the same
[`CAsm65Object`] (see [Outputs](#outputs)).

How the two callers set the linker:

| Who | Sets |
|---|---|
| [`CCPPProgramLinker`] | Missing names stubbed and unused sections removed, on by default; the limit (`CPP65_HEAP_ADDRESS`, `$80000`, unless [`CCPP65Builder::SetLimit`]); the text address; the target's bank size ([`CCPPTarget::GetBankSize`], `$10000` on the W65C816) and stub ([`CCPPTarget::GetStubOpcode`]); soft float and `math.cpp` forbidden in a build with no floats ([`CAsm65Linker::ForbidObject`]).  A runtime routine that is missing ([`CCPPTarget::GetRuntimePrefix`]) fails the link rather than being stubbed: the program would stop at its first multiply. |
| [`CHCP65Build`] | A module's link relocatable and its entry kept ([`CAsm65Linker::KeepSymbol`]); an executable's entry kept.  Each stubbed name is an error in the Build tab ([`CHCP65Build::CheckMissing`]) unless the configuration asks for stubs, so every missing name is listed at once, not just the first. |

The linker came from the C++ compiler's needs.  Before 2026-10-23 the 65C64 generator joined every unit's text
itself, with a prefix per unit to keep their labels apart, shared text it had already emitted, and joined the start
up on at the end.  The user asked for a linker to replace that, in Assembler65Lib, and the generator then wrote one
object per unit; the W65C816's assemblers wrote the same objects from 2026-10-25.

## The assemblers

### One base, two processors

The user asked for the assembler on 2026-10-23, with CppParserLib's structure (now CppCompilerLib) as the model:
a tokeniser, then a parser that writes as it goes.  It was the 65C64's only.  When the W65C816 back end needed one,
the user chose the same style for it (2026-10-25), so what isn't an instruction moved into a base class both
extend:

| Class | Does |
|---|---|
| [`CAsm65BaseAssembler`] | The passes, statements, labels, constants, expressions, directives, sections, objects and debug directives: everything that isn't a processor's. |
| [`CAsm65C64Assembler`] | The 65C64's registers, flags, operands, suffixes and instructions, written as words. |
| [`CAsmW65C816Assembler`] | The W65C816's operands and addressing modes, its opcode table, the m and x widths followed down a section, `.width`, `.word24` and `.relax`. |
| [`CAsm65Tokeniser`] | One line of text to tokens. |
| [`CAsm65Symbols`] | Labels and constants by name; values kept from pass to pass. |
| [`CAsm65Source`], [`CAsm65TextSource`], [`CAsm65IncludeSource`] | Where the lines come from: text in memory, any source with `.include` lines replaced by their files, up to 8 deep. |
| [`CAsm65KeptTokens`] | Tokens kept after their line has gone. |
| [`CAsm65Errors`] | Errors and warnings as `line:column: message 'token'` lines. |
| [`CAsm65Output`] | Absolute mode's bytes as segments at addresses; also the linker's. |

A processor's assembler provides [`CAsm65BaseAssembler::Instruction`], [`CAsm65BaseAssembler::IsRegisterName`],
[`CAsm65BaseAssembler::InstructionAlignment`] (2 bytes on the 65C64, whose instructions are words; 1 on the
W65C816) and [`CAsm65BaseAssembler::DebugProcessor`], and may add [`CAsm65BaseAssembler::ProcessorDirective`] and
[`CAsm65BaseAssembler::SectionStarted`] (the W65C816 forgets its widths and `.relax` there).

Names follow the user's rule of 2026-10-31: what is one processor's has its name in it ([`CAsm65C64Assembler`],
`Asm65C64Operands.cpp`, [`CAsmW65C816Assembler`], `AsmW65C816Instructions.cpp`) and what the family shares is plain
`Asm65`.  The `65C64` and `W65C816` spellings followed on 2026-11-13; [`Assembler65Lib/W65C816Names.txt`] is that
rename.  The `CAsm65` prefix itself keeps the assembler's classes apart from MicroprocessorLib's ([`C65C64`] and the
classes named after it).

The 65C64's encoding tables are MicroprocessorLib's, shared with its decoder and simulator
([`MicroprocessorLib/65C64Decoded.h`]: the register lists, immediate sizes and branch offsets of each mode,
[`Is65C64TwoRegisterMode`], [`Get65C64RegisterIndex`]; [`BitsToWidthCode65`] for widths), so an encoding is
written in one place.  The W65C816's opcode table ([`Assembler65Lib/AsmW65C816Opcodes.h`]) is the assembler's,
checked against MicroprocessorLib's simulator by `TestAsmW65C816Opcodes`; its mnemonics' index is built once,
before units are assembled on several threads ([`CAsmW65C816Assembler::IndexMnemonics`]).

### A line at a time

Every pass reads the source again from its start, a line at a time ([`CAsm65Source`]), and only that line's tokens
exist ([`CAsm65Tokeniser::TokeniseLine`]).  What must outlive its line is copied into a [`CAsm65KeptTokens`]: labels
waiting for the next statement, the first use in a pass of a name not yet defined, and the sections an
`associated` option names.  This was done on 2026-10-31 to make assembling use less memory: a unit's assembly is
read straight out of the compiler's spill store, and no pass holds a unit's tokens.  The tokeniser's errors are
found once, by a pass of their own before the first ([`CAsm65BaseAssembler::CheckTokens`]), not again by each pass.

### Passes

[`CAsm65BaseAssembler::Passes`] runs passes until nothing changes, at most 16 (`ASM65_MAX_PASSES`), then one final
pass that writes the bytes, records the relocations and reports the errors.  A pass changes when a symbol's value,
section or resolution changes, a name becomes an external, or an instruction grows.  If the final pass still
changes something, the assembly fails with "Labels did not settle".

- **Lengths only grow.**  Each instruction's length (in words on the 65C64, bytes on the W65C816) is kept from pass
  to pass ([`CAsm65BaseAssembler::RecordLength`]) and is the least the next pass may give it.  So a forward
  reference starts short and grows only if it must, and the passes always settle: nothing can shrink back and
  move a label the other way.
- **Forward labels are guessed.**  A label further on in the section is taken to have moved at least as far as the
  last label bound in this pass, since lengths only grow; most passes then see the right address the first time.
- **Errors only from the final pass**, and one a statement ([`CAsm65BaseAssembler::Error`]): an earlier pass may
  not know every value.  Warnings (a value's high bits dropped) are kept apart, [`CAsm65BaseAssembler::GetWarnings`].
- **Labels bind at the next statement**: after an instruction has aligned the address, a `.org` moved it or an
  `.align` padded it, so a label is where the next thing is.  A label just before `.section` stays in the section
  it was written in, at its end (2026-11-16): a vtable's label is written after its offsets and header with nothing
  after it, and was bound to the next section, so its words were read from the wrong place.

### Values in an object

In an object a label is an offset in its section, not an address.  A [`SAsm65Value`] counts how many section
addresses (or externals) were added into it, `iTerms`: 0 is a constant, 1 an address the linker places, and
anything else (two labels added, a label multiplied or shifted) can't be written.  Two labels of one section
subtract to a constant.  Each statement counts the section addresses its expressions made and the ones a
relocation or a constant took ([`CAsm65BaseAssembler::UseTerm`]); any left over is "A section address cannot be
used here".  `Label >> 16` and `Label & $FFFF` are kept as a form of the address ([`EAsm65RelocationForm`]), for the
W65C816's banks.

The program counter C is never a value: a value counts it separately (`iProgramCounter`), so `C - 2` is an offset
and can only be a branch target.  The user ruled that C is the processor's real program counter and "never assemble
C as a value computed at assembly time, that will make relocatable code confusing" (2026-10-23).

[`CAsm65BaseAssembler::RecordRelocation`] writes a relocation for each field the linker must fill:

- A local label is relocated by its section; an external, `.global` or `.weak` one by its name, so the definition
  another object gives, or the COMDAT copy the linker keeps, is the one used.
- A relative field's addend has the distance from the field to the next instruction folded in, so the linker only
  subtracts the field's own address.
- A name used in a pass and never defined becomes an external after it ([`CAsm65BaseAssembler::MarkExternals`]);
  `.extern` is optional.  That needs another pass, as an external is always assembled at 32 bits.
- An address the linker places is 32 bits wide unless written `:16` (the user, 2026-10-23); the linker checks a
  narrower field holds it.  A branch to another section or object is a 32 bit relative field; one in its own
  section is an ordinary offset.

The W65C816's own: a label not yet defined in `[Label]` is absolute, relocated *in bank* (the linker's error if the
label is in another bank), and `:24` asks for long; after `.relax` a branch is long when it was in an earlier pass,
is to another section, or is known to be too far ([`CAsmW65C816Assembler::IsLongBranch`]); widths are followed down
a section, top to bottom, not along jumps.  The rules themselves are in [`Assembler65Lib/AssemblerW65C816Syntax.txt`].

## Objects and libraries

A [`CAsm65Object`] is one assembled unit:

- **Sections** ([`SAsm65Section`]): a name whose start is its kind ([`EAsm65SectionKind`]: text, rodata, init, fini,
  data, bss; `text.Helper` is a text section of its own), bytes (none for bss), size, alignment, the COMDAT group,
  and the options: `keep`, `associated` (a section of the same object), `resident`, `at`.
- **Symbols** ([`SAsm65ObjectSymbol`]): every label and constant, locals too (for the map), local, global or weak
  ([`EAsm65Binding`]), in a section, absolute or undefined.
- **Relocations** ([`SAsm65Relocation`]): a field of 1 to 8 bytes, signed or not, relative or not, its form, and
  its target, a section or a symbol, plus an addend; with the line and column it was written at, for the linker's
  errors.
- **Source lines and fingerprints** (`.line`, `.fingerprint`), and **debug records** ([`CAsm65DebugRecords`]) at
  places in its sections.

The user ruled the file format on 2026-10-23: an object is a BaseLib chunk file, with chunks Header, Sections,
Symbols, Relocations and Sources, then the debug records' own chunks; a library is a pack file holding "Library"
(its name and how many objects) and each object's chunk file as "Objects/0", "Objects/1" and so on
([`CAsm65Object::Write`], [`CAsm65Library::Write`]).  The header is a magic name and a version, and reading wants
that version exactly (`ASM65_OBJECT_VERSION`, 6 since the debug records): nothing older is read, as every build
assembles its objects again.  [`CAsm65Object::Read`] calls `ReadClose` even when `ReadOpen` failed, as that is
what frees what a failed open allocated.

Two things are for the HCP65 kernal, which reads executables and modules with this class compiled for the W65C816:

- [`CAsm65Object::SetSkipSectionBytes`] leaves the sections' bytes in the file (where they are and how many), so
  the loader reads them straight into the process's banks: an executable can be bigger than the kernal's heap.
- Without `ASM65_DEBUG_RECORDS` (not defined when `__LIBC65__` is, 2026-11-23) an object has no debug records and
  reads none of them: they would cost the kernal 14.7 KB.

A [`CAsm65Library`] owns its objects.  The linker links a library's object only when it defines a name something
needs; objects added directly are always linked.

## The linker

[`CAsm65Linker::Link`] is called once, after the objects, libraries and settings are given.  In order:

| Step | Does |
|---|---|
| [`CAsm65Linker::Include`] | Each object added: its sections, its globals and the names it needs.  A COMDAT group's sections are kept from the first object that has the group; a later copy is dropped with its symbols.  A global defined twice is an error; a global replaces a weak; the first weak wins over a later one. |
| [`CAsm65Linker::MarkUsed`], [`CAsm65Linker::PullLibraries`] | With unused sections removed: the used sections marked from the roots, then the library objects the used sections need included, and again until nothing more comes in.  Otherwise the library objects any included object needs. |
| [`CAsm65Linker::FindMissing`], [`CAsm65Linker::AddStubs`] | The names needed (not only weakly) that nothing defines: errors, stubs or imports (below). |
| [`CAsm65Linker::PlanLazy`] | With lazy loading: the units and their trampolines. |
| [`CAsm65Linker::Layout`] | Sections with an address placed first ([`CAsm65Linker::LayoutAt`]), then each kind in order, each section where it flows to ([`CAsm65Linker::FlowAddress`]: aligned, not across a bank, round the placed ones).  Kinds that overlap or end past the limit are errors. |
| [`CAsm65Linker::DefineLinkerSymbols`], [`CAsm65Linker::ResolveSymbols`] | `__<kind>_start`, `__<kind>_end` and `__end`, unless an object defines them; then every global's address. |
| [`CAsm65Linker::Relocate`] | Each placed section's bytes copied, its fields written, the copies written to the output in address order.  The objects aren't changed, so they can be linked again ([`CCPP65Builder::LinkWith`] links the same compiled objects with one source after another). |
| [`CAsm65Linker::WriteMap`], [`CAsm65Linker::BuildDebugInfo`] | The map and the debug info. |

### 32 bits, and `.org` absolute only

The linker's addresses are 32 bits: a section ending past `$FFFFFFFF` is an error.  That covers both processors,
the W65C816's 24 bit addresses and the 65C64's 32 bit absolute addresses.  `.org` is only for absolute mode
([`CAsm65BaseAssembler::Assemble`]); an object places nothing itself, and `.section name, at address` is how one
asks for an address (the W65C816's vectors and their stubs).  The user ruled both on 2026-10-23.

### Placing sections

The kinds go text, rodata, init, fini, data then bss, text at `$10000` ([`CAsm65Linker::SetAddress`] moves any
kind), each kind starting where its first section goes, so an init or fini table starts on its first entry.  A
section's alignment is the largest `.align` in it, and 2 once it holds a 65C64 instruction.  bss has no bytes in
the output: a start up or a loader clears it from `__bss_start` to `__bss_end`.

With a bank size ([`CAsm65Linker::SetBankSize`], the W65C816's `$10000`) a section no bigger than a bank is moved
to the next bank rather than cross the end of one, code bigger than a bank is an error (the program counter wraps
in its bank), and only a section with an address may be in bank 0: the user ruled on 2026-10-25 that bank 0 is the
stack and the direct page, with the vectors and their `jml` stubs.

### Removing unused sections

The compiler gives every function, global, static local and string literal its own section, so the linker can
leave out what nothing uses ([`CAsm65Linker::SetRemoveUnused`]).  The roots are the `keep` sections, the init and
fini sections that aren't associated with another, and the sections defining the names given to
[`CAsm65Linker::KeepSymbol`] (an executable's or a module's entry, which only its loader calls).  Each used
section's relocations mark what they reach; an associated section is used when its partner is.  A global's init
and fini entries are associated with its data, so a global nothing uses isn't constructed.

The user asked for it on 2026-10-23, warning that "a lot of global data is going to look like it is used when it
isn't".  Two things follow from marking before pulling libraries: a library object is linked only for a name a
*used* section needs, and a name only removed sections need isn't missing.

[`CAsm65Linker::ForbidObject`] makes any used section that uses what a named object defines an error naming the
section and the symbol (2026-11-06): a kernal is built with no floats, and a float that creeps in is found at its
use, not at run time.

### Missing names

By default a name nothing defines is an error naming the object that used it.  [`CAsm65Linker::SetStubMissing`]
makes each the address of one shared stub instead, a stop instruction in a text section of the linker's own:
the 65C64's `stp`, assembled by a [`CAsm65C64Assembler`], or the byte [`CAsm65Linker::SetStubOpcode`] gives (the
W65C816's `stp` is `$DB`; the 65C64's bytes would run as something else on it).  The compiler links this way so a
test can run a program that uses something it doesn't provide; `(linker stubs)` in the map is where such a
program stops.  [`CAsm65Linker::DescribeMissing`] says why an entry has no address: not defined, or removed as
unused (2026-11-13).

### Relocatable links and modules

[`CAsm65Linker::SetRelocatable`] (2026-11-06) links for a loader that may put the link anywhere: a missing name is
an *import*, never an error or a stub, and each field that changes when the link moves is kept as a fixup
([`SAsm65Fixup`]): an absolute field whose target is in the link, or any field whose target is an import (written as
zeros, for the loader).  A relative field to a place in the link needs none.  [`CAsm65Module`] makes the link into
a kernal driver.

### Lazy loading

For a target with less memory than code, [`CAsm65Linker::SetLazy`] (2026-10-23) makes each placed text section a
*unit* on a drive, loaded the first time it is called and never unloaded: the user ruled "Dont drop units, once
loaded the code stays loaded".  Everything outside a unit refers to it through a resident trampoline,
`jmp [slot]`, whose slot first points at `R_LazyLoad`, a loader the linker writes in 65C64 assembly and assembles
itself ([`Assembler65Lib/Asm65LinkerLazy.cpp`]).  A field loaded with a unit must be a plain 32 bit value.  The
drive is the simulator's, [`CAsm65Memory::SetDrive`]'s registers at `00FF:0000`, which the user chose to start from
(the one the FAT32 tests use).  [`CCPP65Builder::SetLazy`] builds a C++ program this way.

## Outputs

[`CAsm65Linker::GetOutput`] is the program as segments of bytes at addresses ([`CAsm65Output`]), as absolute mode
makes.  A build makes one of three files from it:

| Class | Is |
|---|---|
| [`CAsm65Image`] (2026-11-05) | A file of a fixed size, each segment at its region's offset, the rest the fill: a ROM, or the kernal the boot loader reads. |
| [`CAsm65Executable`] (2026-11-05) | A program a loader places at its linked addresses: an object with one section a segment, nothing relocated, an empty bss section, and absolute symbols (the entry and the configuration's, such as `UserHeapSize`). |
| [`CAsm65Module`] (2026-11-06) | A relocatable link a loader puts anywhere in one bank: an object with one section, its fixups as relocations to that section or to imports, its entry and init table bounds as offsets. |

An executable and a module are [`CAsm65Object`] files so the kernal needs one reader for both, and the format, its
validation and its writing are the linker's already.  Each is described in a build configuration as an XML element
([`CAsm65Image::LoadXML`], [`CAsm65Executable::LoadXML`], [`CAsm65Module::LoadXML`]), with numbers written in
hexadecimal and read in either ([`Assembler65Lib/Asm65Attributes.h`]); `Limit` on each is the highest address the
linker may place anything at.

## Debug info

[`CAsm65Linker::BuildDebugInfo`] makes a program's [`CAsm65DebugInfo`]: its [`CAsm65LineTable`] and its
[`CAsm65DebugRecords`], written beside the program as `<program>.debug` (2026-11-23, in place of `.lines`).

- Each placed section's source lines at their addresses, and a line of 0 (no line) at the end of each section that
  has any, so code after it isn't taken for its last line.  An assembly file's lines that write bytes are source
  lines too ([`CAsm65BaseAssembler::SetSourceFile`]), so assembly can be stepped like C++.
- The fingerprints of the objects with a placed section: a hash of each function's code and its compile time, so a
  profile or a debugger can tell whether it is still the same code.
- Each object's records at its placed sections' addresses; the records of anything not placed are left out, and a
  type is kept once however many units describe it, by its key.

A module's debug info is moved to where its loader put it ([`CAsm65DebugInfo::Move`]).  The directives and what a
debugger reads from them are in [Linker and Outputs](../linker.md#debug-info) and
[Debugging](../../ide/debugging.md); the records are still being extended by the debugger's plans in HCP65-IDE
([`HCP65IDE/DebuggerPlan.txt`] and those beside it).

## The rest of the library

Assembler65Lib also holds what runs and checks assembled programs, used by the tests, the emulators and the
debugger:

| Class | Is |
|---|---|
| [`CAsm65Memory`] | Flat memory for a 65C64 program, with devices in the page `00FF:0000`: the drive, a console, host files ([`CAsm65HostFiles`]) and the memory size. |
| [`CAsm65C64Lockstep`], [`CAsmW65C816Lockstep`] | A program on the fast emulator and the simulator side by side, to the first difference. |
| [`CAsm65FrameCheck`], [`CAsm65C64FrameCheck`], [`CAsmW65C816FrameCheck`] | The calls a program makes, followed a step at a time: the truth a stack walk is checked against; on the W65C816, stores into a caller's frame. |
| [`CAsm65StackWalker`], [`CAsm65C64StackWalker`], [`CAsmW65C816StackWalker`] | The calls a stopped processor is in, from its registers, memory and debug records. |
| [`CAsm65Profile`], [`CAsm65ProfiledCore`] | A profile of a run on either processor: functions, lines, call sites; the W65C816 back end's aligned frames and the inliner read it. |
| [`AsmW65C816Disassemble`] | W65C816 code back to the assembler's syntax. |

## Tests

TestAssembler65Lib (`HCP65/Tests/TestAssembler65Lib`) has encoding, error, object, linker, lazy, image and stack
walker tests, and whole programs in `Input/*.asm` (a sieve, queens, quicksort, SHA-256, Life, Mandelbrot and
others, described in [Worked Examples](../../65c64/examples.md)) run on the emulator and checked against the same
calculation in C++.  The expected encodings were worked out by hand from the instruction sheet.  It also assembles
every `asm` block on the site's 65C64 pages.

## Decisions

- The assembler: CppParserLib's structure, the 65C64 only at first, a working assembler not a skeleton (the user,
  2026-10-23).  Its syntax was approved by the user the same day, ruling by ruling; they are in
  [`Assembler65Lib/Assembler65C64Syntax.txt`] and not repeated here.
- The W65C816 assembler in the 65C64 assembler's style, writing the same objects for the same linker (the user,
  2026-10-25, `W65C816.txt` question 4); its rulings in [`Assembler65Lib/AssemblerW65C816Syntax.txt`].  So one base
  class has everything but the instructions.
- A processor's name in everything that is only that processor's; plain `Asm65` for what both share (the user,
  2026-10-31; the W65C816 spelling from 2026-11-13).
- C is the real program counter and never a value (the user, 2026-10-23), so nothing relocatable is fixed at
  assembly time.
- The linker in Assembler65Lib; addresses it places assembled at 32 bits with `:16` the opt-in, range checked by
  the linker; `.org` for absolute mode only; an object a chunk file of sections, symbols and relocations, a library
  a pack file of objects (the user, 2026-10-23).
- Unused sections removed, with the caution that much global data will look used (the user, 2026-10-23); libraries
  pulled only for used sections.
- Lazy units are never dropped once loaded (the user, 2026-10-23), tested on a simulated drive first.
- Bank 0 holds only the stack, the direct page, the vectors and their stubs (the user, 2026-10-25): only sections
  with an address go there.
- The source read a line at a time, so an assembly is never all in memory (2026-10-31).
- No debug records in an object compiled for the HCP65: 14.7 KB of kernal (2026-11-23).
- My choices of 2026-10-23, never ratified by the user and kept since: implicit externals; a section's kind from its
  name; the `comdat` syntax; bss writes no bytes, so a start up clears it; one shared stub for every missing name.
  A label before `.section` was first bound in the new section, as with `.org`; since 2026-11-16 it binds in the
  one it was written in (the vtable bug above).

## Not done

- The 65C64 assembler's mnemonic table is its own, not shared with MicroprocessorLib's [`C65C64InstructionFactory`]
  (noted 2026-10-23, left for the user).
- Lazy loading is the 65C64's only (its loader is 65C64 code) and has only the simulator's drive: the storage, RAM
  sizes and operating system it would run under were never decided.
- Objects and libraries aren't kept between builds: every build compiles and assembles every unit again.
- Not ruled by the user: `bra` and `brsr` following the branch rule (a label, or a raw offset), and a round bracket
  group of one operand being an error (my reading).
- [`Assembler65Lib/W65C816Names.txt`]'s two open items.
