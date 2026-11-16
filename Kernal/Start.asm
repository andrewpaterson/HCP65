; OS.SYS's start up.  The boot loader (HCP65/Boot) read OS.SYS into MPU RAM, its first 256 bytes to $00FF00 (the
; stubs and vectors below) and the rest to banks 1 to 3, then jumped to $00FF00: ResetStub, which goes on at Start.
; The processor is native and in kernel mode, with interrupts disabled and the NMI timer's NMI off; ROM and RAM have long
; since swapped (Website docs/hcp65/memory-map.md).  So the start up:
;
;   1. Clears the top 1KB of RAM bank 7: the user mode remap table's pages for process IDs $FC-$FF.
;   2. Sets the stack and the direct page, clears bss, sets up the heap, calls the init table's routines (each global's
;      R_Initialise, in link order) and main, then waits for interrupts.
;
; The maths tables aren't in OS.SYS: the kernal is built without them (MathTables="false") and the start up program
; loads them from the SD card into main memory at $400000 (\OS\Math), where every process has them.
;
; Bank 0 holds only the stack, the direct page, the status (KERNAL_STATUS_ADDRESS), the vectors and a jml stub per
; vector; the code is in bank 1 onwards and runs at the address it is linked at.


KERNAL_STACK_TOP	= 0xBFFF		; The kernal's stack.
KERNAL_REMAP_TABLE	= 0x07FC00		; Process $FC's page of the remap table.
KERNAL_INTERRUPT_DEPTH	= 0x100009	; The processor board's interrupt depth register.
KERNAL_NMI_STATE	= 0x100007		; D0: the timer's NMI (written 1: cleared).
KERNAL_NMI_TIMER	= 0x01


		.section rodata.NativeVectors, at 0xFFE4, keep
		.word CopStub				; $FFE4 COP
		.word BrkStub				; $FFE6 BRK
		.word AbortStub				; $FFE8 ABORT
		.word NmiStub				; $FFEA NMI
		.word 0						; $FFEC
		.word IrqStub				; $FFEE IRQ

		; ResetStub is OS.SYS's entry (its first byte is the jml): the boot loader jumps to it.
		.section text.Stubs, at 0xFF00, keep
ResetStub:
		jml Start
CopStub:
		jml KernalCop
BrkStub:
		jml KernalBrk
AbortStub:
		jml KernalAbort
NmiStub:
		jml KernalNmi
IrqStub:
		jml KernalIrq


		.section text, keep
Start:
		sei
		cld
		rep (m, x)

		; Clear the remap table: a zero word, then copy it up through the table.
		.width (A, X), 16
		lda 0
		sta [KERNAL_REMAP_TABLE]:24
		lda 0x400 - 3
		ldx KERNAL_REMAP_TABLE & 0xFFFF
		ldy (KERNAL_REMAP_TABLE & 0xFFFF) + 2
		mvn 0x07, 0x07
		jml StartInRAM

		.include "StartRuntime.inc"


		; BRK and ABORT are not handled yet: they return at once.  (An interrupt taken in user mode pushed its frame on
		; the process's stack before the vector pull raised the interrupt depth, and RTI drops the depth again before
		; it pulls, so the frame comes back from the same stack.)
KernalBrk:
KernalAbort:
		rti


		; An IRQ (KernalInterrupts.asm dispatches it).  From user mode (the interrupt depth is 1: this vector pull
		; counted it up from 0) the frame is on the process's stack and S still points there, in MPU RAM now: the
		; registers are saved without the stack, as KernalCop does, and the dispatch runs on the kernal's stack below
		; where KernalEnterUser left it.  From kernel mode (a wait let it in) they go on the kernal's stack.
KernalIrq:
		rep (m, x)
		sta [_gauiKernalIrqSave + 0]:24
		sep m
		lda [KERNAL_INTERRUPT_DEPTH]:24
		rep m
		and 0xFF
		cmp 1
		beq KernalIrqFromUser
		lda [_gauiKernalIrqSave + 0]:24
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
KernalIrqFromUser:
		.width (A, X), 16
		txa
		sta [_gauiKernalIrqSave + 2]:24
		tya
		sta [_gauiKernalIrqSave + 4]:24
		tsc
		sta [_gauiKernalIrqSave + 6]:24
		tdc
		sta [_gauiKernalIrqSave + 8]:24
		lda [_gsKernalCall + 12]:24
		tcs
		phb
		lda 0
		tcd
		phk
		plb
		jsl _KernalIrqDispatch
		.width (A, X), 16
		plb
		lda [_gauiKernalIrqSave + 8]:24
		tcd
		lda [_gauiKernalIrqSave + 6]:24
		tcs
		lda [_gauiKernalIrqSave + 4]:24
		tay
		lda [_gauiKernalIrqSave + 2]:24
		tax
		lda [_gauiKernalIrqSave + 0]:24
		rti


		; A kernal call: COP n from a process.  Its frame is on the process's stack and S still points there (in MPU
		; RAM now), so the registers are saved without the stack, then the call runs on the kernal's stack below where
		; KernalEnterUser left it for this process (so a process run during another's execute call has its calls below
		; the other's).  KernalCopCall (C++) reads the signature byte through the remap table.  The exit call goes back
		; to whoever ran the process (KernalEnterUser's caller); every other call returns to the process.
KernalCop:
		rep (m, x)
		sta [_gsKernalCall + 0]:24
		txa
		sta [_gsKernalCall + 2]:24
		tya
		sta [_gsKernalCall + 4]:24
		tsc
		sta [_gsKernalCall + 6]:24
		tdc
		sta [_gsKernalCall + 8]:24
		lda [_gsKernalCall + 12]:24
		tcs
		lda 0
		sep m
		phb
		pla
		rep m
		sta [_gsKernalCall + 10]:24
		lda 0
		tcd
		phk
		plb
		jsl _KernalCopCall
		.width (A, X), 16
		lda [_gsKernalCall + 14]:24
		bne KernalCopExit

		sep m
		lda [_gsKernalCall + 10]:24
		pha
		plb
		rep m
		lda [_gsKernalCall + 8]:24
		tcd
		lda [_gsKernalCall + 6]:24
		tcs
		lda [_gsKernalCall + 4]:24
		tay
		lda [_gsKernalCall + 2]:24
		tax
		lda [_gsKernalCall + 0]:24
		rti

		; The process has exited: back on the kernal's stack, return from KernalEnterUser.  The COP's vector pull
		; left the interrupt depth where KernalEnterUser's RTI found it.
KernalCopExit:
		.width (A, X), 16
		lda [_gsKernalCall + 12]:24
		tcs
		pld
		plb
		rtl


		; extern "C" void KernalEnterUser(void): RTI into the process.  KernalRunExecutable put the frame (P, PC and
		; the program bank) at the top of the process's stack and its S in gsKernalCall.uiUserS.  The RTI drops the
		; interrupt depth to 0 (user mode) before it pulls, so it pulls from the process's stack.
		.global _KernalEnterUser
_KernalEnterUser:
		.width (A, X), 16
		phb
		phd
		tsc
		sta [_gsKernalCall + 12]:24
		lda [_gsKernalCall + 6]:24
		tcs
		rti
