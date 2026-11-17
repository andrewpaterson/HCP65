; The Video Board driver's text drawing (VideoBoardText.cpp), where the time goes: a cell's pixels and a row's clearing.
; C functions without arguments, m and x 16 bit, D and the data bank kept; what they draw is in the driver's globals.
; A cell's pixels never cross a bank (its address is a multiple of 8, a row's of 16), so the pointers to its words are
; made by adding to the low word alone; the lines down it may, which [direct page],y carries into the bank.

R_PIXEL0	= 1				; The cell's top row's four words (4 byte pointers).
R_PIXEL1	= 5
R_PIXEL2	= 9
R_PIXEL3	= 13
R_WIDTH		= 17			; The pixels in a line of the screen.
R_FRAME		= 18

F_WORD0		= 1				; A row's first sixteen bytes, a word each (4 byte pointers).
F_FRAME		= 32


		.section text.VideoBoardRender
		.relax


; extern "C" void VideoDrawGlyph(void): the 8 by 16 pixels at guiVideoCellPixel from the expanded glyph at
; guiVideoCellGlyph (128 bytes, a byte a pixel, in main memory), a word at a time with nothing looped.  The glyph is
; read through the data bank and X, the pixels written through four pointers and Y, which steps down the lines.
		.global _VideoDrawGlyph
_VideoDrawGlyph:
		.width (A, X), 16
		phb
		phd
		tsc
		sec
		sbc R_FRAME
		tcs
		tcd
		lda [_guiVideoCellPixel]:24
		sta [D + R_PIXEL0]
		clc
		adc 2
		sta [D + R_PIXEL1]
		adc 2
		sta [D + R_PIXEL2]
		adc 2
		sta [D + R_PIXEL3]
		lda [_guiVideoCellPixel + 2]:24
		sta [D + R_PIXEL0 + 2]
		sta [D + R_PIXEL1 + 2]
		sta [D + R_PIXEL2 + 2]
		sta [D + R_PIXEL3 + 2]
		lda [_guiVideoWidth]:24
		sta [D + R_WIDTH]
		lda [_guiVideoCellGlyph]:24
		tax
		sep m
		lda [_guiVideoCellGlyph + 2]:24
		pha
		plb
		rep m
		ldy 0
		; Row 0.
		lda [0 + X]
		sta [[D + R_PIXEL0]:24 + Y]
		lda [2 + X]
		sta [[D + R_PIXEL1]:24 + Y]
		lda [4 + X]
		sta [[D + R_PIXEL2]:24 + Y]
		lda [6 + X]
		sta [[D + R_PIXEL3]:24 + Y]
		tya
		clc
		adc [D + R_WIDTH]
		tay
		; Row 1.
		lda [8 + X]
		sta [[D + R_PIXEL0]:24 + Y]
		lda [10 + X]
		sta [[D + R_PIXEL1]:24 + Y]
		lda [12 + X]
		sta [[D + R_PIXEL2]:24 + Y]
		lda [14 + X]
		sta [[D + R_PIXEL3]:24 + Y]
		tya
		clc
		adc [D + R_WIDTH]
		tay
		; Row 2.
		lda [16 + X]
		sta [[D + R_PIXEL0]:24 + Y]
		lda [18 + X]
		sta [[D + R_PIXEL1]:24 + Y]
		lda [20 + X]
		sta [[D + R_PIXEL2]:24 + Y]
		lda [22 + X]
		sta [[D + R_PIXEL3]:24 + Y]
		tya
		clc
		adc [D + R_WIDTH]
		tay
		; Row 3.
		lda [24 + X]
		sta [[D + R_PIXEL0]:24 + Y]
		lda [26 + X]
		sta [[D + R_PIXEL1]:24 + Y]
		lda [28 + X]
		sta [[D + R_PIXEL2]:24 + Y]
		lda [30 + X]
		sta [[D + R_PIXEL3]:24 + Y]
		tya
		clc
		adc [D + R_WIDTH]
		tay
		; Row 4.
		lda [32 + X]
		sta [[D + R_PIXEL0]:24 + Y]
		lda [34 + X]
		sta [[D + R_PIXEL1]:24 + Y]
		lda [36 + X]
		sta [[D + R_PIXEL2]:24 + Y]
		lda [38 + X]
		sta [[D + R_PIXEL3]:24 + Y]
		tya
		clc
		adc [D + R_WIDTH]
		tay
		; Row 5.
		lda [40 + X]
		sta [[D + R_PIXEL0]:24 + Y]
		lda [42 + X]
		sta [[D + R_PIXEL1]:24 + Y]
		lda [44 + X]
		sta [[D + R_PIXEL2]:24 + Y]
		lda [46 + X]
		sta [[D + R_PIXEL3]:24 + Y]
		tya
		clc
		adc [D + R_WIDTH]
		tay
		; Row 6.
		lda [48 + X]
		sta [[D + R_PIXEL0]:24 + Y]
		lda [50 + X]
		sta [[D + R_PIXEL1]:24 + Y]
		lda [52 + X]
		sta [[D + R_PIXEL2]:24 + Y]
		lda [54 + X]
		sta [[D + R_PIXEL3]:24 + Y]
		tya
		clc
		adc [D + R_WIDTH]
		tay
		; Row 7.
		lda [56 + X]
		sta [[D + R_PIXEL0]:24 + Y]
		lda [58 + X]
		sta [[D + R_PIXEL1]:24 + Y]
		lda [60 + X]
		sta [[D + R_PIXEL2]:24 + Y]
		lda [62 + X]
		sta [[D + R_PIXEL3]:24 + Y]
		tya
		clc
		adc [D + R_WIDTH]
		tay
		; Row 8.
		lda [64 + X]
		sta [[D + R_PIXEL0]:24 + Y]
		lda [66 + X]
		sta [[D + R_PIXEL1]:24 + Y]
		lda [68 + X]
		sta [[D + R_PIXEL2]:24 + Y]
		lda [70 + X]
		sta [[D + R_PIXEL3]:24 + Y]
		tya
		clc
		adc [D + R_WIDTH]
		tay
		; Row 9.
		lda [72 + X]
		sta [[D + R_PIXEL0]:24 + Y]
		lda [74 + X]
		sta [[D + R_PIXEL1]:24 + Y]
		lda [76 + X]
		sta [[D + R_PIXEL2]:24 + Y]
		lda [78 + X]
		sta [[D + R_PIXEL3]:24 + Y]
		tya
		clc
		adc [D + R_WIDTH]
		tay
		; Row 10.
		lda [80 + X]
		sta [[D + R_PIXEL0]:24 + Y]
		lda [82 + X]
		sta [[D + R_PIXEL1]:24 + Y]
		lda [84 + X]
		sta [[D + R_PIXEL2]:24 + Y]
		lda [86 + X]
		sta [[D + R_PIXEL3]:24 + Y]
		tya
		clc
		adc [D + R_WIDTH]
		tay
		; Row 11.
		lda [88 + X]
		sta [[D + R_PIXEL0]:24 + Y]
		lda [90 + X]
		sta [[D + R_PIXEL1]:24 + Y]
		lda [92 + X]
		sta [[D + R_PIXEL2]:24 + Y]
		lda [94 + X]
		sta [[D + R_PIXEL3]:24 + Y]
		tya
		clc
		adc [D + R_WIDTH]
		tay
		; Row 12.
		lda [96 + X]
		sta [[D + R_PIXEL0]:24 + Y]
		lda [98 + X]
		sta [[D + R_PIXEL1]:24 + Y]
		lda [100 + X]
		sta [[D + R_PIXEL2]:24 + Y]
		lda [102 + X]
		sta [[D + R_PIXEL3]:24 + Y]
		tya
		clc
		adc [D + R_WIDTH]
		tay
		; Row 13.
		lda [104 + X]
		sta [[D + R_PIXEL0]:24 + Y]
		lda [106 + X]
		sta [[D + R_PIXEL1]:24 + Y]
		lda [108 + X]
		sta [[D + R_PIXEL2]:24 + Y]
		lda [110 + X]
		sta [[D + R_PIXEL3]:24 + Y]
		tya
		clc
		adc [D + R_WIDTH]
		tay
		; Row 14.
		lda [112 + X]
		sta [[D + R_PIXEL0]:24 + Y]
		lda [114 + X]
		sta [[D + R_PIXEL1]:24 + Y]
		lda [116 + X]
		sta [[D + R_PIXEL2]:24 + Y]
		lda [118 + X]
		sta [[D + R_PIXEL3]:24 + Y]
		tya
		clc
		adc [D + R_WIDTH]
		tay
		; Row 15.
		lda [120 + X]
		sta [[D + R_PIXEL0]:24 + Y]
		lda [122 + X]
		sta [[D + R_PIXEL1]:24 + Y]
		lda [124 + X]
		sta [[D + R_PIXEL2]:24 + Y]
		lda [126 + X]
		sta [[D + R_PIXEL3]:24 + Y]
		tdc
		clc
		adc R_FRAME
		tcs
		pld
		plb
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
