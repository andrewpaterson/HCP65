; The kernal's copies in main memory (KernalDriver.h's KernalMainCopy): a Blitter Board's if there is one and the copy is
; long enough to be worth its registers being written, the processor's if not.
;
; A C function with the W65C816 compiler's register arguments (CppCompilerLib/W65C816.txt): the destination in A (low
; word) and X (bank), the count in Y, and the source then the transparent colour above the return address.  Only the
; register entry is here, so it can't be called through a pointer.  m and x are 16 bit on entry and on return; D and DBR
; are kept.  The pointers are in a direct page frame on the stack, so the bytes are [D + n] long indirect.

; The fewest bytes a Blitter Board is given, of each kind of copy: under them the processor is quicker.
COPY_BOARD_LEAST_OPAQUE			= 15
COPY_BOARD_LEAST_TRANSPARENT	= 9

; The Blitter Board's registers (Drivers/BlitterBoard/BlitterBoard.h) and its Flags for the two kinds of copy.
BLITTER_FLAGS				= 0x00
BLITTER_TRANSPARENT_COLOUR	= 0x02
BLITTER_DEST				= 0x04
BLITTER_START				= 0x07
BLITTER_SOURCE				= 0x08
BLITTER_COUNT				= 0x0C
BLITTER_COUNT_STOP			= 0x0E
BLITTER_OPAQUE				= 0x40
BLITTER_TRANSPARENT			= 0x41

COPY_OPAQUE					= 0xFFFF	; KERNAL_COPY_OPAQUE.

; The frame.
C_BOARD			= 1			; The board's registers (a 4 byte pointer), 0 if there is none.
C_COUNT			= 5
C_DEST			= 7			; A 4 byte pointer.
C_PUSHED		= 10		; What is pushed after the caller's D.
C_SOURCE		= 16		; A 4 byte pointer: the first argument above the return address.
C_TRANSPARENT	= 20


		.section text.KernalCopy
		.relax


; void KernalMainCopy(uint8* puiDest, const uint8* puiSource, uint16 uiCount, uint16 uiTransparent)
		.global _KernalMainCopy__FPhPKhjj.axssy
_KernalMainCopy__FPhPKhjj.axssy:
		.width (A, X), 16
		phd
		phx
		pha
		phy
		lda [_gpuiKernalBlitter + 2]:24
		pha
		lda [_gpuiKernalBlitter]:24
		pha
		tsc
		tcd
		lda [D + C_COUNT]
		beq CopyDone
		lda [D + C_BOARD]
		ora [D + C_BOARD + 2]
		beq CopyProcessor
		lda [D + C_TRANSPARENT]
		cmp COPY_OPAQUE
		beq CopyBoardOpaque
		lda [D + C_COUNT]
		cmp COPY_BOARD_LEAST_TRANSPARENT
		bcc CopyProcessor
		sep m
		ldy BLITTER_TRANSPARENT_COLOUR
		lda [D + C_TRANSPARENT]
		sta [[D + C_BOARD]:24 + Y]
		lda BLITTER_TRANSPARENT
		bra CopyBoard
CopyBoardOpaque:
		.width A, 16
		lda [D + C_COUNT]
		cmp COPY_BOARD_LEAST_OPAQUE
		bcc CopyProcessor
		sep m
		lda BLITTER_OPAQUE

; The board's: its Flags (in A), the count's stop (its start stays 0), the addresses and Start.  It has main memory until
; its count is the stop: only the board is read meanwhile, and the interrupt handlers that would touch main memory are
; put off (gbKernalMainBusBusy).
CopyBoard:
		.width A, 8
		ldy BLITTER_FLAGS
		sta [[D + C_BOARD]:24 + Y]
		ldy BLITTER_COUNT_STOP
		lda [D + C_COUNT]
		sta [[D + C_BOARD]:24 + Y]
		iny
		lda [D + C_COUNT + 1]
		sta [[D + C_BOARD]:24 + Y]
		ldy BLITTER_SOURCE
		lda [D + C_SOURCE]
		sta [[D + C_BOARD]:24 + Y]
		iny
		lda [D + C_SOURCE + 1]
		sta [[D + C_BOARD]:24 + Y]
		iny
		lda [D + C_SOURCE + 2]
		sta [[D + C_BOARD]:24 + Y]
		ldy BLITTER_DEST
		lda [D + C_DEST]
		sta [[D + C_BOARD]:24 + Y]
		iny
		lda [D + C_DEST + 1]
		sta [[D + C_BOARD]:24 + Y]
		iny
		lda [D + C_DEST + 2]
		sta [[D + C_BOARD]:24 + Y]
		lda 1
		sta [_gbKernalMainBusBusy]:24
		ldy BLITTER_START
		sta [[D + C_BOARD]:24 + Y]
CopyWait:
		.width A, 8
		ldy BLITTER_COUNT
		lda [[D + C_BOARD]:24 + Y]
		cmp [D + C_COUNT]
		bne CopyWait
		iny
		lda [[D + C_BOARD]:24 + Y]
		cmp [D + C_COUNT + 1]
		bne CopyWait
		lda 0
		sta [_gbKernalMainBusBusy]:24
		rep m
		bra CopyDone

; The processor's: an opaque copy a word at a time then its odd byte, a transparent one a byte at a time.
CopyProcessor:
		.width (A, X), 16
		ldy 0
		lda [D + C_TRANSPARENT]
		cmp COPY_OPAQUE
		bne CopyTransparent
		lda [D + C_COUNT]
		lsr A
		beq CopyOdd
		tax
CopyWord:
		.width A, 16
		lda [[D + C_SOURCE]:24 + Y]
		sta [[D + C_DEST]:24 + Y]
		iny
		iny
		dex
		bne CopyWord
CopyOdd:
		.width A, 16
		lda [D + C_COUNT]
		lsr A
		bcc CopyDone
		sep m
		lda [[D + C_SOURCE]:24 + Y]
		sta [[D + C_DEST]:24 + Y]
		rep m
		bra CopyDone
CopyTransparent:
		.width A, 16
		ldx [D + C_COUNT]
		sep m
CopyByte:
		.width A, 8
		lda [[D + C_SOURCE]:24 + Y]
		cmp [D + C_TRANSPARENT]
		beq CopyNone
		sta [[D + C_DEST]:24 + Y]
CopyNone:
		.width A, 8
		iny
		dex
		bne CopyByte
		rep m
CopyDone:
		.width (A, X), 16
		tdc
		clc
		adc C_PUSHED
		tcs
		pld
		rtl
