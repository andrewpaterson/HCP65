; The HCP65 kernal's system call for stdlib.cpp's heap (W65C816 only): COP 13 with A the address of the call's block
; pointer on the stack, as UserLib's UserCalls.asm has it (Libc65 doesn't use UserLib).  The heap calls it only when
; the kernal's heap block says it is there, to map and unmap the heap's banks.
;
; The compiler passes extern "C" arguments in registers too (CppCompilerLib/W65C816.txt): the register entry pushes
; the pointer's words and calls the stack entry, then pops them (ply keeps the result in A).  Both are described for a
; debugger (.dfunction), as UserCalls.asm's are.

		.section text.__HeapSystemCall
		.global ___HeapSystemCall.ax
___HeapSystemCall.ax:
		.dfunction "__HeapSystemCall"
		.width (A, X), 16
		phx
		pha
		jsl ___HeapSystemCall
		ply
		ply
		rtl

		.global ___HeapSystemCall
___HeapSystemCall:
		.dfunction "__HeapSystemCall"
		.width (A, X), 16
		tsc
		clc
		adc 4
		cop 13
		rtl
