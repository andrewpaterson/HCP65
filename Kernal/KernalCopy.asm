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


; void KernalMainScaledCopy(uint8* puiDest, const uint8* puiSource, uint16 uiStart, uint16 uiCount, uint16 uiTransparent):
; the Graphics Board's copy of uiCount bytes, each from the source plus the offset of its count, the counts from uiStart.
; The destination is in A and X, the start in Y, and the source, the count and the transparent colour above the return
; address.  As CopyBoard, but that the count's start is written too (the Blitter Board's stays 0), and its stop is the
; start plus the count, wrapping at 32768.
S_BOARD			= 1			; gpuiKernalGraphics.
S_STOP			= 5			; The start (Y), then the stop once the start is written.
S_DEST			= 7
S_PUSHED		= 10
S_SOURCE		= 16
S_COUNT			= 20
S_TRANSPARENT	= 22
GRAPHICS_COUNT_MASK	= 0x7FFF

		.global _KernalMainScaledCopy__FPhPKhjjj.axssy
_KernalMainScaledCopy__FPhPKhjjj.axssy:
		.width (A, X), 16
		phd
		phx
		pha
		phy
		lda [_gpuiKernalGraphics + 2]:24
		pha
		lda [_gpuiKernalGraphics]:24
		pha
		tsc
		tcd
		lda [D + S_COUNT]
		beq ScaledDone
		sep m
		ldy BLITTER_COUNT
		lda [D + S_STOP]
		sta [[D + S_BOARD]:24 + Y]
		iny
		lda [D + S_STOP + 1]
		sta [[D + S_BOARD]:24 + Y]
		rep m
		lda [D + S_STOP]
		clc
		adc [D + S_COUNT]
		and GRAPHICS_COUNT_MASK
		sta [D + S_STOP]
		sep m
		ldy BLITTER_COUNT_STOP
		lda [D + S_STOP]
		sta [[D + S_BOARD]:24 + Y]
		iny
		lda [D + S_STOP + 1]
		sta [[D + S_BOARD]:24 + Y]
		lda [D + S_TRANSPARENT + 1]
		bne ScaledOpaque
		ldy BLITTER_TRANSPARENT_COLOUR
		lda [D + S_TRANSPARENT]
		sta [[D + S_BOARD]:24 + Y]
		lda BLITTER_TRANSPARENT
		bra ScaledFlags
ScaledOpaque:
		.width A, 8
		lda BLITTER_OPAQUE
ScaledFlags:
		.width A, 8
		ldy BLITTER_FLAGS
		sta [[D + S_BOARD]:24 + Y]
		ldy BLITTER_SOURCE
		lda [D + S_SOURCE]
		sta [[D + S_BOARD]:24 + Y]
		iny
		lda [D + S_SOURCE + 1]
		sta [[D + S_BOARD]:24 + Y]
		iny
		lda [D + S_SOURCE + 2]
		sta [[D + S_BOARD]:24 + Y]
		ldy BLITTER_DEST
		lda [D + S_DEST]
		sta [[D + S_BOARD]:24 + Y]
		iny
		lda [D + S_DEST + 1]
		sta [[D + S_BOARD]:24 + Y]
		iny
		lda [D + S_DEST + 2]
		sta [[D + S_BOARD]:24 + Y]
		lda 1
		sta [_gbKernalMainBusBusy]:24
		ldy BLITTER_START
		sta [[D + S_BOARD]:24 + Y]
ScaledWait:
		.width A, 8
		ldy BLITTER_COUNT
		lda [[D + S_BOARD]:24 + Y]
		cmp [D + S_STOP]
		bne ScaledWait
		iny
		lda [[D + S_BOARD]:24 + Y]
		cmp [D + S_STOP + 1]
		bne ScaledWait
		lda 0
		sta [_gbKernalMainBusBusy]:24
		rep m
ScaledDone:
		.width (A, X), 16
		tdc
		clc
		adc S_PUSHED
		tcs
		pld
		rtl

; void KernalMainScaledRows(SKernalScaledRows* psRows): Graphics Board copies of psRows' rows (KernalDriver.h) while
; they are in the bank its offsets' range is: the board's Flags, transparent colour and count's start and stop once,
; then for each row its source and destination, Start and the wait; then the next drawn row, whose image row is found
; by adding the step to the remainder (and the row step to the offset each time it passes the denominator).  The block
; is copied into the direct page frame, worked on there, and what changes (the destination, the offset, the remainder
; and the rows) copied back where it stopped.  The pointer is in A and X.
SR_DEST			= 0			; SKernalScaledRows.
SR_BASE			= 4
SR_OFFSET		= 8
SR_LOW			= 12
SR_HIGH			= 16
SR_ROW_STEP		= 20
SR_REMAINDER	= 24
SR_STEP			= 28
SR_DENOMINATOR	= 32
SR_DEST_STRIDE	= 36
SR_ROWS			= 38
SR_START		= 40
SR_COUNT		= 42
SR_TRANSPARENT	= 44
SR_STOP			= 46
SR_SIZE			= 48

R_BOARD			= 1			; The frame: gpuiKernalGraphics,
R_BLOCK			= 5			; the block's copy,
R_SOURCE		= R_BLOCK + SR_SIZE			; a row's source,
R_ROWS			= R_SOURCE + 4				; the block.
R_RESERVED		= SR_SIZE + 4
R_PUSHED		= 4 + R_RESERVED + 4

B_DEST			= R_BLOCK + SR_DEST
B_BASE			= R_BLOCK + SR_BASE
B_OFFSET		= R_BLOCK + SR_OFFSET
B_LOW			= R_BLOCK + SR_LOW
B_HIGH			= R_BLOCK + SR_HIGH
B_ROW_STEP		= R_BLOCK + SR_ROW_STEP
B_REMAINDER		= R_BLOCK + SR_REMAINDER
B_STEP			= R_BLOCK + SR_STEP
B_DENOMINATOR	= R_BLOCK + SR_DENOMINATOR
B_DEST_STRIDE	= R_BLOCK + SR_DEST_STRIDE
B_ROWS			= R_BLOCK + SR_ROWS
B_START			= R_BLOCK + SR_START
B_COUNT			= R_BLOCK + SR_COUNT
B_TRANSPARENT	= R_BLOCK + SR_TRANSPARENT
B_STOP			= R_BLOCK + SR_STOP

		.global _KernalMainScaledRows__FPC17SKernalScaledRows.ax
_KernalMainScaledRows__FPC17SKernalScaledRows.ax:
		.width (A, X), 16
		phd
		phx
		pha
		tsc
		sec
		sbc R_RESERVED
		tcs
		lda [_gpuiKernalGraphics + 2]:24
		pha
		lda [_gpuiKernalGraphics]:24
		pha
		tsc
		tcd

		; The block into the frame, a word at a time.
		ldy SR_SIZE - 2
RowsCopyIn:
		.width (A, X), 16
		lda [[D + R_ROWS]:24 + Y]
		tyx
		sta [D + R_BLOCK + X]
		dey
		dey
		bpl RowsCopyIn
		lda [D + B_ROWS]
		bne RowsBoard
		brl RowsDone

		; Once: the count's start and stop (the start plus the count, wrapping at 32768), the transparent colour and
		; the Flags.
RowsBoard:
		.width (A, X), 16
		lda [D + B_START]
		clc
		adc [D + B_COUNT]
		and GRAPHICS_COUNT_MASK
		sta [D + B_STOP]
		sep m
		ldy BLITTER_COUNT
		lda [D + B_START]
		sta [[D + R_BOARD]:24 + Y]
		iny
		lda [D + B_START + 1]
		sta [[D + R_BOARD]:24 + Y]
		ldy BLITTER_COUNT_STOP
		lda [D + B_STOP]
		sta [[D + R_BOARD]:24 + Y]
		iny
		lda [D + B_STOP + 1]
		sta [[D + R_BOARD]:24 + Y]
		lda [D + B_TRANSPARENT + 1]
		bne RowsOpaque
		ldy BLITTER_TRANSPARENT_COLOUR
		lda [D + B_TRANSPARENT]
		sta [[D + R_BOARD]:24 + Y]
		lda BLITTER_TRANSPARENT
		bra RowsFlags
RowsOpaque:
		.width A, 8
		lda BLITTER_OPAQUE
RowsFlags:
		.width A, 8
		ldy BLITTER_FLAGS
		sta [[D + R_BOARD]:24 + Y]
		rep m

		; Each row while there are rows and its offset is from the low to the high.
RowsNext:
		.width (A, X), 16
		lda [D + B_ROWS]
		bne RowsLow
		brl RowsDone
RowsLow:
		.width (A, X), 16
		lda [D + B_OFFSET + 2]
		cmp [D + B_LOW + 2]
		bcc RowsOut
		bne RowsHigh
		lda [D + B_OFFSET]
		cmp [D + B_LOW]
		bcc RowsOut
RowsHigh:
		.width (A, X), 16
		lda [D + B_HIGH + 2]
		cmp [D + B_OFFSET + 2]
		bcc RowsOut
		bne RowsCopy
		lda [D + B_HIGH]
		cmp [D + B_OFFSET]
		bcs RowsCopy
RowsOut:
		.width (A, X), 16
		brl RowsDone

		; The row's source (the base plus its offset) and destination, Start, and the wait for the count to reach the
		; stop.  The board has main memory meanwhile.
RowsCopy:
		.width (A, X), 16
		lda [D + B_BASE]
		clc
		adc [D + B_OFFSET]
		sta [D + R_SOURCE]
		lda [D + B_BASE + 2]
		adc [D + B_OFFSET + 2]
		sta [D + R_SOURCE + 2]
		sep m
		ldy BLITTER_SOURCE
		lda [D + R_SOURCE]
		sta [[D + R_BOARD]:24 + Y]
		iny
		lda [D + R_SOURCE + 1]
		sta [[D + R_BOARD]:24 + Y]
		iny
		lda [D + R_SOURCE + 2]
		sta [[D + R_BOARD]:24 + Y]
		ldy BLITTER_DEST
		lda [D + B_DEST]
		sta [[D + R_BOARD]:24 + Y]
		iny
		lda [D + B_DEST + 1]
		sta [[D + R_BOARD]:24 + Y]
		iny
		lda [D + B_DEST + 2]
		sta [[D + R_BOARD]:24 + Y]
		lda 1
		sta [_gbKernalMainBusBusy]:24
		ldy BLITTER_START
		sta [[D + R_BOARD]:24 + Y]
RowsWait:
		.width A, 8
		ldy BLITTER_COUNT
		lda [[D + R_BOARD]:24 + Y]
		cmp [D + B_STOP]
		bne RowsWait
		iny
		lda [[D + R_BOARD]:24 + Y]
		cmp [D + B_STOP + 1]
		bne RowsWait
		lda 0
		sta [_gbKernalMainBusBusy]:24
		rep m

		; The next drawn row: down the picture a row, one fewer to draw, and the step added to the remainder.
		lda [D + B_DEST]
		clc
		adc [D + B_DEST_STRIDE]
		sta [D + B_DEST]
		lda [D + B_DEST + 2]
		adc 0
		sta [D + B_DEST + 2]
		dec [D + B_ROWS]
		lda [D + B_REMAINDER]
		clc
		adc [D + B_STEP]
		sta [D + B_REMAINDER]
		lda [D + B_REMAINDER + 2]
		adc [D + B_STEP + 2]
		sta [D + B_REMAINDER + 2]

		; While the remainder is the denominator or more: less the denominator, and the offset on a row.
RowsRemainder:
		.width (A, X), 16
		lda [D + B_REMAINDER + 2]
		cmp [D + B_DENOMINATOR + 2]
		bcc RowsStepped
		bne RowsSubtract
		lda [D + B_REMAINDER]
		cmp [D + B_DENOMINATOR]
		bcc RowsStepped
RowsSubtract:
		.width (A, X), 16
		lda [D + B_REMAINDER]
		sec
		sbc [D + B_DENOMINATOR]
		sta [D + B_REMAINDER]
		lda [D + B_REMAINDER + 2]
		sbc [D + B_DENOMINATOR + 2]
		sta [D + B_REMAINDER + 2]
		lda [D + B_OFFSET]
		clc
		adc [D + B_ROW_STEP]
		sta [D + B_OFFSET]
		lda [D + B_OFFSET + 2]
		adc [D + B_ROW_STEP + 2]
		sta [D + B_OFFSET + 2]
		bra RowsRemainder
RowsStepped:
		.width (A, X), 16
		brl RowsNext

		; What changed back into the block: the destination, the offset, the remainder and the rows.
RowsDone:
		.width (A, X), 16
		ldy SR_DEST
		lda [D + B_DEST]
		sta [[D + R_ROWS]:24 + Y]
		ldy SR_DEST + 2
		lda [D + B_DEST + 2]
		sta [[D + R_ROWS]:24 + Y]
		ldy SR_OFFSET
		lda [D + B_OFFSET]
		sta [[D + R_ROWS]:24 + Y]
		ldy SR_OFFSET + 2
		lda [D + B_OFFSET + 2]
		sta [[D + R_ROWS]:24 + Y]
		ldy SR_REMAINDER
		lda [D + B_REMAINDER]
		sta [[D + R_ROWS]:24 + Y]
		ldy SR_REMAINDER + 2
		lda [D + B_REMAINDER + 2]
		sta [[D + R_ROWS]:24 + Y]
		ldy SR_ROWS
		lda [D + B_ROWS]
		sta [[D + R_ROWS]:24 + Y]
		tdc
		clc
		adc R_PUSHED
		tcs
		pld
		rtl
