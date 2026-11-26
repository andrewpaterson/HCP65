# Libc65 Design

[Libc65](../libc65.md) says what the C library has and [W65C816 Runtime](../../hcp65/runtime/w65c816.md) what
the compiler's runtime routines do.  This page says why they are built the way they are: the soft float and string
routines in assembly, the maths tables and the runtime's split, the heap, and BaseLib's FAT code in W65C816
assembly, with what was measured on the way and what was tried and dropped.  The compiler's side is in the sibling
pages: [W65C816 Back End Design](w65c816.md) (the runtime's calling conventions, `int` and [`size_t`] widths),
[65C64 Back End Design](65c64.md) and [IR Design](ir.md).  The allocators themselves are BaseLib's
([Memory](../../libraries/baselib/memory.md)).

The maths tables were planned in [`Libc65/MathTbls/MathTables.txt`].  It is finished and folded into this page; it
is kept as the record [`CppCompilerLib/W65C816.txt`] cites.

## C++ for reference, assembly for speed

Every unit that is hand written in assembly keeps its C++ as the reference: the C++ compiles natively, and the tests
run the same cases through the assembly on the emulators and through the C++ on the host and compare the results bit
for bit.  The C++ compiles to nothing where the assembly replaces it:

| C++ | Left out when | Replaced by |
|---|---|---|
| [`Libc65/softfloat.cpp`] | `__65C64__`, or `__W65C816_ASSEMBLY__` | [`Libc65/softfloat.asm`], [`Libc65/softfloatW65C816.asm`] |
| [`Libc65/string.cpp`] (all but [`strstr`]) | `__65C64__` or `__W65C816__` | [`Libc65/string.asm`], [`Libc65/stringW65C816.asm`] and [`Libc65/stringW65C816_16.asm`] |
| [`Libc65/math.cpp`]'s bit functions ([`floor`], [`fmod`] ...) | `__W65C816_ASSEMBLY__` | [`Libc65/softfloatW65C816.asm`] |
| BaseLib's FAT units | `__W65C816_ASSEMBLY__` | [`BaseLib/FatVolumeW65C816.asm`] and its neighbours ([below](#fat16-and-fat32-in-w65c816-assembly)) |

On the W65C816 a unit's assembly is the file beside it named `XW65C816.asm`, which [`CCPP65Builder::AddAssemblyUnit`]
adds next to `X.cpp`; the target then defines `__W65C816_ASSEMBLY__`.  [`CCPPCompileOptions::SetAssemblyUnits`] false
builds the C++ instead (`TEST_PROJECT65_CPP=1` in TestBaseLib65), so the C++ can be measured and checked on the target
too.  [`strstr`] stays C++ on both processors, written on [`memchr`] and [`memcmp`] so it runs at their speed.

The W65C816's Libc65 units are listed by the target ([`CCPPTargetW65C816::GetLibc65Units`]), its runtime units by
[`CCPPTargetW65C816::GetRuntimeUnits`] and its tables by [`CCPPTargetW65C816::GetMathTableUnits`]; the 65C64 target
lists none, so a 65C64 build names its Libc65 units itself.  Every unit is linked as a library and every routine is
in a section of its own, so a program carries only what it calls.  On the 65C64 no routine jumps or branches into
another section: a lazily loaded section is only ever entered by a call, with the registers 32 bits wide.

## Soft float on the 65C64

The user asked (2026-10-23) to "keep softfloat as a reference but implement IEEE 754 single floats in 65C64
assembly, optimise for speed", then for the doubles.  [`Libc65/softfloat.asm`] has every `SoftFloat` and `SoftDouble`
function, bit exact with [`Libc65/softfloat.cpp`], and works with whole significands in the 64 bit registers:

- A float product is one `mul` (24 bit significands make 48 bits) and a quotient one `div`.  A sum is exact: the
  smaller significand is shifted only out of the larger's spare low bits, and a sticky bit keeps what goes.
- A double product is four 32 bit `mul`s (the top 64 bits of the 106 bit product, and whether the rest is 0).  A
  double quotient is one `div` with the dividend's top half in B: the user ruled (2026-10-24) that a B that isn't 0
  when `div` starts is defined, the top of the dividend, below A; the divide went from 206 to 105 cycles.
- A square root starts from a seed table (`rodata.SoftFloatSqrtTable`) and takes Newton steps by `div`, then a
  multiply to correct the last bit.
- Every routine that rounds shares one section with `RoundPack` (and `DRoundPack` for doubles), so they reach it
  and the special cases with short branches; the hot routines have its usual path in line.  A branch 2 to 8 bytes on
  is one word, further two words taken or not, so rare cases (special values, overflow) are a short branch over a
  `bra` to them.

The result, for normal operands on the simulator, is 25 to 220 cycles a call, 20 to 96 times the compiled C++'s
speed (the table is at the top of [`Libc65/softfloat.asm`]).

## Soft float on the W65C816

On 2026-10-30 the W65C816 was found running the compiled [`Libc65/softfloat.cpp`], and the user asked for
[`Libc65/softfloatW65C816.asm`].  With no 32 bit registers it follows [`Libc65/softfloat.cpp`]'s steps, its 64 bit
integers as four words in the direct page: each operand is unpacked to a sign, a class, the exponent of its lowest
bit and a 64 bit significand, and the two sides are 14 bytes apart so one routine given X works on either.  Writing
it took TestSupportLib65 on the W65C816 from 54.2G cycles to 41.0G (TestSound 5.59G to 1.67G).

Four rounds of tuning followed (2026-10-30 to 2026-10-31; the last at the user's "fastest 816 softfloat ever", using
the tables):

- **The frame is on a page.**  Each function points D at locals rounded down to a page, so every direct page access
  is a cycle cheaper, for up to 255 bytes more stack.
- **Multiply by table rows.**  For each byte b of one significand a 24 bit pointer to MathMult's row for b
  (`MathMult + b * 512`); the other significand's bytes index it, and the products are added straight into the
  128 bit product.  For two normal 7 byte double significands only the columns that can reach the kept bits are
  multiplied; the low columns are added only when bits 42 to 49 of the product are all 1s, which a carry from below
  could change (the pairs in [`TestLibc65/TestLibc65SoftFloat.cpp`] prove that fallback).
- **Divide a byte at a time.**  Radix 256 long division: each quotient byte is estimated from the remainder's top two
  bytes and the reciprocal of the divisor's top 16 bits from INVERT's upper half (`MathInvertHigh`), as
  `q = t2 + ((t1 + t2 * r1 + 104) >> 8)`.  Right 80% of the time and otherwise one out, corrected by adding or taking
  the divisor back once; a fuller estimate right 99.86% of the time cost about 85 cycles a step more than the
  corrections it saved.
- **Rounding where the top bit is known.**  `RoundDoubleAt` and `RoundFloat31` (a float's top bit at 31: the kept
  bits are the top 3 bytes, no shift) instead of the general `RoundPack`.
- **Raw fast paths**: compares from the raw bits, float to double and back for normal values, multiplies and divides
  by powers of two, and the integer conversions.
- **The bit functions of `math.h`** ([`fabs`], [`sqrt`], [`trunc`], [`floor`], [`ceil`], [`round`], [`frexp`],
  [`ldexp`], [`modf`], [`fmod`] and their `f` and `l` forms) are in the same file on the W65C816, bit exact with
  [`Libc65/math.cpp`]'s.

Cycles a call after the last round (2026-10-31, TestLibc65's averages, about 350 of each being the test's loop):

| Operation | Before the tables | With them |
|---|---|---|
| Double multiply | 3365 | 2278 |
| Double divide | 5159 | 2998 |
| Double square root | 6218 | 5118 |
| Float multiply | 1520 | 1354 |
| Float divide | 2055 | 1826 |

The compiler calls the frame operand entries (`name.dp`), which take the operands' places in the caller's frame in X
and Y ([W65C816 Back End Design](w65c816.md#the-runtimes-conventions)).  Tried and not taken: a float square root by
Newton steps (no better than the bit loop), and [`fmod`] a byte at a time.

## The string routines

The user asked (2026-10-24) for "assembler optimised" string functions as the soft float had been.

**65C64.**  [`Libc65/string.asm`] has all of `string.h` but [`strstr`], each one or two of the processor's repeating
block and vector instructions: `vcpy` copies 8 bytes a step, `vcmp` compares 8 bytes a step until two differ,
`btst` finds a string's zero and `bcmp` a byte, `bcpy` fills.  For 1000 bytes [`memcpy`] is 1184 cycles against the
compiled C++'s 15093, [`strcmp`] 3212 against 60081; TestStandardLib65 went from 294 s to 205 s.  The compiler also
copies a constant [`memcpy`] of up to 64 bytes in place (`CPP65_INLINE_MEMCPY`, [`CCPPBuiltins::IsInlineMemcpy`]).

**W65C816.**  The moves use `mvn` and `mvp`, 7 cycles a byte.  Their source and destination banks are operand bytes
of the instruction, so each move writes them into its own code first: the code runs in RAM, and the user allowed it
(2026-10-29).  It makes the routines non-reentrant, so an interrupt handler mustn't call them.  A move stops at the
end of the source's or the destination's bank, since X and Y wrap there, and the next goes on from there.  Two files,
because [`size_t`] is 16 bits ([W65C816 Back End Design](w65c816.md#widths-int-size-and-lsize)):

- [`Libc65/stringW65C816_16.asm`] has [`memcpy`], [`memmove`], [`memset`], [`memcmp`] and [`strlen`] with 16 bit
  counts: what C and BaseLib's natural arrays call.
- [`Libc65/stringW65C816.asm`] has the 32 suffixed ones ([`memcpy32`] ...) and the other string functions; [`memchr`],
  [`strncpy`], [`strncat`] and [`strncmp`] are 16 bit entries into their 32 bit bodies.

Most of BaseLib's copies are a few bytes, for which the block move's frame and set up would cost about 600 cycles:
up to 32 bytes are copied by words instead (about 105 cycles for 4).  [`memset`] within one bank stores the value's
word 16 bytes at a time with no frame; otherwise it stores the first byte and moves the rest up from one below it.
The same files have BaseLib's [`MemSwp`] and its bit array finders ([`FindFirstSetBit`] ...), which share their
tables and code.

## The maths tables and the runtime

On 2026-10-29 the user added 17 of Garth Wilson's 16 bit maths tables (Intel HEX, 2112KB in all) to
`Libc65/MathTbls`, to speed up the W65C816's multiplies and divides.  Their formats, checked against every entry:

| Table | Size | Entry |
|---|---|---|
| MULT | 128KB | word `[a << 8 \| b] = a * b` for bytes a and b |
| SQUARE | 256KB | long `[i] = i * i` |
| INVERT | 256KB | long `[i] = round(2^32 / i)`; `[0]` and `[1]` are `$FFFFFFFF` |
| ATAN | 64KB + 2 | word `[i] = round(atan(i / $8000) * 65536 / 2pi)` |
| LOG2 | 128KB | word `[i] = round(log2(i) * 4096)`; `[0]` is ATAN's last word, Garth's shared word, kept |

The user's rulings that day: multiply first, then divide; a table is linked only if it is used, and lazy loading of
tables was dropped; [`CIntelHex`] goes in BaseLib (the assembler's `.inchex` reads the files with it); there is only
512KB of ROM for everything, so the trig and log tables stay in the file system and are not linked.

**Multiply.**  [`Libc65/MathTbls/MathMult.asm`] starts MULT on a bank, so bit 7 of a picks the bank and
`(a & $7F) << 9 | b << 1` is a 16 bit index.  A 16 bit product is three lookups (only the low bytes of the cross
products count), a 32 bit one up to ten, skipping zero bytes.  The 6502.org thread on fast multiplication was read:
its fastest method is the same 128KB product table.  The table multiply, the faster divide loops (below) and the
runtime's operands in the caller's scratch bytes together took TestBaseLib65 on the W65C816 from 46.715G cycles to
44.887G (-3.9%, 2026-10-29).

**Divide.**  The plan was a reciprocal from a 128KB slice of INVERT, then one correction.  A 16 bit divide by the
reciprocal was built and was faster than the loop of the day, but the reciprocals have only 17 bits: a single pass
over a 32 bit dividend is out by up to about 5500, and two 16 bit halves need about 12 lookups each, about 1100
cycles against the loop's 1000 to 1170.  Then the loops were unrolled, with the quotient bit kept in the carry and
rotated in, and steps chosen by the divisor's size, and the unrolled loop beat the reciprocal everywhere (16 / 16
bits 177 to 110 cycles, 16 / 8 358 to 274).  The integer divides read no table; INVERT is the soft float's alone.

**Tables out of ROM: the split (2026-11-06).**  The user ruled that the HCP65's tables don't go in ROM: the start up
program loads them from the SD card (`\OS\Math\MULT.BIN` and `INVERT.BIN`) into main memory, and the kernal maps them
into every process at the same address ([`UserLib/UserMathTables.asm`]).  The kernal, its drivers and the start up
program run before the tables are there, so the runtime became Libc65's assembly in three files (`Common`,
`MathTables`, `MathSafe`), chosen by [`CCPP65Builder::SetMathTables`] (`MathTables="false"` in a `.hcp65build`).  Only
INVERT's upper half is loaded: nothing reads the lower half (the boot test passes with it all `$CC`), so the tables
are four banks and [`Libc65/MathTbls/MathInvert.asm`] makes the halves two sections, `MathInvert` and
`MathInvertHigh`, so a test program that links the tables links only the half it uses.

Later rounds on the runtime (2026-11-06): every divide leaves its remainder at D + 5, so `x / d` and `x % d` are one
call; the 16 and 32 bit divides take unrolled steps chosen by the divisor; a 16 bit multiply in a loop is the table
lookup in place.  TestStandardLib65 on the W65C816 went from 31.340G to 31.121G and TestBaseLib65 from 19.735G to
19.503G.  `TEST_IR816_RUNTIME_CYCLES=1` makes TestIR816BackendRuntimeRoutines print each routine's cycles.

## The heap

[`malloc`] is BaseLib's [`CTrueMallocator`] on the 65C64 and [`CBankMallocator`] over it on the W65C816 (the user
asked, 2026-10-24, for "a proper TrueMallocator (malloc) in BaseLib that Libc65 malloc can use").  Both are in BaseLib
and use nothing else from it, so [`Libc65/stdlib.cpp`] includes [`BaseLib/TrueMallocator.h`] and a program that
links Libc65 without the BaseLib project adds their units itself: `BaseLib/TrueMallocator.cpp`, and on the W65C816
`BaseLib/BankMallocator.cpp` and [`Libc65/stdlibW65C816.asm`] (the kernal call), as TestLibc65 does.
[`CCPPTargetW65C816::GetLibc65Units`] doesn't list them.

- The heap is made at the first [`malloc`], not by a global's constructor: another global's constructor may
  allocate first.
- `__HeapInit` and the allocation calls are `noinline`, one copy each: the kernal, which is in ROM, shares them
  with Libc65's start up.
- On the W65C816 a heap under the HCP65 kernal (its heap block at `00FF:0300` has the marker) asks for banks as it
  needs them and gives them back as they empty, by COP 13 (`__HeapSystemCall`, so Libc65 doesn't need UserLib), and
  reserves the maths tables' banks.  Elsewhere all of the heap is there from the start, capped at
  `BANK_MALLOCATOR_MAX_BANKS` banks.

[Memory](../../libraries/baselib/memory.md) has how the allocators work and what [`CTrueMallocator`] saved
TestStandardLib65.

## FAT16 and FAT32 in W65C816 assembly

BaseLib's FAT code is the HCP65's file system: the kernal reads the SD card with it.

**The bugs and the benchmark (2026-10-30).**  The user chose BaseLib's FAT32 as a real program to tune the code
generators on (TestFatBench, a separate benchmark, so TestFat32 stayed as it was), and to fix the FAT bugs it found.
Sixteen were found and fixed, each with a test in TestFat32Bugs that fails on the old code, among them:

- A read or write ending on a cluster boundary kept the old cluster; a read at the end of a full cluster returned
  512 bytes; a seek or append to the end of a full last cluster failed; [`CFatFile::Truncate`] on a boundary kept a
  cluster.
- [`CFatVolume::RewindFoundEntries`] was off by one and went back one cluster only (now through
  [`CFatVolume::FindPreviousCluster`]); a FAT entry write kept a stale first sector when crossing clusters.
- Rename and Delete deleted every long name entry with the same checksum
  ([`CFatVolume::DeleteLongNameEntries`] now runs before the short name's); a query's long name was inherited by the
  next short-name-only entry; [`MatchesFatFileName`] compared an unwritten short name; 8.3-only names were never
  written.
- Queries kept every sector locked (more than 32 sectors gave [`FAT_NO_CACHE_LOCK`]);
  [`CFatVolume::SetTotalFreeClusters`] assigned to itself.

TestFatStress checks random operations against a model on a sparse 65600 cluster drive (`CFatTestDrive`).  The C++ was
then made faster for the code generator (2026-10-30): directory entry times decoded lazily, sector and cluster
arithmetic by shifts (Mount refuses sector and cluster sizes that aren't powers of two), the cache's hit loop
first; it found two more bugs ([`FatDecodeDateTime`] left `tm_isdst` unset, and a long name sequence past 20 wrote
past the name).

**Why assembly (2026-10-31).**  The kernal no longer fitted ROM banks 1 to 3 (194.8KB of 192KB), and
`FatVolume.cpp` alone compiled to 63.9KB.  Of the ways offered (making MULT at boot, trimming, code in bank 0) the
user chose to write the FAT code in W65C816 assembly, and to "try and keep the linker interface the same between the
C++ version and the assembly version.  Change the C++ version if necessary."

| C++ | Compiled | Assembly |
|---|---|---|
| `FatVolume.cpp` | 63.9KB | [`BaseLib/FatVolumeW65C816.asm`], 10.2KB |
| `FatFile.cpp` | 11.4KB | [`BaseLib/FatFileW65C816.asm`], 2.9KB |
| `FatFilenameHelper.cpp` | 6.2KB | [`BaseLib/FatFilenameHelperW65C816.asm`], 1.5KB |

The caches are in assembly too ([`BaseLib/FatInfoSectorCacheW65C816.asm`],
[`BaseLib/FatClusterCacheW65C816.asm`]); [`BaseLib/FatCommon.cpp`], [`BaseLib/FatTime.cpp`],
[`BaseLib/FatFormat.cpp`], [`BaseLib/FatDebug.cpp`] and [`BaseLib/FatAbstractFile.cpp`] are small and stay C++.
How it is kept the same:

- The public functions keep their C++ names and calling convention, so C++ calls them unchanged; the protected ones
  are helpers inside the file.  Each file is one section (sections can land in different banks, and `jsr` and
  branches stay in a bank), and its functions share one frame layout, so the helpers find `this`, the cached sector
  and their scratch in the same places.
- The classes' layout comes from [`BaseLib/FatW65C816.inc`], generated by [`TestBaseLib65/TestFatW65C816Layout.cpp`]
  with the W65C816 compiler itself (it runs a program of `offsetof` and `sizeof`, since `offsetof` isn't a constant
  initialiser to it) and compared with the checked-in file.
- [`SFatCache`]'s methods are inline in the header: as calls into the assembly they made the C++ that used them 2KB
  bigger.
- FAT12 was dropped from the C++ and the assembly (the user, 2026-10-31; the C++'s FAT12 was broken anyway): Mount
  refuses a volume of `FAT12_MAX_CLUSTERS` clusters or fewer, and format won't make one.  TestFat16 tests FAT16.

The assembly is about a sixth of the size but slower in cycles than the compiled C++: TestFatBench 287M against 268M,
TestFat32 17.6G against 14.4G (2026-10-31).  It was written for the ROM, not for speed.

**Reading a file fast (2026-11-05).**  The HCP65 took 14.42 s at 25 MHz from reset to modplay's sound: 10.42 s of
it loading the 718,721 byte executable.  5.90 s was the FAT chain: [`CFatFile::Seek`] walked from the first cluster
every time (92 seeks, 104,628 FAT entry lookups on a 1,404 cluster chain of 512 byte clusters).  2.64 s was the SD
driver (about 28k cycles a sector: a bit-banged CMD17, its response and a software CRC7), and 1.72 s copies.  What was
done, in cycles to sound on:

| Change | Cycles |
|---|---|
| Before | 349M |
| CRC7 by table | 332M |
| The SD card's bit-banging in assembly ([`Kernal/KernalSDCard.asm`]) | 288M |
| Seek walks on from the position's cluster when it is known and not past the target | 160M |
| An SD card image with 4KB clusters | 142M |
| CMD18 multi-block reads, DMA straight to the destination, files read straight into the process | 85M |

The forward seek is in both the C++ and [`BaseLib/FatFileW65C816.asm`] (`SeekTo`).  The position's cluster is
ambiguous only at the end of a file that fills its last cluster; then the walk starts from the first.

## Decisions

- [`Libc65/softfloat.cpp`] stays as the reference; the 65C64's IEEE 754 floats are assembly written for speed (the
  user, 2026-10-23), then the doubles (the user, 2026-10-23).  The assembly stays bit exact with the C++.
- A B that isn't 0 when `div` starts is the dividend's top half, defined behaviour (the user, 2026-10-24).
- The string functions in assembly, and a small constant [`memcpy`] copied in place (the user, 2026-10-24).
- [`CTrueMallocator`] is standalone so Libc65 links it without the rest of BaseLib (Claude's choice, 2026-10-24,
  not ruled on: [Memory](../../libraries/baselib/memory.md#decisions)).
- Garth Wilson's tables (the user, 2026-10-29): multiply first; divide by a 128KB slice of INVERT if it measured
  faster; no lazy loading of tables; a table linked only if used; [`CIntelHex`] in BaseLib; ATAN's and LOG2's shared
  word kept ("trust garth"); 512KB of ROM, so the trig and log tables stay in the file system.
- The W65C816's string routines may change their own code: it runs in RAM (the user, 2026-10-29).
- The W65C816's soft float in assembly (the user, 2026-10-30), then tuned with MULT and INVERT (the user, 2026-10-30).
- FAT32 as the code generators' real-world benchmark, kept apart from TestFat32, and its bugs fixed (the user,
  2026-10-30).
- BaseLib's FAT code in W65C816 assembly with the C++'s linker interface (the user, 2026-10-31); FAT12 dropped from
  both (the user, 2026-10-31); a switch between the assembly and the C++, now
  [`CCPPCompileOptions::SetAssemblyUnits`] (the user asked, 2026-10-31).
- BaseLib's FAT names refuse `+ , ; = [ ]` ([`ILLEGAL_CHARS`]): a policy, left as it is (2026-10-30).
- The HCP65's tables are loaded from the SD card, not in ROM; the kernal and the start up program link the runtime
  without them; only INVERT's upper half is loaded (the user, 2026-11-06).
- Measured and dropped: integer divides by reciprocal tables (16 bit, 32 / 16 bit, and for byte divisors, 2026-10-29
  and 2026-11-06: the unrolled loops are as fast or faster); a 32 bit divide by an unbounded constant as a multiply
  (`R816_UMulWide32` 566 or 1210 cycles against the divide's 450 to 480); choosing a multiply routine from compile
  time bounds (-0.19%, not worth a second bounds analysis).

## Not done

- Why the FAT assembly is slower than the compiled C++ (above): profiles were taken (`TEST_PROJECT65_PROFILE`) but
  not analysed, and the assembly has no line table, so its time lands in its callers.  `TEST_PROJECT65_CPP=1` builds
  the C++ to compare.
- There is no `<float.h>`: code that needs a float's limits defines its own constants.
- The trig and log tables aren't used: [`Libc65/math.cpp`] computes those functions.  Reading them into RAM at run
  time by a library call was the plan if they were wanted.
- Lazy loading of code on the W65C816 is a separate project (the user, 2026-10-29): a fetch routine for each machine,
  out of ROM on a machine without a drive, through the FAT code on one with an SD card.  The 65C64's lazy loader
  doesn't work on the W65C816.
- Tables linked at a RAM address, stored in a ROM image and copied by the start up: superseded on the HCP65 by
  loading from the SD card, and part of the real machine still open in [W65C816 Back End Design](w65c816.md#not-done).
- A 64 / 32 bit divide in two 32 bit stages (`R816_UDiv64` takes about 8500 cycles): not hot in any profile.
- A cluster map for seeks (the user allowed it if useful, 2026-11-05; it must fit the ROM and be in both the C++ and
  the assembly).
- TestFat32, TestFatBench, TestFatStress and TestFat32Bugs are left out of the default TestBaseLib65 runs for their
  time; `TEST_PROJECT65_ONLY` runs them.
