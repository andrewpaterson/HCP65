# ret, rti - Return

| Mnemonic | Name | Operation | Flags |
|---|---|---|---|
| `ret` | Return | {S+2 → S; [S] → P}; S+4 → S; [S] → C | all, if P is pulled |
| `rti` | Return from interrupt | S+2 → S; [S] → P; S+4 → S; [S] → C | all |

`ret` returns from a subroutine called with [`jsr` or `brsr`](jump.md).  It pulls C, the return
address, and first P if it's given.  It replaces the W65C816's `rts` and `rtl`.

`rti` returns from an interrupt or a [`brk`](control.md): it pulls P then C, and subtracts 1 from
the interrupt depth.  e and l come back with P.

Pulling P sets every flag in it, including the register widths a, b, x and y.

## Forms

```asm
	ret
	ret P
	ret stack
	ret P, stack
	rti
	rti stack
```

The stack is S (the default), V, W or Y, and must be the one the call or interrupt pushed on.

```asm
	ret
	ret P
	ret P, W
	rti
	rti V
```

## Example

A subroutine called with its own stack in V, saving P:

```asm
	jsr Sub, P, V
	stp
Sub:
	cpy A, V
	ret P, V
```
