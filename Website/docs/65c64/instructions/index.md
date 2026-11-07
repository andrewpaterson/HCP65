# Instructions

Every 65C64 instruction, grouped by what it does.  The operations use the
[notation](../notation.md) of the architecture spreadsheet, and the operands are described in
[Operands and Addressing](../operands.md).

Many instructions have a zero extending and a sign extending form.  They share a mnemonic: the
sign extending form is written with `:s` on the second operand (`cpy A, B:s`).

Flags not listed are unchanged.  The W65C816 column names the W65C816 instructions each one
replaces.

## Copy, logic and arithmetic

These take register, value and memory [operands](../operands.md).

| Mnemonic | Name | Operation | Flags | W65C816 |
|---|---|---|---|---|
| [`cpy`](cpy.md) | Copy | O2 → O1 | n, z | lda ldx ldy sta stx sty stz |
| [`and`](logical.md) | And | O1 ∧ O2 → O1 | n, z | and |
| [`or`](logical.md) | Or | O1 ∨ O2 → O1 | n, z | ora tsb |
| [`xor`](logical.md) | Xor | O1 ⊻ O2 → O1 | n, z | eor |
| [`tst`](logical.md) | Test | O1 ∧ O2 | n, z | bit |
| [`add`](arithmetic.md) | Add | O1 + O2 → O1 | n, v, z, c | adc inc inx iny |
| [`sub`](arithmetic.md) | Subtract | O1 - O2 → O1 | n, v, z, c | sbc dec dex dey |
| [`cmp`](arithmetic.md) | Compare | O1 - O2 | n, v, z, c | cmp cpx cpy |
| [`cpyx`](cpyx.md) | Copy extend | O2:o → R | n, z | |
| [`shl`](shift.md) | Shift left | O1 ‹‹ O2 → O1 | z, c, v | asl |
| [`shr`](shift.md) | Shift right | O1 ›› O2 → O1 | n, z, c, v | lsr |

`add`, `sub` and `cmp` don't use the carry as an input.  `cpyx` copies a value of a given
width (8, 16, 32 or 64 bits) between memory and A, B, X or Y.

## Register operations

| Mnemonic | Name | Operation | Flags | W65C816 |
|---|---|---|---|---|
| [`scp`](subcopy.md) | Subcopy bytes | R1:[O1]…R1:[O1+N] → R2:[O2]…R2:[O2+N] | n, z | tax tay txa txy tya tyx tcd tdc tcs tsc tsx txs xba |
| [`scb`](subcopy.md) | Subcopy bits | R1:b[O1]…R1:b[O1+N] → R2:b[O2]…R2:b[O2+N] | n, z | |
| [`neg`](arithmetic.md#neg-and-abs) | Negate | 0 - R2:s → R1 | n, z | |
| [`abs`](arithmetic.md#neg-and-abs) | Absolute | abs(R2:s) → R1 | n, z | |
| [`mul`](multiply-divide.md#mul) | Multiply (partial) | A × B → Y, a byte of B a step | v, z | |
| [`div`](multiply-divide.md#div) | Divide (partial) | Y ÷ A → Y, remainder B, a nybble a step | v, z, c | |
| [`fsbi`](find.md) | Find set bit | if c {N+1 → N}; while R:b[N] = 0, N+1 → N | v, c | |
| [`fcbi`](find.md) | Find clear bit | if c {N+1 → N}; while R:b[N] = 1, N+1 → N | v, c | |
| [`fscr`](find.md) | Find set crumb | As `fsbi`, for 2 bit crumbs | v, c | |
| [`fccr`](find.md) | Find clear crumb | As `fcbi`, for 2 bit crumbs | v, c | |
| [`fsny`](find.md) | Find non-zero nybble | if c {N+1 → N}; while ∨R:n[N] = 0, N+1 → N | v, c | |
| [`fcny`](find.md) | Find zero nybble | if c {N+1 → N}; while ∧R:n[N] = 1, N+1 → N | v, c | |
| [`fsby`](find.md) | Find non-zero byte | if c {N+1 → N}; while ∨R:[N] = 0, N+1 → N | v, c | |
| [`fcby`](find.md) | Find zero byte | if c {N+1 → N}; while ∧R:[N] = 1, N+1 → N | v, c | |

`mul` and `div` do one step each time they run and leave C on their opcode until the result
is done (v set), so they repeat on their own and can be interrupted between steps.

## Flow

| Mnemonic | Name | Operation | Flags | W65C816 |
|---|---|---|---|---|
| [`jmp`](jump.md) | Jump absolute | {P → [S]; S-2 → S}; O2 → C | | jmp |
| [`jsr`](jump.md) | Jump absolute subroutine | C+i → C; C → [S]; S-4 → S; {P → [S]; S-2 → S}; O2 → C | | jsr |
| [`bra`](jump.md#relative-targets) | Branch always / jump relative | C+i → C; {P → [S]; S-2 → S}; C + O2 → C | | bra brl |
| [`brsr`](jump.md#relative-targets) | Jump relative subroutine | C+i → C; C → [S]; S-4 → S; {P → [S]; S-2 → S}; C + O2 → C | | |
| [`bras`](branch.md) | Branch on set | C+i → C; if p {C + O2 → C} | | bcs beq bvs bmi |
| [`brac`](branch.md) | Branch on clear | C+i → C; if !p {C + O2 → C} | | bcc bne bvc bpl |
| [`brasa`](branch.md) | Branch on set and set | C+i → C; if {p ∧ p} {C + O2 → C} | | |
| [`braso`](branch.md) | Branch on set xor set | C+i → C; if {p ⊻ p} {C + O2 → C} | | |
| [`brae`](branch.md) | Branch on equal | C+i → C; if (p1 = p2) {C + O2 → C} | | |
| [`brane`](branch.md) | Branch on not equal | C+i → C; if (p1 ≠ p2) {C + O2 → C} | | |
| [`ret`](return.md) | Return | {S+2 → S; [S] → P}; S+4 → S; [S] → C | all, if P is pulled | rts rtl |
| [`rti`](return.md) | Return from interrupt | S+2 → S; [S] → P; S+4 → S; [S] → C | all | rti |
| [`brk`](control.md#brk) | Break | C+i → C; C → [S]; S-4 → S; P → [S]; S-2 → S; [vector] → C | | brk cop |
| [`stp`](control.md#stp) | Stop | 1 → Clock | | stp wai |
| [`nop`](control.md#nop) | No operation | wait O2 | | nop wdm |

Branch offsets are in bytes, relative to the next instruction.  `jmp`, `jsr`, `bra` and `brsr`
can push P after C on any stack.

## Stack

| Mnemonic | Name | Operation | Flags | W65C816 |
|---|---|---|---|---|
| [`psh`](stack.md) | Push registers | {A → [S]; S-a → S}; … {Y → [S]; S-y → S} | n, z | pha phx phy phd |
| [`psh`](stack.md) | Push operand | O2 → [S]; S-o → S | | pea pei per |
| [`psh`](stack.md) | Push processor state | P → [S]; S-2 → S | | php |
| [`psh`](stack.md) | Push program count | C → [S]; S-4 → S | | |
| [`pul`](stack.md) | Pull registers | {S+a → S; [S] → A}; … {S+y → S; [S] → Y} | n, z | pla plx ply pld |
| [`pul`](stack.md) | Pull into address | S+4 → S; [S] → O2 | | |
| [`pul`](stack.md) | Pull processor state | S+2 → S; [S] → P | all | plp |

Registers are pushed in the order A, B, S, V, W, X, Y, each at its width, and pulled in the
reverse order.  The stack can be any of A, B, S, V, W, X and Y.

## Processor state

| Mnemonic | Name | Operation | Flags | W65C816 |
|---|---|---|---|---|
| [`sep`](status.md#sep-and-cep) | Set state bits | P ∨ E → P | the flags named | sep sec sei |
| [`cep`](status.md#sep-and-cep) | Clear state bits | P ∧ !E → P | the flags named | rep clc |
| [`wth`](width.md#wth-and-wtth) | Set register width | O → r | a, b, x, y | rep sep |
| [`wtth`](width.md#wth-and-wtth) | Set two register widths | O2 → r1; O2 → r2 | a, b, x, y | |
| [`gwth`](width.md#gwth) | Get register width | 8r → R; {8r - 1 → R} | | |
| [`wthx`](width.md#wthx) | Extend register width | xR:[O-r…O] → R; O → r | | |
| [`bist`](status.md#bist) | Bit to status | R:b[N] → p | the flag named | |
| [`stab`](status.md#stab) | Status to bit | p → R:b[N]; {p → R:b[N±1] … R:b[N±(count-1)]} | | |
| [`stat`](status.md#stat) | Status to status | p → p | the flag named | |

## Block and vector

These repeat on their own, one element a step, until the count in A goes negative (v set) or,
if stopping is allowed, a stop condition on z or n holds.  They can be interrupted between
steps.

| Mnemonic | Name | Operation | Flags |
|---|---|---|---|
| [`bcpy`](block.md) | Block copy | R → [V]; A-1 → A; V+r → V | v |
| [`band`](block.md) | Block and | [V] ∧ R → [V]; A-1 → A; V+r → V | n, v, z |
| [`btst`](block.md) | Block test | [V] ∧ R; A-1 → A; V+r → V | n, v, z |
| [`bor`](block.md) | Block or | [V] ∨ R → [V]; A-1 → A; V+r → V | n, v, z |
| [`bxor`](block.md) | Block xor | [V] ⊻ R → [V]; A-1 → A; V+r → V | n, v, z |
| [`badd`](block.md) | Block add | [V] + R → [V]; A-1 → A; V+r → V | n, v, z |
| [`bsub`](block.md) | Block subtract | [V] - R → [V]; A-1 → A; V+r → V | n, v, z |
| [`bcmp`](block.md) | Block compare | [V] - R; A-1 → A; V+r → V | n, v, z |
| [`bpcpy`](block.md) | Block precopy | [V]; R → [V]; A-1 → A; V+r → V | n, v, z |
| [`bpand`](block.md) | Block preand | [V] ∧ R → [V]; A-1 → A; V+r → V | n, v, z |
| [`bpor`](block.md) | Block preor | [V] ∨ R → [V]; A-1 → A; V+r → V | n, v, z |
| [`bpxor`](block.md) | Block prexor | [V] ⊻ R → [V]; A-1 → A; V+r → V | n, v, z |
| [`vcpy`](vector.md) | Vector copy | [W] → [V]; A-1 → A; V+o → V; W+o → W | n, v, z |
| [`vnot`](vector.md) | Vector invert | ![W] → [V]; A-1 → A; V+o → V; W+o → W | n, v, z |
| [`vand`](vector.md) | Vector and | [V] ∧ [W] → [V]; A-1 → A; V+o → V; W+o → W | n, v, z |
| [`vtst`](vector.md) | Vector test | [V] ∧ [W]; A-1 → A; V+o → V; W+o → W | n, v, z |
| [`vor`](vector.md) | Vector or | [V] ∨ [W] → [V]; A-1 → A; V+o → V; W+o → W | n, v, z |
| [`vxor`](vector.md) | Vector xor | [V] ⊻ [W] → [V]; A-1 → A; V+o → V; W+o → W | n, v, z |
| [`vadd`](vector.md) | Vector add | [V] + [W] → [V]; A-1 → A; V+o → V; W+o → W | n, v, z |
| [`vsub`](vector.md) | Vector subtract | [V] - [W] → [V]; A-1 → A; V+o → V; W+o → W | n, v, z |
| [`vcmp`](vector.md) | Vector compare | [V] - [W]; A-1 → A; V+o → V; W+o → W | n, v, z |
| [`vpcpy`](vector.md) | Vector precopy | [V]; [W] → [V]; A-1 → A; V+o → V; W+o → W | n, v, z |
| [`vpand`](vector.md) | Vector preand | [V] ∧ [W] → [V]; A-1 → A; V+o → V; W+o → W | n, v, z |
| [`vpor`](vector.md) | Vector preor | [V] ∨ [W] → [V]; A-1 → A; V+o → V; W+o → W | n, v, z |
| [`vpxor`](vector.md) | Vector prexor | [V] ⊻ [W] → [V]; A-1 → A; V+o → V; W+o → W | n, v, z |

The *pre* forms set n and z from the element as it was before it's changed, so a stop
condition sees the original value.  The `vcpy` instructions replace the W65C816's `mvn` and
`mvp`.

## Hardware

| Mnemonic | Name | Operation |
|---|---|---|
| [`hws`](hardware.md) | Set hardware all | R → H |
| [`hwg`](hardware.md) | Get hardware all | H → R |
| [`hwsp`](hardware.md) | Set hardware bits | H ∨ O2 → H |
| [`hwcp`](hardware.md) | Clear hardware bits | H ∧ !O2 → H |

The hardware register instructions are in the spreadsheet but aren't assembled or simulated
yet.

## W65C816 instructions without a match

Some W65C816 instructions have no single 65C64 instruction, but can be built from a few:

| W65C816 | 65C64 | How |
|---|---|---|
| `rol` | `stat f, c`<br>`shl A, 1`<br>`stab (A, 0), f` | Copy c into f, shift A left, copy f into bit 0. |
| `ror` | `stat f, c`<br>`gwth X, (A, -1)`<br>`shr A, 1`<br>`stab (A, X), f` | Copy c into f, put A's width less one in X, shift A right, copy f into bit X. |
| `mvn` | `vcpy W, V, -1, A, 8` | Copy bytes from [W] to [V], moving down. |
| `mvp` | `vcpy W, V, +1, A, 8` | Copy bytes from [W] to [V], moving up. |
| `trb` | `xor A, -1`<br>`and [M], A` | Invert A, then and it into memory. |

`cld` and `sed` (decimal mode) have to be written in software.  `phb`, `phk`, `plb` and `xce`
have no meaning outside the W65C816.
