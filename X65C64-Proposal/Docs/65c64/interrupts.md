# Interrupts and Modes

How the 65C64 starts, how it takes interrupts and what kernel mode is.  The instructions involved
are [`brk`, `stp`](instructions/control.md), [`rti`](instructions/return.md) and
[`sep` and `cep`](instructions/status.md#sep-and-cep).

## Reset

Pulling the Reset pin low holds the processor in reset.  When it's released the processor
spends two internal cycles, reads the reset vector at `FFFF:FFFC` and starts at the address it
holds.

| State | After reset |
|---|---|
| A, B, X, Y, V, W | 0 |
| S | `0001:FFFF` |
| Widths of A, B, X and Y | 8 bits |
| e and l | Set: interrupts are disabled |
| f, g, n, v, z, c | Clear |
| Interrupt depth | 1: kernel mode |

## The interrupts

| Interrupt | Pin | Triggered by | Disabled by | W65C816 |
|---|---|---|---|---|
| Edge interrupt 0 | E Interrupt 0 | A falling edge | e | NMIB |
| Edge interrupt 1 | E Interrupt 1 | A falling edge | e | |
| Edge interrupt 2 | E Interrupt 2 | A falling edge | e | |
| Edge interrupt 3 | E Interrupt 3 | A falling edge | e | |
| Level interrupt | Level Interrupt | The pin held low | l | IRQB |
| Hardware interrupts 0 to 2 | | The [hardware registers](instructions/hardware.md) | | |
| Software interrupt | | [`brk`](instructions/control.md#brk) | Never | BRK, COP |

- **Edge interrupts.**  A falling edge on the pin is latched as pending, so a short pulse isn't
  lost.  The interrupt is taken once, when it's enabled.  While e is set the four edge
  interrupts stay pending.  Unlike the W65C816's NMI, an edge interrupt can be masked.
- **Level interrupt.**  It's taken for as long as the pin is low and l is clear.  Nothing is
  latched: the device must hold the pin low until it's serviced, and the handler must make the
  device release it.
- **Hardware interrupts.**  They come from inside the processor (the watchdog timer and the
  counters) through the hardware registers.  The simulator doesn't have them.

When more than one is waiting, edge interrupt 0 is taken first, then 1, 2 and 3, then the
level interrupt.

## Taking an interrupt

Interrupts are taken between instructions.  The [repeating instructions](instructions/block.md)
(block, vector, `mul` and `div`) count each step as an instruction, so they can be interrupted
part way and carry on afterwards.

Taking an interrupt takes the place of fetching the next opcode:

1. C, the address of the next instruction, is pushed on S: 4 bytes.
2. P is pushed on S: 2 bytes.
3. The interrupt's vector is read, with the Interrupt Vector pin low, and its address goes
   into C.
4. The interrupt depth goes up by one.
5. An edge interrupt sets e and l.  A level interrupt sets l.  `brk` sets neither.

That is 5 cycles when S is word aligned: two to push C, one to push P and two to read the
vector.  `brk` takes 6, with its opcode.

The handler returns with [`rti`](instructions/return.md), which pulls P and then C and takes
the interrupt depth down by one.  Pulling P puts e and l back as they were, along with the
register widths, so a handler is free to change widths.

A handler that uses a register must save it, and the register may be at any width when the
interrupt arrives.  Setting a width doesn't change a register's bits, so a handler widens the
registers to 64 bits before it pushes them, and again before it pulls them.  `rti` puts the
interrupted code's widths back with P:

```asm
	.org $2000
Handler:
	wtth (A, X), 64
	psh (A, X)		; every bit of A and X
	wtth (A, X), 32		; the widths the handler works at
	cpy X, Ticks
	cpy A, [X]
	add A, 1
	cpy [X], A
	wtth (A, X), 64
	pul (A, X)
	rti			; pulls P: the old widths, e and l
Ticks:
	.long 0

	.org $FFFFFFE8		; the level interrupt's vector
	.long Handler
```

## Vectors

A vector is the 32 bit address of a handler.  There are 1024, four bytes each, in the top 4KB
of memory: vector n is at `FFFF:F000 + 4n`.

| Vector | Address | Used by |
|---|---|---|
| 1023 | `FFFF:FFFC` | Reset |
| 1022 | `FFFF:FFF8` | Edge interrupt 0 |
| 1021 | `FFFF:FFF4` | Edge interrupt 1 |
| 1020 | `FFFF:FFF0` | Edge interrupt 2 |
| 1019 | `FFFF:FFEC` | Edge interrupt 3 |
| 1018 | `FFFF:FFE8` | Level interrupt |
| 1017 | `FFFF:FFE4` | Hardware interrupt 0 |
| 1016 | `FFFF:FFE0` | Hardware interrupt 1 |
| 1015 | `FFFF:FFDC` | Hardware interrupt 2 |
| 0 to 1014 | `FFFF:F000`–`FFFF:FFD8` | Software: `brk 0` to `brk 1014` |

`brk` can name any vector, so `brk 1018` runs the level interrupt's handler.

The proposal has a hardware register, the interrupt vector offset, that moves the vector table.
It isn't simulated.

## Waiting for an interrupt

[`stp`](instructions/control.md#stp) stops the processor.  With wake bits it waits for one of
the interrupts it names (`l`, `e0` to `e3`), like the W65C816's `wai`:

- If the interrupt that arrives is enabled, it's taken, with C pointing after the `stp`.
- If it's disabled, it still wakes the processor, and execution carries on after the `stp`
  without the interrupt being taken.  That is the quickest way to wait for a device: no
  vector, no push and no `rti`.
- An interrupt that the `stp` doesn't name doesn't wake it, and stays pending.

Without wake bits, `stp` stops the processor until reset.

```asm
	sep (e, l)		; interrupts disabled
	stp e0			; wait for edge interrupt 0, then carry on here
```

## Kernel mode and user mode

The processor counts how deeply it's nested in interrupts.  The *interrupt depth* goes up by
one each time a vector is read (an interrupt, a `brk` or reset) and down by one at each `rti`.

- A depth above 0 is **kernel mode**.
- A depth of 0 is **user mode**.

Reset leaves the depth at 1, so the processor starts in kernel mode.  A kernel starts a user
program by returning to it with `rti`, which takes the depth to 0, and the program gets back
into the kernel with `brk` or when an interrupt arrives.

In the proposal, user mode:

- can't run the kernel only instructions, such as `stp`,
- has its addresses moved by the [user bank map](memory-map.md#user-bank-remapping), so each
  process sees its own memory, and
- is shown on the Kernel Mode pin (low in kernel mode).

!!! note
    The simulator counts the interrupt depth, but it is always in kernel mode: kernel only
    instructions always run and no addresses are remapped.
