; The Video Board driver's text drawing (VideoBoardText.cpp), where the time goes: the cells a write changes, the cells a
; frame is owed, a glyph's runs, a cell's clearing and a row's.  C functions without arguments, m and x 16 bit, D and the
; data bank kept; what they work on is the driver's globals (VideoBoard.h).  A cell's pixels never cross a bank (its
; address is a multiple of 8, a row's of 16); the lines down it may.  Interrupts are off in the driver's calls, so the
; frame being drawn doesn't change under them.

; VideoBoard.h's and HCP65VideoBoard.h's.
FLAG_MEMORY_FRAME	= 0x04
TEXT_FRAME_BYTES	= 768
TEXT_MAX_ROWS		= 48
TEXT_MAX_PENDING	= 1024
TEXT_CURSOR_PALETTE	= 127
CURSOR_NONE			= 0xFFFF

; The text routines' frame.
T_DEST		= 1				; The cell's first pixel, then the row of it being drawn (a 4 byte pointer).
T_CLEAR0	= 5				; The cell's top row's four words (4 byte pointers).
T_CLEAR1	= 9
T_CLEAR2	= 13
T_CLEAR3	= 17
T_WIDTH		= 21			; The pixels in a line of the screen.
T_COUNT		= 23			; A run's pixels, or rows, still to do.
T_PALETTE	= 25			; The board's Palette Byte register (a 4 byte pointer).
T_CELL		= 29			; The cell: its physical row (D8-D15) and column (D0-D7),
T_INDEX		= 31			; and where its character and colour are.
T_ROW		= 33
T_COLUMN	= 35
T_GLYPH		= 37			; Where the glyph's runs start in the font, 0 if it has no pixel.
T_DIRTY		= 39			; The cell's byte of gauiVideoDirty, and its bit in it,
T_BIT		= 41
T_GROUND	= 43			; and its row's byte of gauiVideoRowGround, in the frame being drawn.
T_DIRTY0	= 45			; That frame's first byte of gauiVideoDirty and of gauiVideoRowGround.
T_GROUND0	= 47
T_SOURCE	= 49			; The characters being written (a 4 byte pointer),
T_NEXT		= 53			; the next of them (or of the cells owed),
T_LEFT		= 55			; and how many are left.
T_CURSOR	= 57			; The cursor's cell.
T_COLOUR	= 59			; The text colour.
T_DREW		= 61			; Not 0 once a cell has been drawn.
T_FRAME		= 62

F_WORD0		= 1				; A row's first sixteen bytes, a word each (4 byte pointers).
F_FRAME		= 32


		.section text.VideoBoardRender
		.relax


; extern "C" void VideoRenderCell(void): the cell guiVideoCell drawn in the frame being drawn.
		.global _VideoRenderCell
_VideoRenderCell:
		.width (A, X), 16
		ldx 0
		bra TextEnter


; extern "C" void VideoRenderPending(void): the guiVideoPending cells of gauiVideoPending drawn.
		.global _VideoRenderPending
_VideoRenderPending:
		.width (A, X), 16
		ldx 1
		bra TextEnter


; extern "C" void VideoRenderAll(void): every cell drawn.
		.global _VideoRenderAll
_VideoRenderAll:
		.width (A, X), 16
		ldx 2
		bra TextEnter


; extern "C" void VideoWriteCells(void): the guiVideoCount (not 0) characters at guiVideoSource into the cells from
; guiVideoCell, along the rows and from the last row to the first, in the text colour.  A cell that changes (or is the
; cursor's, which it then no longer is) is drawn and owed to the other frame.  guiVideoCell is left the cell after the
; last, and guiVideoDrew not 0 if a cell was drawn.
		.global _VideoWriteCells
_VideoWriteCells:
		.width (A, X), 16
		ldx 3

; The frame, with what every cell needs, and the data bank the font's (a glyph's runs are read through it and X).
TextEnter:
		.width (A, X), 16
		phb
		phd
		tsc
		sec
		sbc T_FRAME
		tcs
		tcd
		lda [_guiVideoWidth]:24
		sta [D + T_WIDTH]
		lda [_guiVideoPalette]:24
		sta [D + T_PALETTE]
		lda [_guiVideoPalette + 2]:24
		sta [D + T_PALETTE + 2]
		lda [_guiVideoCursor]:24
		sta [D + T_CURSOR]
		stz [D + T_DIRTY0]
		stz [D + T_GROUND0]
		lda [_guiVideoFlags]:24
		and FLAG_MEMORY_FRAME
		beq TextFrame
		lda TEXT_FRAME_BYTES
		sta [D + T_DIRTY0]
		lda TEXT_MAX_ROWS
		sta [D + T_GROUND0]
TextFrame:
		sep m
		lda [_gpuiVideoFont + 2]:24
		pha
		plb
		rep m
		txa
		beq TextCell
		dec A
		beq TextPending
		dec A
		beq TextAll
		bra TextWrite

TextCell:
		.width (A, X), 16
		lda [_guiVideoCell]:24
		sta [D + T_CELL]
		jsr TextCellIndex
		jsr TextRenderCell
		bra TextLeave

TextPending:
		.width (A, X), 16
		lda [_guiVideoPending]:24
		beq TextLeave
		sta [D + T_LEFT]
		stz [D + T_NEXT]
TextPendingNext:
		ldx [D + T_NEXT]
		lda [_gauiVideoPending + X]:24
		sta [D + T_CELL]
		inx
		inx
		stx [D + T_NEXT]
		jsr TextCellIndex
		jsr TextRenderCell
		dec [D + T_LEFT]
		bne TextPendingNext
		bra TextLeave

TextAll:
		.width (A, X), 16
		stz [D + T_CELL]
		stz [D + T_INDEX]
TextAllNext:
		jsr TextRenderCell
		jsr TextCellAfter
		lda [D + T_CELL]
		bne TextAllNext

TextLeave:
		.width (A, X), 16
		tdc
		clc
		adc T_FRAME
		tcs
		pld
		plb
		rtl

TextWrite:
		.width (A, X), 16
		lda [_guiVideoSource]:24
		sta [D + T_SOURCE]
		lda [_guiVideoSource + 2]:24
		sta [D + T_SOURCE + 2]
		lda [_guiVideoCount]:24
		sta [D + T_LEFT]
		lda [_guiVideoTextColour]:24
		and 0x00FF
		sta [D + T_COLOUR]
		stz [D + T_NEXT]
		stz [D + T_DREW]
		lda [_guiVideoCell]:24
		sta [D + T_CELL]
		jsr TextCellIndex
TextWriteNext:
		.width (A, X), 16
		ldy [D + T_NEXT]
		ldx [D + T_INDEX]
		lda [D + T_CELL]
		cmp [D + T_CURSOR]
		beq TextWriteCursor
		; A cell that is already this (a space where the line is blank) isn't drawn again.
		sep m
		lda [[D + T_SOURCE]:24 + Y]
		cmp [_gauiVideoChars + X]:24
		bne TextWriteChanged
		lda [_gauiVideoCellColours + X]:24
		cmp [D + T_COLOUR]
		bne TextWriteChanged
		rep m
		bra TextWriteAfter
TextWriteCursor:
		.width (A, X), 16
		lda CURSOR_NONE
		sta [D + T_CURSOR]
		sta [_guiVideoCursor]:24
		sep m
TextWriteChanged:
		.width A, 8
		lda [[D + T_SOURCE]:24 + Y]
		sta [_gauiVideoChars + X]:24
		lda [D + T_COLOUR]
		sta [_gauiVideoCellColours + X]:24
		rep m
		jsr TextRenderCell
		inc [D + T_DREW]
		; Owed to the other frame: in the list, or every cell once the list is full.
		lda [_gbVideoPendingAll]:24
		and 0x00FF
		bne TextWriteAfter
		lda [_guiVideoPending]:24
		cmp TEXT_MAX_PENDING
		bcs TextWriteAll
		asl A
		tax
		lda [D + T_CELL]
		sta [_gauiVideoPending + X]:24
		lda [_guiVideoPending]:24
		inc A
		sta [_guiVideoPending]:24
		bra TextWriteAfter
TextWriteAll:
		sep m
		lda 1
		sta [_gbVideoPendingAll]:24
		rep m
TextWriteAfter:
		.width (A, X), 16
		inc [D + T_NEXT]
		jsr TextCellAfter
		dec [D + T_LEFT]
		bne TextWriteNext
		lda [D + T_CELL]
		sta [_guiVideoCell]:24
		lda [D + T_DREW]
		sta [_guiVideoDrew]:24
		bra TextLeave


; T_INDEX for T_CELL: its row's first cell and its column.
TextCellIndex:
		.width (A, X), 16
		lda [D + T_CELL]
		xba
		and 0x00FF
		asl A
		tax
		lda [D + T_CELL]
		and 0x00FF
		clc
		adc [_gauiVideoRowCells + X]:24
		sta [D + T_INDEX]
		rts


; T_CELL and T_INDEX the cell after: along the row, then the start of the next row down (the first after the last).  A
; row's cells follow the row before's.
TextCellAfter:
		.width (A, X), 16
		inc [D + T_INDEX]
		lda [D + T_CELL]
		inc A
		sta [D + T_CELL]
		and 0x00FF
		cmp [_guiVideoColumns]:24
		bne TextCellAfterDone
		lda [D + T_CELL]
		and 0xFF00
		clc
		adc 0x0100
		sta [D + T_CELL]
		xba
		cmp [_guiVideoRows]:24
		bne TextCellAfterDone
		stz [D + T_CELL]
		stz [D + T_INDEX]
TextCellAfterDone:
		rts


; The cell T_CELL, whose character and colour are at T_INDEX, drawn in the frame being drawn: its character's glyph in
; its colour's palette (the cursor's if it is the cursor's cell).  The glyph's runs are only its pixels, so a cell that
; has pixels in this frame, or whose background isn't what its row was cleared to, is cleared first.
TextRenderCell:
		.width (A, X), 16
		lda [D + T_CELL]
		and 0x00FF
		sta [D + T_COLUMN]
		lda [D + T_CELL]
		xba
		and 0x00FF
		sta [D + T_ROW]
		; Its first pixel: its row's, and 8 a column.
		asl A
		asl A
		tax
		lda [D + T_COLUMN]
		asl A
		asl A
		asl A
		clc
		adc [_gauiVideoRowAddress + X]:24
		sta [D + T_DEST]
		lda [_gauiVideoRowAddress + 2 + X]:24
		adc 0
		sta [D + T_DEST + 2]
		; Its row's background and its dirty bit: 16 bytes a row, 8 cells a byte.
		lda [D + T_ROW]
		clc
		adc [D + T_GROUND0]
		sta [D + T_GROUND]
		lda [D + T_ROW]
		asl A
		asl A
		asl A
		asl A
		adc [D + T_DIRTY0]
		sta [D + T_DIRTY]
		lda [D + T_COLUMN]
		lsr A
		lsr A
		lsr A
		clc
		adc [D + T_DIRTY]
		sta [D + T_DIRTY]
		lda [D + T_COLUMN]
		and 7
		tax
		lda [TextCellBits + X]:24
		and 0x00FF
		sta [D + T_BIT]
		; Its glyph.
		ldx [D + T_INDEX]
		lda [_gauiVideoChars + X]:24
		and 0x00FF
		asl A
		tax
		lda [_gauiVideoGlyphs + X]:24
		sta [D + T_GLYPH]

		lda [D + T_CELL]
		cmp [D + T_CURSOR]
		bne TextRenderOwn
		; The cursor's cell is all drawn, and isn't in its row's background after.
		sep m
		lda TEXT_CURSOR_PALETTE
		sta [[D + T_PALETTE]:24]
		bra TextRenderClear
TextRenderOwn:
		.width (A, X), 16
		ldx [D + T_INDEX]
		sep m
		lda [_gauiVideoCellColours + X]:24
		sta [[D + T_PALETTE]:24]
		lsr A
		lsr A
		lsr A
		lsr A
		ldx [D + T_GROUND]
		cmp [_gauiVideoRowGround + X]:24
		bne TextRenderClear
		ldx [D + T_DIRTY]
		lda [_gauiVideoDirty + X]:24
		and [D + T_BIT]
		bne TextRenderDirty
		; Blank in its row's background: only a glyph with pixels need be drawn, and the cell then has them.
		rep m
		lda [D + T_GLYPH]
		beq TextRenderDone
		sep m
		lda [_gauiVideoDirty + X]:24
		ora [D + T_BIT]
		sta [_gauiVideoDirty + X]:24
		bra TextRenderRuns
TextRenderDirty:
		; It has pixels, in its row's background: blank again if its glyph has none.
		.width A, 8
		jsr TextClearCell
		.width A, 16
		lda [D + T_GLYPH]
		bne TextRenderRuns
		ldx [D + T_DIRTY]
		sep m
		lda [D + T_BIT]
		eor 0xFF
		and [_gauiVideoDirty + X]:24
		sta [_gauiVideoDirty + X]:24
		rep m
		rts
TextRenderClear:
		; Not in its row's background.
		.width A, 8
		ldx [D + T_DIRTY]
		lda [_gauiVideoDirty + X]:24
		ora [D + T_BIT]
		sta [_gauiVideoDirty + X]:24
		jsr TextClearCell
		.width A, 16
		lda [D + T_GLYPH]
		beq TextRenderDone

; The glyph's runs (a transparent SFT image's: UserLib/HCP65SFT.h), drawn with the image's top left at T_DEST.  The
; driver has checked the font: every run is inside a cell.  The codes and pixels are read through the data bank and X; a
; row's pixels are written through its pointer and Y, the pixel across.  A is 8 bit throughout, with its high byte 0 so
; that Y can be made from it.
TextRenderRuns:
		rep m
		lda [_gpuiVideoFont]:24
		clc
		adc [D + T_GLYPH]
		tax
		lda 0
		sep m
		ldy 0
RunNext:
		lda [0 + X]
		beq TextRenderDone
		bmi RunMove
		; 1 to 127: that many pixels, most often one.
		inx
		cmp 1
		bne RunMany
		lda [0 + X]
		sta [[D + T_DEST]:24 + Y]
		inx
		iny
		bra RunNext
RunMany:
		sta [D + T_COUNT]
RunPixel:
		lda [0 + X]
		sta [[D + T_DEST]:24 + Y]
		inx
		iny
		dec [D + T_COUNT]
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
		sta [D + T_COUNT]
		tya
		sec
		adc [D + T_COUNT]
		tay
		bra RunNext
RunRows:
		cmp 0xE0
		bcs RunMixed
		; $C0 + (n - 1): n rows down, to the row's left, most often one.
		and 0x1F
		bne RunRowsMany
		clc
		lda [D + T_DEST]
		adc [D + T_WIDTH]
		sta [D + T_DEST]
		lda [D + T_DEST + 1]
		adc [D + T_WIDTH + 1]
		sta [D + T_DEST + 1]
		bcc RunRowOne
		inc [D + T_DEST + 2]
RunRowOne:
		ldy 0
		bra RunNext
RunRowsMany:
		sta [D + T_COUNT]
RunRow:
		clc
		lda [D + T_DEST]
		adc [D + T_WIDTH]
		sta [D + T_DEST]
		lda [D + T_DEST + 1]
		adc [D + T_WIDTH + 1]
		sta [D + T_DEST + 1]
		bcc RunRowSame
		inc [D + T_DEST + 2]
RunRowSame:
		dec [D + T_COUNT]
		bpl RunRow
		ldy 0
		bra RunNext
RunMixed:
		; $E0 + (n - 1): n pixels, of which those that are 0 aren't drawn.
		and 0x1F
		inc A
		sta [D + T_COUNT]
RunMixedPixel:
		lda [0 + X]
		beq RunMixedNone
		sta [[D + T_DEST]:24 + Y]
RunMixedNone:
		inx
		iny
		dec [D + T_COUNT]
		bne RunMixedPixel
		bra RunNext
TextRenderDone:
		rep m
		rts


; The 8 by 16 pixels at T_DEST made 0, a word at a time through four pointers and Y, which steps down the lines.  A
; is left 16 bit.
TextClearCell:
		rep m
		lda [D + T_DEST]
		sta [D + T_CLEAR0]
		clc
		adc 2
		sta [D + T_CLEAR1]
		adc 2
		sta [D + T_CLEAR2]
		adc 2
		sta [D + T_CLEAR3]
		lda [D + T_DEST + 2]
		sta [D + T_CLEAR0 + 2]
		sta [D + T_CLEAR1 + 2]
		sta [D + T_CLEAR2 + 2]
		sta [D + T_CLEAR3 + 2]
		ldy 0
		ldx 16
TextClearLine:
		lda 0
		sta [[D + T_CLEAR0]:24 + Y]
		sta [[D + T_CLEAR1]:24 + Y]
		sta [[D + T_CLEAR2]:24 + Y]
		sta [[D + T_CLEAR3]:24 + Y]
		tya
		clc
		adc [D + T_WIDTH]
		tay
		dex
		bne TextClearLine
		rts


; A cell's bit in its byte of gauiVideoDirty (and a byte for the word read of the last).
TextCellBits:
		.byte 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0


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
