# vcpy, vnot, vand, vtst, vor, vxor, vadd, vsub, vcmp, vpcpy, vpand, vpor, vpxor - Vector

| Mnemonic | Name | Operation | Flags |
|---|---|---|---|
| `vcpy` | Vector copy | [W] → [V]; A-1 → A; V+o → V; W+o → W | n, v, z |
| `vnot` | Vector invert | ![W] → [V]; A-1 → A; V+o → V; W+o → W | n, v, z |
| `vand` | Vector and | [V] ∧ [W] → [V]; A-1 → A; V+o → V; W+o → W | n, v, z |
| `vtst` | Vector test | [V] ∧ [W]; A-1 → A; V+o → V; W+o → W | n, v, z |
| `vor` | Vector or | [V] ∨ [W] → [V]; A-1 → A; V+o → V; W+o → W | n, v, z |
| `vxor` | Vector xor | [V] ⊻ [W] → [V]; A-1 → A; V+o → V; W+o → W | n, v, z |
| `vadd` | Vector add | [V] + [W] → [V]; A-1 → A; V+o → V; W+o → W | n, v, z |
| `vsub` | Vector subtract | [V] - [W] → [V]; A-1 → A; V+o → V; W+o → W | n, v, z |
| `vcmp` | Vector compare | [V] - [W]; A-1 → A; V+o → V; W+o → W | n, v, z |
| `vpcpy` | Vector precopy | [V]; [W] → [V]; A-1 → A; V+o → V; W+o → W | n, v, z |
| `vpand` | Vector preand | [V] ∧ [W] → [V]; A-1 → A; V+o → V; W+o → W | n, v, z |
| `vpor` | Vector preor | [V] ∨ [W] → [V]; A-1 → A; V+o → V; W+o → W | n, v, z |
| `vpxor` | Vector prexor | [V] ⊻ [W] → [V]; A-1 → A; V+o → V; W+o → W | n, v, z |

Combine two blocks of memory an element at a time: copy, invert, mask, add, compare.  W points at
the source, V at the destination, and A counts the elements.  `vcpy` replaces the W65C816's
`mvn` and `mvp`.

## How they run

Each step reads [W] and then [V] (not for `vcpy` and `vnot`), computes D = [V] op [W] (or [W],
or ![W]), and writes D to [V] (not for `vtst` and `vcmp`).  It then moves V and W on by one
element and subtracts 1 from A.

Like the [block instructions](block.md), it repeats on its own until A goes negative (v is set),
so A + 1 elements are done, or until an allowed stop condition holds.  It can be interrupted
between steps.  A step takes the opcode cycle plus a cycle per word of [W] and [V] read and of
[V] written.

| Flag | Meaning |
|---|---|
| v | A is negative: done |
| z | D is 0 |
| n | D is negative |

The *pre* forms (`vpcpy`, `vpand`, `vpor` and `vpxor`) set n and z from the original [V], before
it's written, as the [block pre forms](block.md#the-pre-forms) do.

## Forms

```asm
	op W, V, direction, A, width
	op W, V, direction, A, width, stop
	op W, V, direction-W, direction-V, A, width
	op W, V, direction-W, direction-V, A, width, stop
```

| Part | Allowed |
|---|---|
| direction | `+1` (up) or `-1` (down).  One direction is for both W and V, and two are W's then V's. |
| width | The element width: 8, 16, 32 or 64 bits |
| stop | `z`, `!z`, `n` or `!n`, as for the [block instructions](block.md#forms) |

One word.

```asm
	vcpy W, V, +1, A, 64
	vcpy W, V, -1, A, 8
	vsub W, V, -1, A, 16, n
	vsub W, V, -1, +1, A, 16, n
	vpxor W, V, +1, A, 32
```

## Examples

Copy A + 1 64 bit values from [W] to [V] (the spreadsheet's block move: 1.13 cycles a byte):

```asm
	wtth (X, Y), 32
	wth A, 32
	pul (A, V, W), S
	vcpy W, V, +1, A, 64
```

Add a table of four 16 bit values to another:

```asm
	wth A, 32
	cpy W, $3000
	cpy V, $3010
	cpy A, 3
	vadd W, V, +1, A, 16
```

The W65C816's `mvn` and `mvp`, copying bytes down or up:

```asm
	vcpy W, V, -1, A, 8	; mvn
	vcpy W, V, +1, A, 8	; mvp
```
