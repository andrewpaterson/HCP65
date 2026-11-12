# Objects and Linking

A program of more than one source file is assembled into *objects* and joined by the *linker*.
An object (`CAsm65Object`) holds one source file's sections, symbols and relocations.  The
linker (`CAsm65Linker`) places the sections at addresses, resolves the symbols between objects
and writes the bytes.

The same object format and linker serve the W65C816 assembler.

## Assembling an object

In an object there is no `.org`: code and data go into named sections, and the linker decides
where each one ends up.

```asm
	.section text
	.global Main
	.extern Square

Main:
	cpy A, 7
	jsr Square
	cpy [Result], A
	ret

	.section data
Result:
	.long 0
```

| Directive | Meaning |
|---|---|
| `.section name {, option}…` | Assemble what follows into this section |
| `.global name, …` | The name is visible to other objects |
| `.weak name, …` | As `.global`, but a global definition in another object wins.  An undefined weak name is 0. |
| `.extern name, …` | The name is defined by another object |

`.extern` is optional: any name that's used but not defined is an external.  `.org` is only
allowed in absolute mode, and these four directives only in an object.

## Sections

An object starts in the section `text`.  A section's name starts with its kind, and anything
after a dot makes it a section of its own: `text.Helper` is a text section apart from `text`.

| Kind | Holds |
|---|---|
| `text` | Code |
| `rodata` | Constant data |
| `init` | Addresses of functions to call before the program starts |
| `fini` | Addresses of functions to call after it ends |
| `data` | Data |
| `bss` | Data that starts as zeros.  It has a size but no bytes, so it takes only labels, `.space` and `.align`. |

The linker places the kinds in that order, each after the one before, starting with `text` at
`$10000`.  A section is aligned to the largest `.align` in it, and to 2 bytes once it holds an
instruction.

| Option | Meaning |
|---|---|
| `at address` | Place the section at this address.  The linker places these first and the others flow around them. |
| `keep` | Never remove the section as unused |
| `comdat` | The section is one copy of something several objects may define (see [COMDAT](#comdat)) |
| `comdat group` | The same, naming the group |
| `associated section` | The section is used only when the section it names is used |
| `resident` | With [lazy loading](#lazy-loading), the section stays in memory |

```asm
	.section text.Vectors, at $FFFFFFFC, keep
	.long Reset

	.section text
Reset:
	stp

	.section bss.Buffer
Buffer:
	.space 256
```

## Addresses in an object

Inside an object a label is an offset in its section, not yet an address.  That limits what an
expression can do with one:

- A value can be a constant, or one label plus or minus a constant.
- Two labels in the same section subtract to a constant: `End - Start`.
- Any other use of a label (multiplying it, shifting it, combining labels of two sections, an
  embedded value) is an error.

Where a label is assembled:

| Used as | Size |
|---|---|
| An immediate or an absolute address | 32 bits.  A suffix (`:16`, `:s16`, `:8` … `:64`) gives another width and the linker checks the value fits. |
| A branch, `bra` or `brsr` target in the same section | An ordinary offset |
| A branch, `bra` or `brsr` target in another section or object | A 32 bit relative offset |
| `.byte`, `.word`, `.long` or `.quad` | The directive's width |

So in an object `cpy A, [Table]` is always the three word absolute form, unless it's written
`cpy A, [Table:16]`.

## Relocations

Each place that holds a label gets a relocation.  The linker writes the target's address plus
an addend into a field of 1 to 8 bytes, little endian.  A relative relocation subtracts the
field's own address.

Labels local to an object are relocated by section.  External, `.global` and `.weak` names are
relocated by name, so the definition another object gives (or the kept COMDAT copy) is the one
used.

## Symbols

- A global defined twice is an error.
- A global replaces a weak definition, and the first weak definition wins over a later one.
- A name nothing defines is an error.

The linker defines these names itself, unless an object defines them:

| Name | Value |
|---|---|
| `__text_start`, `__text_end` | The start and end of the text sections |
| `__rodata_start`, `__rodata_end` | The start and end of the rodata sections |
| `__init_start`, `__init_end` | The start and end of the init table |
| `__fini_start`, `__fini_end` | The start and end of the fini table |
| `__data_start`, `__data_end` | The start and end of the data sections |
| `__bss_start`, `__bss_end` | The start and end of the bss sections |
| `__end` | The end of the program |

## Start up

Each `init` section holds `.long` addresses of functions to call before the program's main
function, and each `fini` section functions to call after it.  The linker places them in link
order between `__init_start` and `__init_end` (and `__fini_start` and `__fini_end`).  The start
up code calls the init table forwards and the fini table backwards.

```asm
	.section text
MakeTable:
	ret

	.section init.MakeTable
	.long MakeTable
```

## Removing unused sections

The linker can leave out the sections nothing uses.  It starts from the sections that must
stay (`keep` sections, and `init` and `fini` sections that aren't associated) and follows their
relocations to every section they reach, and on from those.  Everything else is removed.

An `associated` section is used when the section it names is used.  That is how a global's
`init` entry goes when nothing uses the global:

```asm
	.section data.gTable
gTable:
	.space 64

	.section text.MakeTable
MakeTable:
	ret

	.section init.MakeTable, associated data.gTable
	.long MakeTable
```

Give each function and each global a section of its own for the most to be removed.  A name
that only removed sections need isn't missing, and a library's object is linked only for a name
that a used section needs.

## COMDAT

A `comdat` section is one copy of something that several objects may each define, like an
inline function.  The linker keeps the group's sections from the first object that has them and
drops the others, with their symbols.

```asm
	.section text.Inline, comdat
	.global Inline
Inline:
	ret
```

Refer to a COMDAT section's labels by a global name.  Code that reaches a dropped copy through
a local label is an error.

## Libraries

A library (`CAsm65Library`) is a set of objects.  The linker links a library's object only when
it defines a name that's needed, where objects added directly are always linked.

## Lazy loading

The linker can make a program whose code is loaded from a drive as it is first called.  Each
text section becomes a *unit*, kept on the drive.  Data, `rodata`, `init` and `fini` sections,
`resident` sections and the objects added as resident stay in memory from the start: the
*core*.

- Every reference to a unit from outside it (a call, a function pointer, a vtable) goes to a
  small resident trampoline: `jmp [slot]`.
- The slot first points at code that loads the unit.  The loader (`R_LazyLoad`, written by the
  linker) reads the unit's sectors into memory after the core, applies its relocations, points
  the slot at the code and jumps to it.
- Later calls go through the slot straight to the code.  A unit is never unloaded.
- References inside a unit stay direct.

The loader sets A, B, X and Y to 32 bits and keeps their values.  Code that is called with wider
registers must be resident.  A load that fails stops the processor with A = -1 and
`R_LazyError` set to 1 (no room) or 2 (the drive failed).

## Using the linker from C++

```cpp
CAsm65C64Assembler	cAssembler;
CAsm65Object		cObject;
CAsm65Linker		cLinker;

cObject.Init("main");
cAssembler.Init();
cAssembler.AssembleObject(szSource, &cObject);
cAssembler.Kill();

cLinker.Init();
cLinker.AddObject(&cObject);		//Always linked, in order.
cLinker.AddLibrary(&cLibrary);		//Members linked only when needed.
if (cLinker.Link())
{
	cLinker.GetOutput();		//Segments of bytes at addresses, as from absolute mode.
	cLinker.GetMap();		//Sections, removed and dropped sections, symbols.
}
cLinker.GetErrors();			//"object:line:column: message 'detail'" lines.
cLinker.Kill();
cObject.Kill();
```

| Setting | Meaning |
|---|---|
| `SetAddress(kind, address)` | Where a kind of section starts (text is at `$10000` otherwise) |
| `SetLimit(address)` | Ending past this address is an error |
| `SetRemoveUnused(true)` | Remove unused sections |
| `SetStubMissing(true)` | A name nothing defines becomes the address of one `stp`, not an error |
| `SetLazy(codeEnd, sectorSize, firstSector)` | Lazy loading: units load below `codeEnd` from the drive's sectors |
| `SetBankSize(size)` | Keep sections from crossing a bank (for the W65C816) |

An object is saved and loaded with `CAsm65Object::Write` and `Read`, and a library with
`CAsm65Library::Write` and `Read`.
