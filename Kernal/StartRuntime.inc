; The start up the boot loader (HCP65/Boot/BootStart.asm) and OS.SYS (Start.asm) share, from where each is in MPU RAM
; with the ROM swapped out: native, in kernel mode, interrupts disabled.  Included by both; it needs KERNAL_STACK_TOP,
; KERNAL_NMI_STATE and KERNAL_NMI_TIMER, and gives StartInRAM, KernalIdle, __Exit and KernalNmi.
;
; StartInRAM sets the stack and the direct page, clears bss, sets up the heap (KernalMemory.cpp), calls the init
; table's routines (each global's R_Initialise, in link order) and main, then waits for interrupts.


StartInRAM:
		.width (A, X), 16
		lda KERNAL_STACK_TOP
		tcs
		lda 0
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
		bne StartClearLarge
		tya
		inc A
		lsr A
		beq StartCleared
		tay
		ldx 0
		lda 0
StartClearWord:
		sta [__bss_start + X]:24
		inx
		inx
		dey
		bne StartClearWord
		bra StartCleared
StartClearLarge:
		lda __bss_start & 0xFFFF
		sta [D + 0]
		lda __bss_start >> 16
		sta [D + 2]
StartClear:
		lda [D + 0]
		cmp __bss_end & 0xFFFF
		bne StartClearByte
		lda [D + 2]
		cmp __bss_end >> 16
		beq StartCleared
StartClearByte:
		sep m
		lda 0
		sta [[D + 0]:24]
		rep m
		inc [D + 0]
		bne StartClear
		inc [D + 2]
		bra StartClear
StartCleared:

		; The heap before anything can allocate.
		jsl _KernalInitHeap
		.width (A, X), 16

		lda __init_start & 0xFFFF
		sta [D + 0]
		lda __init_start >> 16
		sta [D + 2]
StartInitialise:
		lda [D + 0]
		cmp __init_end & 0xFFFF
		bne StartInitialiseCall
		lda [D + 2]
		cmp __init_end >> 16
		beq StartMain
StartInitialiseCall:
		jsr StartCallEntry
		clc
		lda [D + 0]
		adc 4
		sta [D + 0]
		lda [D + 2]
		adc 0
		sta [D + 2]
		bra StartInitialise
StartMain:
		jsl _main
		.width (A, X), 16
		.global KernalIdle
KernalIdle:
		wai
		bra KernalIdle

		; Calls the routine the init table entry [D + 0] points at: the return address and the routine - 1 are pushed,
		; and rtl goes to it.
StartCallEntry:
		.width (A, X), 16
		ldy 0
		lda [[D + 0]:24 + Y]
		sta [D + 4]
		ldy 2
		lda [[D + 0]:24 + Y]
		sta [D + 6]
		phk
		per StartCallReturn - 1
		sep m
		lda [D + 6]
		pha
		rep m
		lda [D + 4]
		dec A
		pha
		rtl
StartCallReturn:
		rts

		; C's _Exit(int): the kernal has nowhere to exit to, so it idles.
		.global __Exit
__Exit:
		.width (A, X), 16
		jml KernalIdle


		; An NMI: the timer's tick is counted (guiKernalTicks) and cleared.  Nothing goes on the stack but the frame
		; (an NMI can come at any time, even while an IRQ or COP is changing stacks), and only A is used.
KernalNmi:
		rep m
		sta [_guiKernalNmiSave]:24
		sep m
		lda [KERNAL_NMI_STATE]:24
		and KERNAL_NMI_TIMER
		beq KernalNmiDone
		sta [KERNAL_NMI_STATE]:24
		rep m
		lda [_guiKernalTicks]:24
		inc A
		sta [_guiKernalTicks]:24
		bne KernalNmiDone
		lda [_guiKernalTicks + 2]:24
		inc A
		sta [_guiKernalTicks + 2]:24
KernalNmiDone:
		rep m
		lda [_guiKernalNmiSave]:24
		rti
