; The kernal's IRQ dispatch (KernalInterrupts.h and KernalInterrupts.txt).  Start.asm's KernalIrq saves the registers
; and calls KernalIrqDispatch on the kernal's stack with D 0.  The dispatch only reads and writes MPU RAM and the device
; registers, never main memory, so it can run while the SD Card Board's DMA has the main memory bus.
;
; gasKernalInterrupts[slot] is 16 bytes: the handler (+0, a C function's stack entry: the context is pushed and the
; caller removes it), the context (+4), the device's Flags register (+8), trusted (+12) and pending (+14).

INTERRUPT_HANDLER	= 0
INTERRUPT_CONTEXT	= 4
INTERRUPT_FLAGS		= 8
INTERRUPT_TRUSTED	= 12
INTERRUPT_PENDING	= 14

REGISTER_INTERRUPTS_LOW		= 0x100000		; D1-D7: devices 1-7, 0 interrupting.
REGISTER_INTERRUPTS_HIGH	= 0x100002		; D0-D3: devices 16-19.
REGISTER_ENABLE_LOW			= 0x100003		; D1-D7: devices 1-7, 0 held in reset.
REGISTER_ENABLE_HIGH		= 0x100005		; D0-D3: devices 16-19.
STATUS_STRAY_INTERRUPTS		= 32			; gsKernalStatus.uiStrayInterrupts.

; The frame (on the stack, D = S): the slots still to look at, the slot, a pointer.
L_LOW		= 1				; Devices 1-7 interrupting (or pending), one bit each from D1.
L_HIGH		= 3				; Devices 16-19, from D0.
L_SLOT		= 5
L_POINTER	= 7				; 4 bytes.
L_MODE		= 11			; 0: dispatch the interrupting slots, else run the pending ones.
L_SIZE		= 12


		.section text.KernalInterrupts
		.relax


; extern "C" void KernalIrqDispatch(void): calls the handler of each device that is interrupting.
		.global _KernalIrqDispatch
_KernalIrqDispatch:
		.width (A, X), 16
		phd
		tsc
		sec
		sbc L_SIZE
		tcs
		tcd
		sep m
		lda [REGISTER_INTERRUPTS_LOW]:24
		eor 0xFF
		and 0xFE
		sta [D + L_LOW]
		stz [D + L_LOW + 1]
		lda [REGISTER_INTERRUPTS_HIGH]:24
		eor 0xFF
		and 0x0F
		sta [D + L_HIGH]
		stz [D + L_HIGH + 1]
		rep m
		stz [D + L_MODE]
		jsr EachSlot
		tdc
		clc
		adc L_SIZE
		tcs
		pld
		rtl


; extern "C" void KernalRunPendingInterrupts(void): with I set, calls the handlers deferred while the main memory bus was
; busy.
		.global _KernalRunPendingInterrupts
_KernalRunPendingInterrupts:
		.width (A, X), 16
		php
		sei
		phd
		tsc
		sec
		sbc L_SIZE
		tcs
		tcd
		stz [D + L_LOW]
		stz [D + L_HIGH]
		ldx 1 * 16
		ldy 2
PendingLow:
		lda [_gasKernalInterrupts + INTERRUPT_PENDING + X]:24
		beq PendingLowNext
		tya
		ora [D + L_LOW]
		sta [D + L_LOW]
PendingLowNext:
		tya
		asl A
		tay
		txa
		clc
		adc 16
		tax
		cpx 8 * 16
		bne PendingLow
		ldx 16 * 16
		ldy 1
PendingHigh:
		lda [_gasKernalInterrupts + INTERRUPT_PENDING + X]:24
		beq PendingHighNext
		tya
		ora [D + L_HIGH]
		sta [D + L_HIGH]
PendingHighNext:
		tya
		asl A
		tay
		txa
		clc
		adc 16
		tax
		cpx 20 * 16
		bne PendingHigh
		lda 1
		sta [D + L_MODE]
		jsr EachSlot
		tdc
		clc
		adc L_SIZE
		tcs
		pld
		plp
		rtl


; For each slot whose bit is set in L_LOW and L_HIGH: DispatchSlot (L_MODE 0) or RunPendingSlot, with A the slot.
; They keep nothing but the frame.
EachSlot:
		.width (A, X), 16
		lda 1
		sta [D + L_SLOT]
		lsr [D + L_LOW]
EachLow:
		lsr [D + L_LOW]
		bcc EachLowNext
		jsr SlotRoutine
EachLowNext:
		lda [D + L_SLOT]
		inc A
		sta [D + L_SLOT]
		cmp 8
		bne EachLow
		lda 16
		sta [D + L_SLOT]
EachHigh:
		lsr [D + L_HIGH]
		bcc EachHighNext
		jsr SlotRoutine
EachHighNext:
		lda [D + L_SLOT]
		inc A
		sta [D + L_SLOT]
		cmp 20
		bne EachHigh
		rts

SlotRoutine:
		.width (A, X), 16
		lda [D + L_MODE]
		bne SlotRoutinePending
		lda [D + L_SLOT]
		bra DispatchSlot
SlotRoutinePending:
		lda [D + L_SLOT]
		bra RunPendingSlot


; A device in slot A is interrupting: its handler is called, or, while the main memory bus is busy and the handler isn't
; trusted, the device is quietened and the handler left pending.  With no handler the device is reset.
DispatchSlot:
		.width (A, X), 16
		jsr SlotOffset
		lda [_gasKernalInterrupts + INTERRUPT_HANDLER + X]:24
		ora [_gasKernalInterrupts + INTERRUPT_HANDLER + 2 + X]:24
		beq DispatchStray
		sep m
		lda [_gbKernalMainBusBusy]:24
		rep m
		and 0xFF
		beq CallHandler
		lda [_gasKernalInterrupts + INTERRUPT_TRUSTED + X]:24
		bne CallHandler
		; Quieten it: its Flags register with D7 (interrupt enable) written 0.
		lda [_gasKernalInterrupts + INTERRUPT_FLAGS + X]:24
		sta [D + L_POINTER]
		lda [_gasKernalInterrupts + INTERRUPT_FLAGS + 2 + X]:24
		sta [D + L_POINTER + 2]
		sep m
		lda [[D + L_POINTER]:24]
		and 0x7F
		sta [[D + L_POINTER]:24]
		rep m
		lda 1
		sta [_gasKernalInterrupts + INTERRUPT_PENDING + X]:24
		rts

DispatchStray:
		.width (A, X), 16
		; Reset the device (0 then 1 in its enable register bit) and note its slot.
		lda [D + L_SLOT]
		and 0x0F
		tay
		lda 1
DispatchStrayBit:
		cpy 0
		beq DispatchStrayReset
		asl A
		dey
		bra DispatchStrayBit
DispatchStrayReset:
		sta [D + L_POINTER]
		lda [D + L_SLOT]
		cmp 16
		bcs DispatchStrayHigh
		sep m
		lda [REGISTER_ENABLE_LOW]:24
		pha
		lda [D + L_POINTER]
		eor 0xFF
		and [S + 1]
		sta [REGISTER_ENABLE_LOW]:24
		pla
		sta [REGISTER_ENABLE_LOW]:24
		rep m
		bra DispatchStrayNoted
DispatchStrayHigh:
		.width (A, X), 16
		sep m
		lda [REGISTER_ENABLE_HIGH]:24
		pha
		lda [D + L_POINTER]
		eor 0xFF
		and [S + 1]
		sta [REGISTER_ENABLE_HIGH]:24
		pla
		sta [REGISTER_ENABLE_HIGH]:24
		rep m
DispatchStrayNoted:
		.width (A, X), 16
		; uiStrayInterrupts |= 1 << slot.
		lda [D + L_SLOT]
		tay
		lda 1
		sta [D + L_POINTER]
		stz [D + L_POINTER + 2]
DispatchStrayShift:
		cpy 0
		beq DispatchStrayOr
		asl [D + L_POINTER]
		rol [D + L_POINTER + 2]
		dey
		bra DispatchStrayShift
DispatchStrayOr:
		lda [_gsKernalStatus + STATUS_STRAY_INTERRUPTS]:24
		ora [D + L_POINTER]
		sta [_gsKernalStatus + STATUS_STRAY_INTERRUPTS]:24
		lda [_gsKernalStatus + STATUS_STRAY_INTERRUPTS + 2]:24
		ora [D + L_POINTER + 2]
		sta [_gsKernalStatus + STATUS_STRAY_INTERRUPTS + 2]:24
		rts


; A slot deferred while the bus was busy: its handler is called now (it turns its device's interrupt enable back on).
RunPendingSlot:
		.width (A, X), 16
		jsr SlotOffset
		lda 0
		sta [_gasKernalInterrupts + INTERRUPT_PENDING + X]:24
		lda [_gasKernalInterrupts + INTERRUPT_HANDLER + X]:24
		ora [_gasKernalInterrupts + INTERRUPT_HANDLER + 2 + X]:24
		bne CallHandler
		rts


; Calls the handler of the slot at X (its offset in the table) with its context, as a C function pointer is called: the
; context pushed, then a jsl to the handler (made with rtl: its address less one is pushed), then the context removed.
CallHandler:
		.width (A, X), 16
		lda [_gasKernalInterrupts + INTERRUPT_CONTEXT + 2 + X]:24
		pha
		lda [_gasKernalInterrupts + INTERRUPT_CONTEXT + X]:24
		pha
		phk
		per CallHandlerReturn - 1
		sep m
		lda [_gasKernalInterrupts + INTERRUPT_HANDLER + 2 + X]:24
		pha
		rep m
		lda [_gasKernalInterrupts + INTERRUPT_HANDLER + X]:24
		dec A
		pha
		lda [_guiKernalInterrupts]:24
		inc A
		sta [_guiKernalInterrupts]:24
		bne CallHandlerJump
		lda [_guiKernalInterrupts + 2]:24
		inc A
		sta [_guiKernalInterrupts + 2]:24
CallHandlerJump:
		rtl
CallHandlerReturn:
		.width (A, X), 16
		pla
		pla
		rts


; X = the slot in A's offset in gasKernalInterrupts (16 bytes a slot).
SlotOffset:
		.width (A, X), 16
		asl A
		asl A
		asl A
		asl A
		tax
		rts


; extern "C" void KernalWaitForInterrupt(void): with I set, waits for an IRQ (or the timer's NMI), then lets a pending
; IRQ be taken (and dispatched) before setting I again.
		.global _KernalWaitForInterrupt
_KernalWaitForInterrupt:
		.width (A, X), 16
		wai
		cli
		nop
		sei
		rtl


		.global _KernalInterruptsOn
_KernalInterruptsOn:
		.width (A, X), 16
		cli
		rtl


		.global _KernalInterruptsOff
_KernalInterruptsOff:
		.width (A, X), 16
		sei
		rtl
