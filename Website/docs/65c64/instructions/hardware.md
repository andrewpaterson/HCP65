# hws, hwg, hwsp, hwcp - Hardware Registers

| Mnemonic | Name | Operation |
|---|---|---|
| `hws` | Set hardware all | R → H |
| `hwg` | Get hardware all | H → R |
| `hwsp` | Set hardware bits | H ∨ O2 → H |
| `hwcp` | Clear hardware bits | H ∧ !O2 → H |

Read and write the processor's hardware registers: the interrupt depth, the counters, the clock,
the watchdog, the programmable I/O and the user mode bank remapping.

The opcode holds the hardware register index (0 to 63).  The operand is A, B, X or Y, or for
`hws` and `hwsp`, an unsigned 8, 16, 32 or 64 bit immediate.

## Hardware registers

| Index | Width | Purpose |
|---|---|---|
| 0 | 32 bits | Get / set the interrupt depth (above 0 is kernel mode) |
| 1 | | Reset the instruction count |
| 2 | 32 bits | Get the instruction count, low (latches the count) |
| 3 | 32 bits | Get the instruction count, high |
| 4 | | Reset the clock tick count |
| 5 | 32 bits | Get the clock tick count, low (latches the count) |
| 6 | 32 bits | Get the clock tick count, high |
| 7 | bool | Get / set the internal SRAM enable |
| 8 | 8 bits | Get / set the clock multiplier for internal operations |
| 9 | 8 bits | Get / clear NMI waiting |
| 10 | bool | Get / set the watchdog timer enable |
| 11 | 16 bits | Get / set the watchdog clock rate selector |
| 12 | 32 bits | Set a CPLD transient fuse |
| 13 | | Reset all the CPLD transient fuses |
| 14 | bool | Get / set the GPIO enable |
| 15 | 32 bits | Get the GPIO pin values |
| 16 | 8 bits | Get / set the Initial flag (including high on an invalid address) |
| 17 | bool | Get / set the User flag |
| 18 | 32 bits | Get / set the interrupt vector offset |
| 19 | bool | Get / set user mode bank remapping enable |
| 20 | 16 bits | Get / set the user mode bank width (256 is 16MB, 512 is 32MB …) |
| 21 | 32 bits | Get / set the memory bank remapping offset |
| 22 | 16 bits | Get / set the user process ID |
| 23 | 32 bits | Get / set the CPLD bits |

!!! note
    The hardware register instructions are in the architecture spreadsheet, but they aren't
    assembled or simulated yet.  The assembler reports them as unsupported.

## Encoding

The hardware register instructions are instruction code 15, extend 0 to 3.  See [Opcode
Encoding](../opcode-encoding.md) for the bits, and [Sizes and Timing](../timing.md) for the words
and cycles.
