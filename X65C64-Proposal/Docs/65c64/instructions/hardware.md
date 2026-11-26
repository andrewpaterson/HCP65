# hws, hwg, hwsp, hwcp - Hardware Registers

--8<-- "hardware.txt"

Read and write the processor's hardware registers: the interrupt depth, the counters, the clock,
the watchdog, the programmable I/O and the user mode bank remapping.

The opcode holds the hardware register index (0 to 63).  The operand is A, B, X or Y, or for
`hws` and `hwsp`, an unsigned 8, 16, 32 or 64 bit immediate.

## Hardware registers

--8<-- "hardware-registers.txt"

!!! note
    The hardware register instructions are in the architecture spreadsheet, but they aren't
    assembled or simulated yet.  The assembler reports them as unsupported.

## Encoding

The hardware register instructions are instruction code 15, extend 0 to 3.  See [Opcode
Encoding](../opcode-encoding.md) for the bits, and [Sizes and Timing](../timing.md) for the words
and cycles.
