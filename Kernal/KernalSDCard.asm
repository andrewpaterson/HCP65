; The SD Card Board's bit banging for the kernal's SD driver (KernalSDCard.cpp): commands go out on CMD, responses
; come back on it, a bit per SD CLK, through the board's GPIO register (gpuiKernalSDGPIO).  Each bit is a store with CLK
; low then a store with CLK high (the card takes CMD as CLK rises and changes it as CLK falls); a bit is read once CLK
; has risen.  The GPIO values are D0 CLK, D1 CMD, D4 drive CLK, D5 drive CMD.
;
; C functions with the W65C816 compiler's register arguments (CppCompilerLib/W65C816.txt): a pointer in A (low word) and
; X (bank), a 16 bit value in the next free register.  The plain labels are the stack entries.  m and x are 16 bit on
; entry and on return; D and DBR are kept.  The GPIO pointer and the locals are copied to a direct page frame on the
; stack so the stores are [D + n] long indirect.

GPIO_CLK_LOW		= 0x10		; Drive CLK low, CMD let go.
GPIO_CLK_HIGH		= 0x11
GPIO_IDLE_LOW		= 0x32		; Drive CLK low and CMD high.
GPIO_IDLE_HIGH		= 0x33
GPIO_CMD			= 0x02


		.section text.KernalSDCard


; extern "C" void KernalSDSendBits(const uint8* puiBytes, uint16 uiBytes): uiBytes bytes on CMD, high bit first, CMD
; driven.  CLK is left high.
		.global _KernalSDSendBits
_KernalSDSendBits:
		.width (A, X), 16
		lda [S + 8]
		tay
		lda [S + 6]
		tax
		lda [S + 4]

		.global _KernalSDSendBits.axy
_KernalSDSendBits.axy:
		.width (A, X), 16
		phd
		phx							; D + 11: the bytes' bank
		pha							; D + 9: the bytes
		phy							; D + 7: how many
		pea 0						; D + 5: the byte going out
		lda [_gpuiKernalSDGPIO + 2]:24
		pha							; D + 3: GPIO bank
		lda [_gpuiKernalSDGPIO]:24
		pha							; D + 1: GPIO
		tsc
		tcd
		cpy 0
		beq SDSendDone
		ldy 0
		sep m
SDSendByte:
		lda [[D + 9]:24 + Y]
		sta [D + 5]

		; Each bit: shifted out into C, then 0x0C rol asl is GPIO_IDLE_LOW less CMD with C in CMD's place.
		asl [D + 5]
		lda 0x0C
		rol A
		asl A
		sta [[D + 1]:24]
		ora 1
		sta [[D + 1]:24]

		asl [D + 5]
		lda 0x0C
		rol A
		asl A
		sta [[D + 1]:24]
		ora 1
		sta [[D + 1]:24]

		asl [D + 5]
		lda 0x0C
		rol A
		asl A
		sta [[D + 1]:24]
		ora 1
		sta [[D + 1]:24]

		asl [D + 5]
		lda 0x0C
		rol A
		asl A
		sta [[D + 1]:24]
		ora 1
		sta [[D + 1]:24]

		asl [D + 5]
		lda 0x0C
		rol A
		asl A
		sta [[D + 1]:24]
		ora 1
		sta [[D + 1]:24]

		asl [D + 5]
		lda 0x0C
		rol A
		asl A
		sta [[D + 1]:24]
		ora 1
		sta [[D + 1]:24]

		asl [D + 5]
		lda 0x0C
		rol A
		asl A
		sta [[D + 1]:24]
		ora 1
		sta [[D + 1]:24]

		asl [D + 5]
		lda 0x0C
		rol A
		asl A
		sta [[D + 1]:24]
		ora 1
		sta [[D + 1]:24]

		iny
		cpy [D + 7]
		bne SDSendByte
		rep m
SDSendDone:
		tsc
		clc
		adc 12
		tcs
		pld
		rtl


; extern "C" void KernalSDReceiveBits(uint8* puiBytes, uint16 uiBits): clocks uiBits bits in from CMD (CMD let go), high
; bit first, after a start bit the caller has already clocked in.  The start bit (0) is the first byte's top bit, so the
; first byte takes 7 bits and every byte after takes 8: uiBits + 1 must be a multiple of 8.  CLK is left high.
		.global _KernalSDReceiveBits
_KernalSDReceiveBits:
		.width (A, X), 16
		lda [S + 8]
		tay
		lda [S + 6]
		tax
		lda [S + 4]

		.global _KernalSDReceiveBits.axy
_KernalSDReceiveBits.axy:
		.width (A, X), 16
		phd
		phx							; D + 9: the bytes' bank
		pha							; D + 7: the bytes
		pea 2						; D + 5: the byte coming in, with a marker bit above the 7 still to come
		lda [_gpuiKernalSDGPIO + 2]:24
		pha							; D + 3: GPIO bank
		lda [_gpuiKernalSDGPIO]:24
		pha							; D + 1: GPIO
		tsc
		tcd
		tyx
		beq SDReceiveDone
		ldy 0
		sep m
SDReceiveBit:
		lda GPIO_CLK_LOW
		sta [[D + 1]:24]
		lda GPIO_CLK_HIGH
		sta [[D + 1]:24]
		lda [[D + 1]:24]
		lsr A
		lsr A
		rol [D + 5]
		bcc SDReceiveNext
		; The marker came out: the byte is complete.
		lda [D + 5]
		sta [[D + 7]:24 + Y]
		iny
		lda 1
		sta [D + 5]
SDReceiveNext:
		dex
		bne SDReceiveBit
		rep m
SDReceiveDone:
		tsc
		clc
		adc 10
		tcs
		pld
		rtl


; extern "C" uint16 KernalSDWaitStart(uint16 uiClocks): clocks with CMD let go until the card pulls CMD low (a
; response's start bit), at most uiClocks times.  Returns 0 if it never did, otherwise not 0.  CLK is left high.
		.global _KernalSDWaitStart
_KernalSDWaitStart:
		.width (A, X), 16
		lda [S + 4]

		.global _KernalSDWaitStart.a
_KernalSDWaitStart.a:
		.width (A, X), 16
		phd
		tax
		lda [_gpuiKernalSDGPIO + 2]:24
		pha							; D + 3: GPIO bank
		lda [_gpuiKernalSDGPIO]:24
		pha							; D + 1: GPIO
		tsc
		tcd
		cpx 0
		beq SDWaitDone
		sep m
SDWaitClock:
		lda GPIO_CLK_LOW
		sta [[D + 1]:24]
		lda GPIO_CLK_HIGH
		sta [[D + 1]:24]
		lda [[D + 1]:24]
		and GPIO_CMD
		beq SDWaitFound
		dex
		bne SDWaitClock
SDWaitFound:
		rep m
SDWaitDone:
		pla
		pla
		txa
		pld
		rtl


; extern "C" void KernalSDClocks(uint16 uiClocks): uiClocks clocks with CMD driven high.  CLK is left high.
		.global _KernalSDClocks
_KernalSDClocks:
		.width (A, X), 16
		lda [S + 4]

		.global _KernalSDClocks.a
_KernalSDClocks.a:
		.width (A, X), 16
		phd
		tax
		lda [_gpuiKernalSDGPIO + 2]:24
		pha							; D + 3: GPIO bank
		lda [_gpuiKernalSDGPIO]:24
		pha							; D + 1: GPIO
		tsc
		tcd
		cpx 0
		beq SDClocksDone
		sep m
SDClock:
		lda GPIO_IDLE_LOW
		sta [[D + 1]:24]
		lda GPIO_IDLE_HIGH
		sta [[D + 1]:24]
		dex
		bne SDClock
		rep m
SDClocksDone:
		pla
		pla
		pld
		rtl
