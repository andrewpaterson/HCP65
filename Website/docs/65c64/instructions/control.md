# brk, stp, nop - Control

| Mnemonic | Name | Operation |
|---|---|---|
| `brk` | Break | C+i → C; C → [S]; S-4 → S; P → [S]; S-2 → S; [vector] → C |
| `stp` | Stop | 1 → Clock |
| `nop` | No operation | wait O2 |

None of them change the flags.

## brk

```text
	brk vector
```

A software interrupt through one of 1024 vectors, replacing the W65C816's `brk` and `cop`.  It
pushes C (the address of the next instruction) and then P on S, adds 1 to the interrupt depth
and jumps to the address in the vector.  Unlike an interrupt, it leaves e and l as they are.
Return with [`rti`](return.md).

Vector n (0 to 1023) is the 32 bit address at `FFFF:F000 + 4n`.  The top vectors belong to the
hardware:

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
| 0 to 1014 | `FFFF:F000`–`FFFF:FFD8` | Software |

The hardware interrupts 0 to 2 are raised through the
[hardware registers](hardware.md), which aren't simulated, so the simulator never takes them.
See [Interrupts and Modes](../interrupts.md) for what happens when an interrupt is taken.

```asm
	brk 5
```

## stp

```text
	stp
	stp wake
	stp (wake, wake, …)
```

Stop the processor, replacing the W65C816's `stp` and `wai`.  It's a kernel only instruction.

- Without wake bits, the processor stops until reset.
- With wake bits it waits for one of those interrupts, even a disabled one.  If the interrupt
  is enabled it's taken, with C pointing after the `stp`.  If it's disabled, execution carries
  on after the `stp`.

The wake bits are `l` (the level interrupt) and `e0` to `e3` (the edge interrupts).

```asm
	stp
	stp e3
	stp (l, e0)
```

## nop

```text
	nop
	nop cycles
```

Do nothing for 1 to 8 extra cycles (1 when no count is given), replacing the W65C816's `nop`
and `wdm`.  A `nop n` takes 1 + n cycles.

```asm
	nop
	nop 3
```

## Encoding

`brk` and `stp` are instruction code 11.  `nop` is instruction code 12, extend 1.  See [Opcode
Encoding](../opcode-encoding.md) for the bits, and [Sizes and Timing](../timing.md) for the words
and cycles.
