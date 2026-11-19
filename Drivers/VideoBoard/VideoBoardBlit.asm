; The Video Board driver's blit of a transparent SFT image that is all in one bank of the process's and all inside the
; clip rectangle (VideoBoardBlit.cpp's BlitTransparentWhole without a Blitter Board): its codes read and its runs copied
; here, as a C++ loop and a copy for each run took most of a sprite's time.  A C function without arguments, m and x 16
; bit, D and the data bank kept; what it works on is the driver's globals (VideoBoard.h).  The same checks as the C++: a
; run outside the image or the image's bytes is guiVideoBlitResult 1, with what came before it drawn.
;
; A run or a row of BLIT_MOVE_LEAST pixels or more is one mvn (7 cycles a byte; a word at a time is 11.5), its banks
; written into it first, as libc's memmove does: the driver's calls are made with interrupts off, so nothing else runs
; it meanwhile.  Not one whose pixels cross a bank of the screen, which mvn can't: that is a word at a time.

BLIT_MOVE_LEAST	= 8

; The frame.
B_ROW		= 1				; The row's first pixel (a 4 byte pointer).
B_DEST		= 5				; A run's first pixel (a 4 byte pointer).
B_SOURCE	= 9				; A run's first byte (a 4 byte pointer, in the image's bank).
B_ACROSS	= 13			; The pixel across the image.
B_ROWS		= 15			; The row of the image.
B_COUNT		= 17			; A run's pixels, or rows.
B_END		= 19			; Just past the image's bytes, in its bank.
B_STRIDE	= 21			; The pixels in a line of the screen.
B_WIDTH		= 23			; The image's.
B_HEIGHT	= 25
B_FRAME		= 27


		.section text.VideoBoardBlit
		.relax


; extern "C" void VideoBlitRuns(void): the image whose first code is at gpuiVideoBlitCode (and whose bytes end at
; guiVideoBlitEnd in the same bank, which isn't its end), guiVideoBlitWidth by guiVideoBlitHeight, drawn with its top
; left at gpuiVideoBlitRow; guiVideoBlitResult 0, or 1 if a run isn't in it.  The codes are read through the data bank
; and X; a run's pixels through its pointers and Y.
		.global _VideoBlitRuns
_VideoBlitRuns:
		.width (A, X), 16
		phb
		phd
		tsc
		sec
		sbc B_FRAME
		tcs
		tcd
		lda [_gpuiVideoBlitRow]:24
		sta [D + B_ROW]
		lda [_gpuiVideoBlitRow + 2]:24
		sta [D + B_ROW + 2]
		lda [_gpuiVideoBlitCode + 2]:24
		and 0x00FF
		sta [D + B_SOURCE + 2]
		lda [_guiVideoBlitEnd]:24
		sta [D + B_END]
		lda [_guiVideoWidth]:24
		sta [D + B_STRIDE]
		lda [_guiVideoBlitWidth]:24
		sta [D + B_WIDTH]
		lda [_guiVideoBlitHeight]:24
		sta [D + B_HEIGHT]
		stz [D + B_ACROSS]
		stz [D + B_ROWS]
		sep m
		lda [_gpuiVideoBlitCode + 2]:24
		sta [BlitMove + 2]:24
		pha
		plb
		rep m
		lda [_gpuiVideoBlitCode]:24
		tax

BlitNext:
		.width (A, X), 16
		txa
		cmp [D + B_END]
		bcs BlitBad
		lda [0 + X]
		and 0x00FF
		inx
		cmp 0
		beq BlitDone
		cmp 0x80
		bcc BlitOpaque
		cmp 0xC0
		bcc BlitSkip
		cmp 0xE0
		bcs BlitMixed

		; $C0 + (n - 1): n rows down, to the row's left.
		and 0x1F
		inc A
		sta [D + B_COUNT]
		clc
		adc [D + B_ROWS]
		sta [D + B_ROWS]
		stz [D + B_ACROSS]
BlitRowDown:
		clc
		lda [D + B_ROW]
		adc [D + B_STRIDE]
		sta [D + B_ROW]
		bcc BlitRowSame
		inc [D + B_ROW + 2]
BlitRowSame:
		dec [D + B_COUNT]
		bne BlitRowDown
		bra BlitNext

		; $80 + (n - 1): n pixels across.
BlitSkip:
		and 0x3F
		sec
		adc [D + B_ACROSS]
		sta [D + B_ACROSS]
		bra BlitNext

		; 1 to 127: that many pixels, a word at a time then the odd one, X kept on the stack while it counts them.
BlitOpaque:
		jsr BlitCheck
		bcs BlitBad
		cmp BLIT_MOVE_LEAST
		bcc BlitWords
		; One mvn, unless its pixels cross a bank: then X is past the run, and so the next code, as the run's next byte.
		lda [D + B_DEST]
		clc
		adc [D + B_COUNT]
		bcc BlitMoveFits
		bne BlitWords
BlitMoveFits:
		sep m
		lda [D + B_DEST + 2]
		sta [BlitMove + 1]:24
		rep m
		ldy [D + B_DEST]
		lda [D + B_COUNT]
		dec A
		phb
BlitMove:
		mvn 0, 0
		plb
		lda [D + B_ACROSS]
		clc
		adc [D + B_COUNT]
		sta [D + B_ACROSS]
		jmp BlitNext
BlitWords:
		phx
		ldy 0
		lda [D + B_COUNT]
		lsr A
		beq BlitOdd
		tax
BlitWord:
		lda [[D + B_SOURCE]:24 + Y]
		sta [[D + B_DEST]:24 + Y]
		iny
		iny
		dex
		bne BlitWord
BlitOdd:
		lda [D + B_COUNT]
		lsr A
		bcc BlitOpaqueDone
		sep m
		lda [[D + B_SOURCE]:24 + Y]
		sta [[D + B_DEST]:24 + Y]
		rep m
BlitOpaqueDone:
		plx
		bra BlitAfter

		; $E0 + (n - 1): n pixels, of which those that are 0 aren't drawn.
BlitMixed:
		and 0x1F
		inc A
		jsr BlitCheck
		bcs BlitBad
		ldy 0
		sep m
BlitMixedPixel:
		lda [[D + B_SOURCE]:24 + Y]
		beq BlitMixedNone
		sta [[D + B_DEST]:24 + Y]
BlitMixedNone:
		iny
		cpy [D + B_COUNT]
		bne BlitMixedPixel
		rep m

		; Past the run, in the codes and across the image.
BlitAfter:
		.width (A, X), 16
		txa
		clc
		adc [D + B_COUNT]
		tax
		lda [D + B_ACROSS]
		clc
		adc [D + B_COUNT]
		sta [D + B_ACROSS]
		bra BlitNext

BlitDone:
		.width (A, X), 16
		lda 0
		bra BlitLeave
BlitBad:
		.width (A, X), 16
		lda 1
BlitLeave:
		.width (A, X), 16
		sta [_guiVideoBlitResult]:24
		tdc
		clc
		adc B_FRAME
		tcs
		pld
		plb
		rtl


; A run of A pixels whose first byte is at X: carry clear if it is in the image's row and its bytes, with B_COUNT,
; B_SOURCE and B_DEST set for it; carry set if it isn't.
BlitCheck:
		.width (A, X), 16
		sta [D + B_COUNT]
		lda [D + B_ROWS]
		cmp [D + B_HEIGHT]
		bcs BlitCheckBad
		lda [D + B_ACROSS]
		clc
		adc [D + B_COUNT]
		bcs BlitCheckBad
		cmp [D + B_WIDTH]
		beq BlitCheckAcross
		bcs BlitCheckBad
BlitCheckAcross:
		txa
		clc
		adc [D + B_COUNT]
		bcs BlitCheckBad
		cmp [D + B_END]
		beq BlitCheckSource
		bcs BlitCheckBad
BlitCheckSource:
		stx [D + B_SOURCE]
		lda [D + B_ROW]
		clc
		adc [D + B_ACROSS]
		sta [D + B_DEST]
		lda [D + B_ROW + 2]
		adc 0
		sta [D + B_DEST + 2]
		lda [D + B_COUNT]
		clc
		rts
BlitCheckBad:
		sec
		rts


; The span copy's frame.
S_SOURCE	= 1				; The row's pixel at the spans' left (a 4 byte pointer, in its bank).
S_DEST		= 5				; Where that pixel goes (a 4 byte pointer).
S_SPANS		= 9				; The row's span (a 4 byte pointer).
S_ROWS		= 13			; Rows still to do.
S_LEAST		= 15			; The least left and the most right a span is cut to.
S_MOST		= 17
S_LEFT		= 19			; The row's span, cut.
S_RIGHT		= 21
S_SOURCE_STRIDE	= 23		; The image's width, and the screen's.
S_DEST_STRIDE	= 25
S_COUNT		= 27			; The row's pixels.
S_FRAME		= 29


; extern "C" void VideoBlitSpans(void): guiVideoSpanRows rows of an opaque image, each from its span's left to its
; right (two words at gpuiVideoSpans, a row's after the row before's) cut to guiVideoSpanLeast and guiVideoSpanMost.
; Pixel 0 of the first row is at gpuiVideoSpanSource, and each row is guiVideoSpanSourceStride on in the same bank
; (the driver has checked that every row is in it); it goes to gpuiVideoSpanDest, and each row a line (guiVideoWidth)
; on.  A span is pixels across from the spans' left: 0 for the pixel there.
		.global _VideoBlitSpans
_VideoBlitSpans:
		.width (A, X), 16
		phb
		phd
		tsc
		sec
		sbc S_FRAME
		tcs
		tcd
		lda [_gpuiVideoSpanSource]:24
		sta [D + S_SOURCE]
		lda [_gpuiVideoSpanSource + 2]:24
		and 0x00FF
		sta [D + S_SOURCE + 2]
		sep m
		sta [SpanMove + 2]:24
		rep m
		lda [_gpuiVideoSpanDest]:24
		sta [D + S_DEST]
		lda [_gpuiVideoSpanDest + 2]:24
		and 0x00FF
		sta [D + S_DEST + 2]
		lda [_gpuiVideoSpans]:24
		sta [D + S_SPANS]
		lda [_gpuiVideoSpans + 2]:24
		and 0x00FF
		sta [D + S_SPANS + 2]
		lda [_guiVideoSpanRows]:24
		sta [D + S_ROWS]
		lda [_guiVideoSpanLeast]:24
		sta [D + S_LEAST]
		lda [_guiVideoSpanMost]:24
		sta [D + S_MOST]
		lda [_guiVideoSpanSourceStride]:24
		sta [D + S_SOURCE_STRIDE]
		lda [_guiVideoWidth]:24
		sta [D + S_DEST_STRIDE]
		lda [D + S_ROWS]
		beq SpanLeave

SpanRow:
		.width (A, X), 16
		; The span, cut to the least and the most.
		ldy 0
		lda [[D + S_SPANS]:24 + Y]
		cmp [D + S_LEAST]
		bcs SpanLeftCut
		lda [D + S_LEAST]
SpanLeftCut:
		sta [D + S_LEFT]
		ldy 2
		lda [[D + S_SPANS]:24 + Y]
		cmp [D + S_MOST]
		bcc SpanRightCut
		lda [D + S_MOST]
SpanRightCut:
		sta [D + S_RIGHT]
		sec
		sbc [D + S_LEFT]
		beq SpanNext
		bcc SpanNext
		cmp BLIT_MOVE_LEAST
		bcc SpanWords
		; One mvn from the left to the right: the destination's first pixel S_DEST + left, unless the row's pixels cross
		; a bank there.
		sta [D + S_COUNT]
		lda [D + S_DEST]
		clc
		adc [D + S_LEFT]
		tay
		lda [D + S_DEST + 2]
		adc 0
		sep m
		sta [SpanMove + 1]:24
		rep m
		tya
		clc
		adc [D + S_COUNT]
		bcc SpanMoveFits
		bne SpanWordsAgain
SpanMoveFits:
		lda [D + S_SOURCE]
		clc
		adc [D + S_LEFT]
		tax
		lda [D + S_COUNT]
		dec A
		phb
SpanMove:
		mvn 0, 0
		plb
		bra SpanNext
SpanWordsAgain:
		lda [D + S_COUNT]
SpanWords:
		; A pixels from the left: a word at a time then the odd one.
		lsr A
		tax
		ldy [D + S_LEFT]
		cpx 0
		beq SpanOdd
SpanWord:
		lda [[D + S_SOURCE]:24 + Y]
		sta [[D + S_DEST]:24 + Y]
		iny
		iny
		dex
		bne SpanWord
SpanOdd:
		cpy [D + S_RIGHT]
		beq SpanNext
		sep m
		lda [[D + S_SOURCE]:24 + Y]
		sta [[D + S_DEST]:24 + Y]
		rep m

		; The next row: the source on in its bank, the destination a line on, the next span.
SpanNext:
		.width (A, X), 16
		lda [D + S_SOURCE]
		clc
		adc [D + S_SOURCE_STRIDE]
		sta [D + S_SOURCE]
		lda [D + S_DEST]
		clc
		adc [D + S_DEST_STRIDE]
		sta [D + S_DEST]
		bcc SpanDestSame
		inc [D + S_DEST + 2]
SpanDestSame:
		lda [D + S_SPANS]
		clc
		adc 4
		sta [D + S_SPANS]
		dec [D + S_ROWS]
		bne SpanRow

SpanLeave:
		.width (A, X), 16
		tdc
		clc
		adc S_FRAME
		tcs
		pld
		plb
		rtl
