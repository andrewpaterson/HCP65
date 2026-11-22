; The start up of a program the HCP65 kernal runs (an executable: see Kernal/KernalProcess.cpp).
;
; The kernal maps the process's banks, puts the program's sections in them and RTIs to UserStart in user mode, with
; S at the top of the process's bank 0, 8 bit registers and interrupts off.  UserStart goes 16 bit, sets the stack and
; the direct page (at USER_DIRECT_PAGE: the bank's first bytes are the kernal's NULL canary, and $000200 its stack
; canary, which the stack must stay above: Kernal/KernalProcess.h), clears bss, calls the init table's routines (each global's R_Initialise, in link order) and main
; (std::terminate if an exception leaves it, in a program built with exceptions), then
; runs the exit lists (Libc65's __RunAtExit when stdlib.cpp is linked, else the compiler's R_RunExitLists: the atexit
; functions and the destructors of what was made at start up and of static locals, last first) and the fini table's
; routines last first, and exits with main's result (COP 0).  Code and data are linked from bank 1; bank 0 is the stack
; and direct page.

USER_STACK_TOP		= 0xEFFF
USER_DIRECT_PAGE	= 0x0100


		.section text, keep
		.global UserStart
UserStart:
		.width (A, X), 8
		rep (m, x)
		lda USER_STACK_TOP
		tcs
		lda USER_DIRECT_PAGE
		tcd
		phk
		plb
		; bss (globals with no initialiser) isn't in the image.  Up to 64KB is cleared a word at a time (a long address
		; plus X crosses banks; an odd size clears the byte after it too, where nothing is yet), more a byte at a time.
		lda __bss_end & 0xFFFF
		sec
		sbc __bss_start & 0xFFFF
		tay
		lda __bss_end >> 16
		sbc __bss_start >> 16
		bne UserClearLarge
		tya
		inc A
		lsr A
		beq UserCleared
		tay
		ldx 0
		lda 0
UserClearWord:
		sta [__bss_start + X]:24
		inx
		inx
		dey
		bne UserClearWord
		bra UserCleared
UserClearLarge:
		lda __bss_start & 0xFFFF
		sta [D + 0]
		lda __bss_start >> 16
		sta [D + 2]
UserClear:
		lda [D + 0]
		cmp __bss_end & 0xFFFF
		bne UserClearByte
		lda [D + 2]
		cmp __bss_end >> 16
		beq UserCleared
UserClearByte:
		sep m
		lda 0
		sta [[D + 0]:24]
		rep m
		inc [D + 0]
		bne UserClear
		inc [D + 2]
		bra UserClear
UserCleared:
		lda __init_start & 0xFFFF
		sta [D + 0]
		lda __init_start >> 16
		sta [D + 2]
UserInitialise:
		lda [D + 0]
		cmp __init_end & 0xFFFF
		bne UserInitialiseCall
		lda [D + 2]
		cmp __init_end >> 16
		beq UserMain
UserInitialiseCall:
		jsr UserCallEntry
		clc
		lda [D + 0]
		adc 4
		sta [D + 0]
		lda [D + 2]
		adc 0
		sta [D + 2]
		bra UserInitialise
UserMain:
		jsl _main
		.width (A, X), 16
		; In a program built with exceptions (R_Exceptions: its units define it) main returns with the carry set when an
		; exception nobody caught left it: std::terminate, Libc65's __Terminate, linked when anything throws.  Without
		; exceptions the carry is as it fell.  A is main's result, kept in Y meanwhile.
		bcc UserReturned
		.weak R_Exceptions
		.weak ___Terminate
		tay
		lda R_Exceptions & 0xFFFF
		ora R_Exceptions >> 16
		beq UserNotThrown
		lda ___Terminate & 0xFFFF
		ora ___Terminate >> 16
		beq UserNotThrown
		jsl ___Terminate
UserNotThrown:
		tya
UserReturned:
		pha
		.weak ___RunAtExit
		.weak R_RunExitLists
		lda ___RunAtExit & 0xFFFF
		ora ___RunAtExit >> 16
		beq UserExitLists
		jsl ___RunAtExit
		bra UserExitListsDone
UserExitLists:
		lda R_RunExitLists & 0xFFFF
		ora R_RunExitLists >> 16
		beq UserExitListsDone
		jsl R_RunExitLists
UserExitListsDone:
		lda __fini_end & 0xFFFF
		sta [D + 0]
		lda __fini_end >> 16
		sta [D + 2]
UserFinalise:
		lda [D + 0]
		cmp __fini_start & 0xFFFF
		bne UserFinaliseCall
		lda [D + 2]
		cmp __fini_start >> 16
		beq UserFinalised
UserFinaliseCall:
		sec
		lda [D + 0]
		sbc 4
		sta [D + 0]
		lda [D + 2]
		sbc 0
		sta [D + 2]
		jsr UserCallEntry
		bra UserFinalise
UserFinalised:
		pla
UserExit:
		cop 0
		bra UserExit

		; Calls the routine the init or fini table entry [D + 0] points at.
UserCallEntry:
		.width (A, X), 16
		ldy 0
		lda [[D + 0]:24 + Y]
		sta [D + 4]
		ldy 2
		lda [[D + 0]:24 + Y]
		sta [D + 6]
		phk
		per UserCallReturn - 1
		sep m
		lda [D + 6]
		pha
		rep m
		lda [D + 4]
		dec A
		pha
		rtl
UserCallReturn:
		rts

		; C's _Exit(int): exits the process with its argument, in A as C++ passes it (a function with C linkage takes its
		; first arguments in registers: CCPPTarget's start up has the same two labels), or on the stack.
		.global __Exit.a
__Exit.a:
		.width (A, X), 16
		bra UserExit
		.global __Exit
__Exit:
		.width (A, X), 16
		lda [S + 4]
		bra UserExit

		; The exit lists' heads (the compiler's start up has its own, CCPPTarget::AppendExitLists): empty.
		.section data.ExitLists, keep
		.align 4
		.global __ExitList, __ThreadExitList
__ExitList:
		.long 0
__ThreadExitList:
		.long 0
