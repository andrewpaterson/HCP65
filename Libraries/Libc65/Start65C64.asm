; The start up of a 65C64 program that runs from reset on a bare 65C64 (the X65C64 Emulator: HCP65/X65C64-Emulator), as
; UserStart.asm is a W65C816 program's on the HCP65.  A 65C64 project lists it with its sources; its section is linked
; first, at the start of the text (0001:0000), where the image is loaded and reset starts it.
;
; It sets S to 001F:FFFF, W to the pseudo registers at 0000:0000 and the registers to 32 bits, calls the init table's
; routines (each global's R_Initialise, in link order) and main (std::terminate if an exception nobody caught left it,
; in a program built with exceptions), runs the exit lists and the fini table's routines last first, and stops with
; main's result in A.  It also defines C's _Exit.
;
; It is the compiler's own start up (CCPPTarget65C64::AppendStartUp), which the compiler's tests link: TestCppCompilerLib's
; TestCPP65GeneratorStartUpFile keeps the two the same, and writes the compiler's to its Output/Start65C64.asm when they
; differ.

		.section text, keep, resident
Start:
		cpy S, 0x1fffff
		cpy W, 0
		wtth (A, B), 32
		wtth (X, Y), 32
		cpy Y, __init_start
StartInitialise:
		cmp Y, __init_end
		bras c, StartMain
		psh Y
		jsr [Y]
		pul Y
		add Y, 4
		bra StartInitialise
StartMain:
		jsr _main
		brac c, StartReturned
		.weak R_Exceptions
		cpy Y, R_Exceptions
		cmp Y, 0
		bras z, StartReturned
		.weak ___Terminate
		cpy Y, ___Terminate
		cmp Y, 0
		bras z, StartUncaught
		jsr Y
StartUncaught:
		stp
StartReturned:
		psh A
		.weak ___RunAtExit
		.weak R_RunExitLists
		cpy Y, ___RunAtExit
		cmp Y, 0
		bras z, StartExitLists
		jsr Y
		bra StartAtExitDone
StartExitLists:
		cpy Y, R_RunExitLists
		cmp Y, 0
		bras z, StartAtExitDone
		jsr Y
StartAtExitDone:
		cpy Y, __fini_end
StartFinalise:
		cmp Y, __fini_start
		bras z, StartStop
		sub Y, 4
		psh Y
		jsr [Y]
		pul Y
		bra StartFinalise
StartStop:
		pul A
		stp
		.global __Exit.a
__Exit.a:
		stp
		.global __Exit
__Exit:
		cpy V, S
		cpy A, [5 + V]
		stp
		.global __ExitEnd
__ExitEnd:
		.section data.ExitLists, keep
		.align 4
		.global __ExitList, __ThreadExitList
__ExitList:
		.long 0
__ThreadExitList:
		.long 0

