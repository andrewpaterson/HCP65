; The HCP65 boot loader's start up (Boot.cpp is the rest).
;
; At reset MPU ROM is at $000000-$07FFFF and MPU RAM at $080000-$0FFFFF.  The first xce into native mode swaps them for
; good (the Native Latch): RAM at $000000, ROM at $080000.  Before the swap bank 0 is ROM, so there is no stack and no
; direct page, and emulation mode's 8 bit index registers can't copy 128KB.  The boot loader is linked to run from MPU
; RAM banks 4 and 5, out of the way of OS.SYS, which it reads into banks 1 to 3 (and $00FF00); its image has it at the
; same offsets in the ROM.  So the start up:
;
;   1. In emulation mode, copies the 4 byte jml at StartSwapped into RAM where StartSwapped will be once RAM is at 0.
;   2. Goes native.  The next instruction is fetched from RAM: that jml, which goes on in ROM at StartFromROM + $80000.
;   3. Copies ROM banks 4 and 5 (the boot loader's code and data) to RAM with mvn, and the page of stubs and vectors at
;      $FF00, so that interrupts reach the boot loader.
;   4. Goes on in RAM at StartInRAM (StartRuntime.inc, which OS.SYS's start up shares).
;
; It only runs in kernel mode: an IRQ is dispatched on the boot loader's stack, and COP, BRK and ABORT return at once.


KERNAL_STACK_TOP	= 0xBFFF		; The boot loader's stack (and then the kernal's).
KERNAL_ROM_OFFSET	= 0x80000		; Where an address's ROM byte is once ROM and RAM have swapped.
KERNAL_NMI_STATE	= 0x100007		; D0: the timer's NMI (written 1: cleared).
KERNAL_NMI_TIMER	= 0x01


		.section rodata.NativeVectors, at 0xFFE4, keep
		.word OtherStub				; $FFE4 COP
		.word OtherStub				; $FFE6 BRK
		.word OtherStub				; $FFE8 ABORT
		.word NmiStub				; $FFEA NMI
		.word 0						; $FFEC
		.word IrqStub				; $FFEE IRQ

		.section rodata.EmulationVectors, at 0xFFF4, keep
		.word OtherStub				; $FFF4 COP
		.word 0						; $FFF6
		.word OtherStub				; $FFF8 ABORT
		.word NmiStub				; $FFFA NMI
		.word ResetStub				; $FFFC RESET
		.word OtherStub				; $FFFE IRQ and BRK

		.section text.Stubs, at 0xFF00, keep
ResetStub:
		jml Start
OtherStub:
		jml BootReturn
NmiStub:
		jml KernalNmi
IrqStub:
		jml BootIrq


		.section text, keep
Start:
		.width (A, X), 8
		sei
		cld
		lda [StartSwapped]:24
		sta [StartSwapped + KERNAL_ROM_OFFSET]:24
		lda [StartSwapped + 1]:24
		sta [StartSwapped + KERNAL_ROM_OFFSET + 1]:24
		lda [StartSwapped + 2]:24
		sta [StartSwapped + KERNAL_ROM_OFFSET + 2]:24
		lda [StartSwapped + 3]:24
		sta [StartSwapped + KERNAL_ROM_OFFSET + 3]:24
		clc
		xce
StartSwapped:
		jml StartFromROM + KERNAL_ROM_OFFSET

		; Running from ROM at $0Cxxxx.  Each mvn of a bank copies 64KB and leaves C = $FFFF and X = Y = 0 for the next:
		; the boot loader to MPU RAM banks 4 and 5, then the stubs and vectors to $00FF00.
StartFromROM:
		rep (m, x)
		.width (A, X), 16
		lda 0xFFFF
		ldx 0
		ldy 0
		mvn 0x0C, 0x04
		mvn 0x0D, 0x05
		lda 0xFF
		ldx 0xFF00
		ldy 0xFF00
		mvn 0x08, 0x00
		jml StartInRAM

		.include "../Kernal/StartRuntime.inc"


		; COP, BRK and ABORT: nothing in the boot loader causes them.
BootReturn:
		rti


		; An IRQ, in kernel mode (KernalInterrupts.asm dispatches it): the registers go on the boot loader's stack.
BootIrq:
		rep (m, x)
		.width (A, X), 16
		pha
		phx
		phy
		phd
		phb
		lda 0
		tcd
		phk
		plb
		jsl _KernalIrqDispatch
		.width (A, X), 16
		plb
		pld
		ply
		plx
		pla
		rti
