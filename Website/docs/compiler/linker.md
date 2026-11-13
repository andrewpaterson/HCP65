# Linker and Outputs

Both processors' assemblers write the same object format (`CAsm65Object`), and one linker
(`CAsm65Linker`) links them.  Sections, symbols, relocations, COMDAT, libraries, removing unused
sections and lazy loading are described in [Objects and Linking](../65c64/linking.md); they work
the same for the W65C816.  This page is how the compiler uses the linker, what is different for
the W65C816, and the files a link writes.

## What a compiled unit gives the linker

Each unit is one object.  Each function, global, static local and string literal is a section of
its own, named after its label (`text._Sum__FPKst`, `data._gaiValues`, `rodata.Str0`), so the
linker can leave out whatever nothing uses.  The compiler turns on removing unused sections for
every program.

- Functions and globals with external linkage are `.global`; static ones are local to the object.
- Inline and template functions, vtables, thunks and runtime routines are written by every unit
  that uses them, each in a COMDAT section, and the linker keeps one copy.
- A global that needs a constructor has an entry in an `init` section, and one that needs a
  destructor an entry in a `fini` section.  Those sections are associated with the global's data,
  so a global nothing uses is not made at all, even if its constructor does something else.
  `__attribute__((used))` keeps it.

Libc65 and the runtime are linked as libraries: only the units a program needs are linked.

## The linker's symbols

The linker places sections by kind, `text`, `rodata`, `init`, `fini`, `data` then `bss`, and defines
`__<kind>_start` and `__<kind>_end` for each kind, and `__end` after the last.  The start up uses
them to clear the bss and walk the init and fini tables.

## The W65C816

With a bank size (`SetBankSize`, `$10000` for the W65C816):

- A section no bigger than a bank never crosses the end of one.
- Only sections placed at an address (`.section name, at address`) may be in bank 0, which is for
  the stack, the direct page, the vectors and their stubs.
- 16 bit addresses are relocated *in bank*: the linker writes a label's low 16 bits, and it is an
  error unless the label is in the bank of the instruction using it.
- `Label >> 16` and `Label & $FFFF` are relocations of their own, for a label's bank and offset.

A program that calls a runtime routine (`R816_...`) Libc65 doesn't define fails to link, rather
than the call being turned into a stop.

## Outputs

A build writes one of three kinds of file, or only compiles.

### Executable

A program a loader places: the HCP65 kernal loads `boot.exe` and the programs it runs.  It is an
object file holding one section for each linked segment at its address, nothing relocated, an
empty bss section (`__bss_start` to `__bss_end`) for the loader to clear, and absolute symbols:
the entry point, and the symbols the configuration gives, which the loader or the program reads.

```xml
<Executable File="boot.exe" Entry="UserStart" Limit="0x200000">
	<Symbol Name="UserHeapSize" Value="0x20000"/>
</Executable>
```

### Image

A file of a fixed size, loaded whole: a ROM.  Each segment goes where its region says, and bytes
nothing goes in are the fill (erased flash is `$FF`).  A segment not wholly inside one region is
an error.

```xml
<Image File="Kernal.rom" Size="0x80000" Fill="0xFF" Limit="0x7FC00" Load="0x0">
	<Region Start="0x0" End="0x40000" Offset="0x0"/>
</Image>
```

`Load` is where the IDE's debugger writes the image into the emulator.

### Module

A relocatable link a loader can put anywhere in one bank: an HCP65 kernal driver.  It holds one
section, from the start of its text to the end of its data, with its size taking in its bss.  Its
relocations are the fixups the loader applies when it moves the module: a place inside the module,
or an *import*, a name nothing in the module defines that the loader finds among the kernal's
exports.  Its symbols are the entry point and the bounds of its init table.

```xml
<Module File="UART.DRV" Entry="_KernalCreateDriver" Limit="0x20000"/>
```

`Limit`, on all three, is the highest address the linker may place anything at.

## Line tables

A program built for debugging has a line table beside it (`hello.exe.lines`): for each address,
the source file and line its code is for, and a fingerprint of each linked function.  The compiler
writes them into the assembly with two directives:

| Directive | Means |
|---|---|
| `.line "file", line` | The code from here is for this line of this file. |
| `.fingerprint "label", hash, time` | The function's code hashed, so a debugger can tell whether the function changed since the line table was made. |

The HCP65 IDE's debugger and the emulators' profilers read the line table to show the source for an
address.

## Missing names

By default a name nothing defines is an error that says which object used it.  With
`SetStubMissing(true)` it becomes the address of a stub that stops the processor (`stp` on the
W65C816): the compiler's tests link this way so they can run a program that uses something they
don't provide.  When a W65C816 program stops unexpectedly, look for `(linker stubs)` in the map
first.
