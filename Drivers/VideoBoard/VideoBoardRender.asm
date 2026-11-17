; The Video Board driver's text drawing (VideoBoardText.cpp), where the time goes: a glyph's runs, a cell's clearing and
; a row's.  C functions without arguments, m and x 16 bit, D and the data bank kept; what they draw is in the driver's
; globals.  A cell's pixels never cross a bank (its address is a multiple of 8, a row's of 16); the lines down it may.

R_DEST		= 1				; The row of the cell being drawn: its first pixel (a 4 byte pointer).
R_WIDTH		= 5				; The pixels in a line of the screen.
R_COUNT		= 7				; Pixels, or rows, still to do.
R_FRAME		= 8

C_PIXEL0	= 1				; The cell's top row's four words (4 byte pointers).
C_PIXEL1	= 5
C_PIXEL2	= 9
C_PIXEL3	= 13
C_WIDTH		= 17
C_FRAME		= 18

F_WORD0		= 1				; A row's first sixteen bytes, a word each (4 byte pointers).
F_FRAME		= 32


		.section text.VideoBoardRender
		.relax


; extern "C" void VideoDrawRuns(void): a transparent SFT image's runs (UserLib/HCP65SFT.h), from the code at
; guiVideoCellGlyph, drawn with the image's top left at guiVideoCellPixel.  The driver has checked the image: every
; run is inside a cell.  The codes and pixels are read through the data bank and X; a row's pixels are written through
; its pointer and Y, the pixel across.  A is 8 bit throughout, with its high byte 0 so that Y can be made from it.
		.global _VideoDrawRuns
_VideoDrawRuns:
		.width (A, X), 16
		phb
		phd
		tsc
		sec
		sbc R_FRAME
		tcs
		tcd
		lda [_guiVideoCellPixel]:24
		sta [D + R_DEST]
		lda [_guiVideoCellPixel + 2]:24
		sta [D + R_DEST + 2]
		lda [_guiVideoWidth]:24
		sta [D + R_WIDTH]
		lda [_guiVideoCellGlyph]:24
		tax
		lda 0
		sep m
		lda [_guiVideoCellGlyph + 2]:24
		pha
		plb
		ldy 0
RunNext:
		lda [0 + X]
		beq RunDone
		bmi RunMove
		; 1 to 127: that many pixels, most often one.
		inx
		cmp 1
		bne RunMany
		lda [0 + X]
		sta [[D + R_DEST]:24 + Y]
		inx
		iny
		bra RunNext
RunMany:
		sta [D + R_COUNT]
RunPixel:
		lda [0 + X]
		sta [[D + R_DEST]:24 + Y]
		inx
		iny
		dec [D + R_COUNT]
		bne RunPixel
		bra RunNext
RunMove:
		inx
		cmp 0xC0
		bcs RunRows
		; $80 + (n - 1): n pixels across, most often one.
		and 0x3F
		bne RunSkipMany
		iny
		bra RunNext
RunSkipMany:
		sta [D + R_COUNT]
		tya
		sec
		adc [D + R_COUNT]
		tay
		bra RunNext
RunRows:
		; $C0 + (n - 1): n rows down, to the row's left, most often one.
		and 0x3F
		bne RunRowsMany
		clc
		lda [D + R_DEST]
		adc [D + R_WIDTH]
		sta [D + R_DEST]
		lda [D + R_DEST + 1]
		adc [D + R_WIDTH + 1]
		sta [D + R_DEST + 1]
		bcc RunRowOne
		inc [D + R_DEST + 2]
RunRowOne:
		ldy 0
		bra RunNext
RunRowsMany:
		sta [D + R_COUNT]
RunRow:
		clc
		lda [D + R_DEST]
		adc [D + R_WIDTH]
		sta [D + R_DEST]
		lda [D + R_DEST + 1]
		adc [D + R_WIDTH + 1]
		sta [D + R_DEST + 1]
		bcc RunRowSame
		inc [D + R_DEST + 2]
RunRowSame:
		dec [D + R_COUNT]
		bpl RunRow
		ldy 0
		bra RunNext
RunDone:
		rep m
		tdc
		clc
		adc R_FRAME
		tcs
		pld
		plb
		rtl


; extern "C" void VideoClearCell(void): the 8 by 16 pixels at guiVideoCellPixel made 0, a word at a time through four
; pointers and Y, which steps down the lines.
		.global _VideoClearCell
_VideoClearCell:
		.width (A, X), 16
		phd
		tsc
		sec
		sbc C_FRAME
		tcs
		tcd
		lda [_guiVideoCellPixel]:24
		sta [D + C_PIXEL0]
		clc
		adc 2
		sta [D + C_PIXEL1]
		adc 2
		sta [D + C_PIXEL2]
		adc 2
		sta [D + C_PIXEL3]
		lda [_guiVideoCellPixel + 2]:24
		sta [D + C_PIXEL0 + 2]
		sta [D + C_PIXEL1 + 2]
		sta [D + C_PIXEL2 + 2]
		sta [D + C_PIXEL3 + 2]
		lda [_guiVideoWidth]:24
		sta [D + C_WIDTH]
		ldy 0
		ldx 16
ClearRow:
		lda 0
		sta [[D + C_PIXEL0]:24 + Y]
		sta [[D + C_PIXEL1]:24 + Y]
		sta [[D + C_PIXEL2]:24 + Y]
		sta [[D + C_PIXEL3]:24 + Y]
		tya
		clc
		adc [D + C_WIDTH]
		tay
		dex
		bne ClearRow
		tdc
		clc
		adc C_FRAME
		tcs
		pld
		rtl


; extern "C" void VideoFillZero(void): guiVideoFillGroups groups of sixteen zero bytes from guiVideoFillAddress (a
; multiple of 16; no more than 64KB of them): a row of cells blank.
		.global _VideoFillZero
_VideoFillZero:
		.width (A, X), 16
		phd
		tsc
		sec
		sbc F_FRAME
		tcs
		tcd
		; The eight pointers: the address and the seven words after it.
		ldx 0
		lda [_guiVideoFillAddress]:24
FillPointer:
		sta [D + F_WORD0 + X]
		pha
		lda [_guiVideoFillAddress + 2]:24
		sta [D + F_WORD0 + 2 + X]
		pla
		clc
		adc 2
		inx
		inx
		inx
		inx
		cpx 32
		bne FillPointer
		lda [_guiVideoFillGroups]:24
		tax
		beq FillDone
		ldy 0
FillGroup:
		lda 0
		sta [[D + F_WORD0]:24 + Y]
		sta [[D + F_WORD0 + 4]:24 + Y]
		sta [[D + F_WORD0 + 8]:24 + Y]
		sta [[D + F_WORD0 + 12]:24 + Y]
		sta [[D + F_WORD0 + 16]:24 + Y]
		sta [[D + F_WORD0 + 20]:24 + Y]
		sta [[D + F_WORD0 + 24]:24 + Y]
		sta [[D + F_WORD0 + 28]:24 + Y]
		tya
		clc
		adc 16
		tay
		dex
		bne FillGroup
FillDone:
		tdc
		clc
		adc F_FRAME
		tcs
		pld
		rtl
