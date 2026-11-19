; The Video Board driver's text drawing (VideoBoardText.cpp), where the time goes: the cells a write changes, the cells a
; frame is owed, a glyph's runs, a cell's clearing and a row's.  C functions without arguments, m and x 16 bit, D and the
; data bank kept; what they work on is the driver's globals (VideoBoard.h).  With a Blitter Board in the computer (the
; kernal's gpuiKernalBlitter) a glyph's runs that are long enough are its copies, and so are a row's clearing and the
; screen's; a cell is always cleared by the processor, which is quicker at eight pixels a row.  A cell's pixels never cross a bank (its
; address is a multiple of 8, a row's of 16); the lines down it may.  Interrupts are off in the driver's calls, so the
; frame being drawn doesn't change under them.

; VideoBoard.h's and HCP65VideoBoard.h's.
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
T_SOURCE	= 39			; The characters being written (a 4 byte pointer),
T_NEXT		= 43			; the next of them (or of the cells owed),
T_LEFT		= 45			; and how many are left.
T_CURSOR	= 47			; The cursor's cell.
T_COLOUR	= 49			; The text colour.
T_DREW		= 51			; Not 0 once a cell has been drawn.
T_BLITTER	= 52			; The Blitter Board's registers (a 4 byte pointer), 0 if there is none.
T_ACROSS	= 56			; The pixel across while the board draws a run,
T_MODE		= 58			; what the board's Flags were last written,
T_LEAST		= 59			; and the fewest pixels of an opaque run, then of a mixed one, that the board draws: 255 without one.
T_FRAME		= 61

; The fewest pixels of a glyph's run that a Blitter Board draws: under them the processor is quicker.
TEXT_BOARD_LEAST			= 0x0809	; A mixed run's (8) in the high byte, an opaque one's (9) in the low.
TEXT_BOARD_NEVER			= 0xFFFF

; The Blitter Board's registers (Drivers/BlitterBoard/BlitterBoard.h) and its Flags for the two kinds of run.
BLITTER_FLAGS				= 0x00
BLITTER_TRANSPARENT_COLOUR	= 0x02
BLITTER_DEST				= 0x04
BLITTER_START				= 0x07
BLITTER_SOURCE				= 0x08
BLITTER_COUNT				= 0x0C
BLITTER_COUNT_STOP			= 0x0E
BLITTER_OPAQUE				= 0x40
BLITTER_TRANSPARENT			= 0x41

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
		stz [D + T_COUNT]
		lda TEXT_BOARD_NEVER
		sta [D + T_LEAST]
		lda [_gpuiKernalBlitter]:24
		sta [D + T_BLITTER]
		lda [_gpuiKernalBlitter + 2]:24
		sta [D + T_BLITTER + 2]
		ora [D + T_BLITTER]
		beq TextFont
		; A Blitter Board draws the glyphs' longer runs: its source is in the font's bank, a run is under 256 pixels, and
		; it leaves colour 0 out of a mixed run.
		lda TEXT_BOARD_LEAST
		sta [D + T_LEAST]
		sep m
		ldy BLITTER_SOURCE + 2
		lda [_gpuiVideoFont + 2]:24
		sta [[D + T_BLITTER]:24 + Y]
		ldy BLITTER_COUNT_STOP + 1
		lda 0
		sta [[D + T_BLITTER]:24 + Y]
		ldy BLITTER_TRANSPARENT_COLOUR
		sta [[D + T_BLITTER]:24 + Y]
		ldy BLITTER_FLAGS
		lda BLITTER_TRANSPARENT
		sta [[D + T_BLITTER]:24 + Y]
		sta [D + T_MODE]
		rep m
TextFont:
		.width (A, X), 16
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
; its colour's palette (the cursor's if it is the cursor's cell).  The glyph's runs are only its pixels, so the cell is
; cleared first.
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
TextRenderClear:
		; Its pixels made its background, then its glyph's drawn.
		.width A, 8
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
		cmp [D + T_LEAST]
		bcs RunBoardOpaque
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
		cmp [D + T_LEAST + 1]
		bcs RunBoardMixed
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

; A run of A pixels (8 or more: a Blitter Board is there and the run is long enough) drawn by the board: a copy from the
; font to the row, a mixed run with the board leaving colour 0 out.  Its Flags are written when the kind of run changes,
; and the count's stop, the source and the destination for each run.  The board has main memory from Start until its
; count is the run's pixels: only the board is read meanwhile.  Then X, Y and A are as the processor's runs leave them.
RunBoardOpaque:
		.width A, 8
		sta [D + T_COUNT]
		lda BLITTER_OPAQUE
		bra RunBoard
RunBoardMixed:
		.width A, 8
		sta [D + T_COUNT]
		lda BLITTER_TRANSPARENT
RunBoard:
		.width A, 8
		sty [D + T_ACROSS]
		cmp [D + T_MODE]
		beq RunBoardKind
		sta [D + T_MODE]
		ldy BLITTER_FLAGS
		sta [[D + T_BLITTER]:24 + Y]
RunBoardKind:
		.width A, 8
		ldy BLITTER_COUNT_STOP
		lda [D + T_COUNT]
		sta [[D + T_BLITTER]:24 + Y]
		rep m
		txa
		sep m
		ldy BLITTER_SOURCE
		sta [[D + T_BLITTER]:24 + Y]
		xba
		iny
		sta [[D + T_BLITTER]:24 + Y]
		rep m
		lda [D + T_DEST]
		clc
		adc [D + T_ACROSS]
		sep m
		ldy BLITTER_DEST
		sta [[D + T_BLITTER]:24 + Y]
		xba
		iny
		sta [[D + T_BLITTER]:24 + Y]
		iny
		lda [D + T_DEST + 2]
		sta [[D + T_BLITTER]:24 + Y]
		ldy BLITTER_START
		sta [[D + T_BLITTER]:24 + Y]
		; While it copies, where the next code is and the pixel across after the run: neither is in main memory.
		rep m
		txa
		clc
		adc [D + T_COUNT]
		tax
		lda [D + T_ACROSS]
		clc
		adc [D + T_COUNT]
		sta [D + T_ACROSS]
		sep m
		ldy BLITTER_COUNT
RunBoardWait:
		.width A, 8
		lda [[D + T_BLITTER]:24 + Y]
		cmp [D + T_COUNT]
		bne RunBoardWait
		ldy [D + T_ACROSS]
		rep m
		lda 0
		sep m
		bra RunNext


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


; extern "C" void VideoClearLines(void): guiVideoFillLines lines of guiVideoWidth pixels from guiVideoFillAddress made
; 0 by the Blitter Board (gpuiKernalBlitter, which the caller has seen isn't NULL): each line a copy of the driver's
; clear row (gpuiVideoClear, a line of zeros in main memory).  The lines follow one another, so the board's destination
; is written once and counts on through them; its source is written for each.
		.global _VideoClearLines
_VideoClearLines:
		.width (A, X), 16
		phd
		lda [_gpuiKernalBlitter + 2]:24
		pha
		lda [_gpuiKernalBlitter]:24
		pha
		tsc
		tcd
		lda [_guiVideoFillLines]:24
		beq ClearDone
		tax
		sep m
		ldy BLITTER_FLAGS
		lda BLITTER_OPAQUE
		sta [[D + 1]:24 + Y]
		ldy BLITTER_COUNT_STOP
		lda [_guiVideoWidth]:24
		sta [[D + 1]:24 + Y]
		iny
		lda [_guiVideoWidth + 1]:24
		sta [[D + 1]:24 + Y]
		ldy BLITTER_DEST
		lda [_guiVideoFillAddress]:24
		sta [[D + 1]:24 + Y]
		iny
		lda [_guiVideoFillAddress + 1]:24
		sta [[D + 1]:24 + Y]
		iny
		lda [_guiVideoFillAddress + 2]:24
		sta [[D + 1]:24 + Y]
ClearLine:
		.width A, 8
		ldy BLITTER_SOURCE
		lda [_gpuiVideoClear]:24
		sta [[D + 1]:24 + Y]
		iny
		lda [_gpuiVideoClear + 1]:24
		sta [[D + 1]:24 + Y]
		iny
		lda [_gpuiVideoClear + 2]:24
		sta [[D + 1]:24 + Y]
		ldy BLITTER_START
		sta [[D + 1]:24 + Y]
ClearWait:
		.width A, 8
		ldy BLITTER_COUNT
		lda [[D + 1]:24 + Y]
		cmp [_guiVideoWidth]:24
		bne ClearWait
		iny
		lda [[D + 1]:24 + Y]
		cmp [_guiVideoWidth + 1]:24
		bne ClearWait
		dex
		bne ClearLine
		rep m
ClearDone:
		.width (A, X), 16
		pla
		pla
		pld
		rtl
