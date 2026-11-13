; memcpy, memmove and memset for the W65C816, with its block moves: mvn copies A + 1 bytes
; from [source bank : X] up to [destination bank : Y], 7 cycles a byte; mvp copies down from
; the ends.  The banks are the instruction's operand bytes, so each move writes them first
; (the code runs in RAM).  X and Y wrap in their bank, so a move stops at the end of the
; source's or the destination's bank and the next goes on from there.  memset stores the
; first byte and moves the rest up from one byte below, so each byte copies the one before
; it.  string.cpp has the other string functions (and these, for the 65C64's reference).
; stringW65C816_16.asm has memcpy16 and the like, with 16 bit counts, for BaseLib's natural arrays.
; The bit array finders (with lsize counts) and the tables they share with stringW65C816_16.asm's are at the end.
; The code changes itself, so it isn't reentrant: an interrupt handler mustn't call these.
; The calling convention is the compiler's register arguments (IR816Backend.h, W65C816.txt):
; the C functions here take the first argument (a pointer) in A (low) and X (high) and an int
; after it in Y, the rest pushed last first (pointers and size_t 4 bytes, int 2); A and X are 16
; bits wide on the way in and out, the result is in A (low) and X (high), D and the data bank
; are kept and A, X and Y change.  The register entries are name.ax and name.axy (the
; suffix says which registers); the plain names are stack entries (function pointers,
; assembly) that load the registers and call them.  Each function (but memset within one bank,
; which works from S) pushes Y, X and A, the data bank, 14 bytes of locals and D, and points D
; at the stack:
;	D + 1		the caller's D
;	D + 3		Count: the bytes of this move less 1
;	D + 5		Dst: the destination (memmove down: its last byte)
;	D + 9		Src: the source (memmove down: its last byte)
;	D + 13		Bytes: the bytes left
;	D + 17		the caller's data bank
;	D + 18		Argument: the destination (A and X), then memset's value (Y)
;	D + 24		the return address
;	D + 27		the pushed arguments: memcpy's and memmove's source, then the size; memset's size

Count		= 3
Dst			= 5
Src			= 9
Bytes		= 13
Argument	= 18
SetValue	= 22
Pushed		= 27


		.section text.MemMove816
		.global _memcpy, _memmove, _memset, _memcpy.ax, _memmove.ax, _memset.axy

_memcpy:
		.width (A, X), 16
		lda [S + 14]
		pha
		lda [S + 14]
		pha
		lda [S + 14]
		pha
		lda [S + 14]
		pha
		lda [S + 14]
		tax
		lda [S + 12]
		jsl _memcpy.ax
		tay
		tsc
		clc
		adc 8
		tcs
		tya
		rtl

_memmove:
		.width (A, X), 16
		lda [S + 14]
		pha
		lda [S + 14]
		pha
		lda [S + 14]
		pha
		lda [S + 14]
		pha
		lda [S + 14]
		tax
		lda [S + 12]
		jsl _memmove.ax
		tay
		tsc
		clc
		adc 8
		tcs
		tya
		rtl

_memset:
		.width (A, X), 16
		lda [S + 12]
		pha
		lda [S + 12]
		pha
		lda [S + 12]
		tay
		lda [S + 10]
		tax
		lda [S + 8]
		jsl _memset.axy
		ply
		ply
		rtl

; void* memcpy(void* pvDest, const void* pvSource, size_t uiBytes)
; Up to SmallBytes bytes are copied here, last first, by a byte (if odd) then words through the
; pointers ([[D + n]:24 + Y] crosses banks), with D at the stack: D + 1 the caller's D, D + 3
; the destination (A and X), D + 7 the return address, D + 10 the source, D + 14 the size.
; Most of BaseLib's copies are a few bytes (a pixel's channels), which the block move's frame
; and set up would take about 600 cycles for; this takes about 105 for 4.  More go to MoveUp.
SmallBytes	= 32
SmallDst	= 3
SmallSrc	= 10
SmallSize	= 14

_memcpy.ax:
		.width (A, X), 16
		phx
		pha
		phd
		tsc
		tcd
		lda [D + SmallSize + 2]
		bne MemcpyLarge
		ldy [D + SmallSize]
		cpy SmallBytes + 1
		bcs MemcpyLarge
		tya
		beq MemcpySmallDone
		lsr A
		bcc MemcpyWords
		dey
		sep m
		lda [[D + SmallSrc]:24 + Y]
		sta [[D + SmallDst]:24 + Y]
		rep m
		tyx
		beq MemcpySmallDone
MemcpyWords:
		.width (A, X), 16
		dey
		dey
		lda [[D + SmallSrc]:24 + Y]
		sta [[D + SmallDst]:24 + Y]
		tyx
		bne MemcpyWords
MemcpySmallDone:
		.width (A, X), 16
		pld
		pla
		plx
		rtl
MemcpyLarge:
		.width (A, X), 16
		pld
		pla
		plx
		phy
		phx
		pha
		phb
		pea 0
		pea 0
		pea 0
		pea 0
		pea 0
		pea 0
		pea 0
		phd
		tsc
		tcd
		jsr Arguments
		brl MoveUp

; void* memmove(void* pvDest, const void* pvSource, size_t uiBytes): up unless the destination
; is above the source and in it.
_memmove.ax:
		.width (A, X), 16
		phy
		phx
		pha
		phb
		pea 0
		pea 0
		pea 0
		pea 0
		pea 0
		pea 0
		pea 0
		phd
		tsc
		tcd
		jsr Arguments
		lda [D + Dst + 2]
		cmp [D + Src + 2]
		bne Ordered
		lda [D + Dst]
		cmp [D + Src]
Ordered:
		.width (A, X), 16
		bcc ToUp
		beq ToUp
		clc
		lda [D + Src]
		adc [D + Bytes]
		tax
		lda [D + Src + 2]
		adc [D + Bytes + 2]
		cmp [D + Dst + 2]
		bne Ended
		txa
		cmp [D + Dst]
Ended:
		.width (A, X), 16
		bcc ToUp
		beq ToUp
		brl MoveDown
ToUp:
		.width (A, X), 16
		brl MoveUp

; void* memset(void* pvDest, int iValue, size_t uiBytes): within one bank, without a frame
; (the arguments are read from S), the data bank set to the destination's and the value's
; byte twice in a word stored 16 bytes at a time (sta [n + Y], about 4 cycles a byte), then
; the words and the byte left; otherwise (or for 0 bytes) the first byte, then each from the
; one before.
_memset.axy:
		.width (A, X), 16
		phy
		phx
		pha
		lda [S + 12]
		bne MemsetMove
		lda [S + 10]
		beq MemsetMove
		clc
		adc [S + 1]
		bcs MemsetMove
		phb
		sep m
		lda [S + 4]
		pha
		plb
		lda [S + 6]
		xba
		lda [S + 6]
		rep m
		pha
		lda [S + 4]
		tay
		lda [S + 13]
		lsr A
		lsr A
		lsr A
		lsr A
		beq MemsetWords
		tax
		lda [S + 1]
MemsetSixteen:
		.width (A, X), 16
		sta [0 + Y]
		sta [2 + Y]
		sta [4 + Y]
		sta [6 + Y]
		sta [8 + Y]
		sta [10 + Y]
		sta [12 + Y]
		sta [14 + Y]
		tya
		clc
		adc 16
		tay
		lda [S + 1]
		dex
		bne MemsetSixteen
MemsetWords:
		.width (A, X), 16
		lda [S + 13]
		and 14
		lsr A
		beq MemsetByte
		tax
		lda [S + 1]
MemsetWord:
		.width (A, X), 16
		sta [0 + Y]
		iny
		iny
		dex
		bne MemsetWord
MemsetByte:
		.width (A, X), 16
		lda [S + 13]
		and 1
		beq MemsetDone
		sep m
		lda [S + 1]
		sta [0 + Y]
		rep m
MemsetDone:
		.width (A, X), 16
		ply
		plb
		pla
		plx
		ply
		rtl
MemsetMove:
		.width (A, X), 16
		phb
		pea 0
		pea 0
		pea 0
		pea 0
		pea 0
		pea 0
		pea 0
		phd
		tsc
		tcd
		lda [D + Pushed]
		ora [D + Pushed + 2]
		bne Set
		brl Exit
Set:
		.width (A, X), 16
		sep m
		lda [D + SetValue]
		sta [[D + Argument]:24]
		rep m
		lda [D + Argument]
		sta [D + Src]
		clc
		adc 1
		sta [D + Dst]
		lda [D + Argument + 2]
		sta [D + Src + 2]
		adc 0
		sta [D + Dst + 2]
		sec
		lda [D + Pushed]
		sbc 1
		sta [D + Bytes]
		lda [D + Pushed + 2]
		sbc 0
		sta [D + Bytes + 2]

; Moves up while there are bytes left: as many as the rest of the source's and the
; destination's banks hold, and at most 65536.
MoveUp:
		.width (A, X), 16
		lda [D + Bytes]
		ora [D + Bytes + 2]
		beq Exit
		jsr Limit
		lda [D + Src]
		eor 65535
		cmp [D + Count]
		bcs UpSource
		sta [D + Count]
UpSource:
		.width (A, X), 16
		lda [D + Dst]
		eor 65535
		cmp [D + Count]
		bcs UpDestination
		sta [D + Count]
UpDestination:
		.width (A, X), 16
		sep m
		lda [D + Src + 2]
		sta [Up + 2]:24
		lda [D + Dst + 2]
		sta [Up + 1]:24
		rep m
		ldx [D + Src]
		ldy [D + Dst]
		lda [D + Count]
Up:
		mvn 0, 0
		sec
		lda [D + Src]
		adc [D + Count]
		sta [D + Src]
		lda [D + Src + 2]
		adc 0
		sta [D + Src + 2]
		sec
		lda [D + Dst]
		adc [D + Count]
		sta [D + Dst]
		lda [D + Dst + 2]
		adc 0
		sta [D + Dst + 2]
		jsr Moved
		bra MoveUp

; The result is the destination argument.
Exit:
		.width (A, X), 16
		lda [D + Argument]
		ldx [D + Argument + 2]
		pld
		ply
		ply
		ply
		ply
		ply
		ply
		ply
		plb
		ply
		ply
		ply
		rtl

; Moves down from the ends while there are bytes left, as many as the source's and the
; destination's banks hold below them.
MoveDown:
		.width (A, X), 16
		clc
		lda [D + Src]
		adc [D + Bytes]
		sta [D + Src]
		lda [D + Src + 2]
		adc [D + Bytes + 2]
		sta [D + Src + 2]
		clc
		lda [D + Dst]
		adc [D + Bytes]
		sta [D + Dst]
		lda [D + Dst + 2]
		adc [D + Bytes + 2]
		sta [D + Dst + 2]
		lda [D + Src]
		bne SourceEnd
		dec [D + Src + 2]
SourceEnd:
		.width (A, X), 16
		dec [D + Src]
		lda [D + Dst]
		bne DestinationEnd
		dec [D + Dst + 2]
DestinationEnd:
		.width (A, X), 16
		dec [D + Dst]
DownLoop:
		.width (A, X), 16
		lda [D + Bytes]
		ora [D + Bytes + 2]
		beq Exit
		jsr Limit
		lda [D + Src]
		cmp [D + Count]
		bcs DownSource
		sta [D + Count]
DownSource:
		.width (A, X), 16
		lda [D + Dst]
		cmp [D + Count]
		bcs DownDestination
		sta [D + Count]
DownDestination:
		.width (A, X), 16
		sep m
		lda [D + Src + 2]
		sta [Down + 2]:24
		lda [D + Dst + 2]
		sta [Down + 1]:24
		rep m
		ldx [D + Src]
		ldy [D + Dst]
		lda [D + Count]
Down:
		mvp 0, 0
		clc
		lda [D + Src]
		sbc [D + Count]
		sta [D + Src]
		lda [D + Src + 2]
		sbc 0
		sta [D + Src + 2]
		clc
		lda [D + Dst]
		sbc [D + Count]
		sta [D + Dst]
		lda [D + Dst + 2]
		sbc 0
		sta [D + Dst + 2]
		jsr Moved
		bra DownLoop

; Copies the destination, source and size arguments to Dst, Src and Bytes.
Arguments:
		.width (A, X), 16
		lda [D + Argument]
		sta [D + Dst]
		lda [D + Argument + 2]
		sta [D + Dst + 2]
		lda [D + Pushed]
		sta [D + Src]
		lda [D + Pushed + 2]
		sta [D + Src + 2]
		lda [D + Pushed + 4]
		sta [D + Bytes]
		lda [D + Pushed + 6]
		sta [D + Bytes + 2]
		rts

; Count = the bytes left less 1, at most 65535.
Limit:
		.width (A, X), 16
		lda [D + Bytes + 2]
		bne Most
		lda [D + Bytes]
		dec A
		sta [D + Count]
		rts
Most:
		.width (A, X), 16
		lda 65535
		sta [D + Count]
		rts

; Bytes -= Count + 1 (the clear carry takes the 1 more).
Moved:
		.width (A, X), 16
		clc
		lda [D + Bytes]
		sbc [D + Count]
		sta [D + Bytes]
		lda [D + Bytes + 2]
		sbc 0
		sta [D + Bytes + 2]
		rts


; The other string functions (string.cpp keeps strstr, which uses these), a byte at a time
; through the 24 bit pointers ([[D + n]:24 + Y]; memcmp a word at a time).  When Y wraps
; past 65535 the pointers' banks are moved on.  Each function (name.ax or name.axy: the first
; argument in A and X, an int after it in Y) pushes Y, X and A, 6 bytes of locals and D, and
; points D at the stack:
;	D + 1		the caller's D
;	D + 3		Hi: a size_t count's high word (X is its low word)
;	D + 5		Save: the destination to return, or what is being worked out
;	D + 9		First: the first argument (A and X)
;	D + 13		IntArg: memchr's, strchr's and strrchr's int (Y)
;	D + 15		the return address
;	D + 18		the pushed arguments: Second (the second pointer, then a size_t), or memchr's size

Hi			= 3
Save		= 5
First		= 9
IntArg		= 13
Second		= 18
SizeArg		= 18


		.section text.String816
		.global _memcmp, _memchr, _strlen, _strcpy, _strncpy, _strcat, _strncat
		.global _strcmp, _strncmp, _strchr, _strrchr
		.global _memcmp.ax, _memchr.axy, _strlen.ax, _strcpy.ax, _strncpy.ax, _strcat.ax, _strncat.ax
		.global _strcmp.ax, _strncmp.ax, _strchr.axy, _strrchr.axy

; The stack entries.
_strlen:
		.width (A, X), 16
		lda [S + 6]
		tax
		lda [S + 4]
		brl _strlen.ax

_strcpy:
		.width (A, X), 16
		lda [S + 10]
		pha
		lda [S + 10]
		pha
		lda [S + 10]
		tax
		lda [S + 8]
		jsl _strcpy.ax
		ply
		ply
		rtl

_strcat:
		.width (A, X), 16
		lda [S + 10]
		pha
		lda [S + 10]
		pha
		lda [S + 10]
		tax
		lda [S + 8]
		jsl _strcat.ax
		ply
		ply
		rtl

_strncpy:
		.width (A, X), 16
		lda [S + 14]
		pha
		lda [S + 14]
		pha
		lda [S + 14]
		pha
		lda [S + 14]
		pha
		lda [S + 14]
		tax
		lda [S + 12]
		jsl _strncpy.ax
		tay
		tsc
		clc
		adc 8
		tcs
		tya
		rtl

_strncat:
		.width (A, X), 16
		lda [S + 14]
		pha
		lda [S + 14]
		pha
		lda [S + 14]
		pha
		lda [S + 14]
		pha
		lda [S + 14]
		tax
		lda [S + 12]
		jsl _strncat.ax
		tay
		tsc
		clc
		adc 8
		tcs
		tya
		rtl

_strcmp:
		.width (A, X), 16
		lda [S + 10]
		pha
		lda [S + 10]
		pha
		lda [S + 10]
		tax
		lda [S + 8]
		jsl _strcmp.ax
		ply
		ply
		rtl

_strncmp:
		.width (A, X), 16
		lda [S + 14]
		pha
		lda [S + 14]
		pha
		lda [S + 14]
		pha
		lda [S + 14]
		pha
		lda [S + 14]
		tax
		lda [S + 12]
		jsl _strncmp.ax
		tay
		tsc
		clc
		adc 8
		tcs
		tya
		rtl

_memcmp:
		.width (A, X), 16
		lda [S + 14]
		pha
		lda [S + 14]
		pha
		lda [S + 14]
		pha
		lda [S + 14]
		pha
		lda [S + 14]
		tax
		lda [S + 12]
		jsl _memcmp.ax
		tay
		tsc
		clc
		adc 8
		tcs
		tya
		rtl

_memchr:
		.width (A, X), 16
		lda [S + 12]
		pha
		lda [S + 12]
		pha
		lda [S + 12]
		tay
		lda [S + 10]
		tax
		lda [S + 8]
		jsl _memchr.axy
		ply
		ply
		rtl

_strchr:
		.width (A, X), 16
		lda [S + 8]
		tay
		lda [S + 6]
		tax
		lda [S + 4]
		brl _strchr.axy

_strrchr:
		.width (A, X), 16
		lda [S + 8]
		tay
		lda [S + 6]
		tax
		lda [S + 4]
		brl _strrchr.axy

; size_t strlen(const char* sz): A the low word, X the high.
_strlen.ax:
		.width (A, X), 16
		phy
		phx
		pha
		pea 0
		pea 0
		pea 0
		phd
		tsc
		tcd
		sep m
		ldy 0
		ldx 0
StrlenByte:
		.width A, 8
		.width X, 16
		lda [[D + First]:24 + Y]
		beq StrlenEnd
		iny
		bne StrlenByte
		inx
		inc [D + First + 2]
		bra StrlenByte
StrlenEnd:
		.width A, 8
		.width X, 16
		rep m
		tya
		pld
		tay
		tsc
		clc
		adc 12
		tcs
		tya
		rtl

; char* strcpy(char* szDest, const char* szSource)
_strcpy.ax:
		.width (A, X), 16
		phy
		phx
		pha
		pea 0
		pea 0
		pea 0
		phd
		tsc
		tcd
		jsr SaveFirst
		sep m
		ldy 0
		brl CopyString

; char* strcat(char* szDest, const char* szSource): strcpy to the destination's end.
_strcat.ax:
		.width (A, X), 16
		phy
		phx
		pha
		pea 0
		pea 0
		pea 0
		phd
		tsc
		tcd
		jsr SaveFirst
		jsr FindEnd
		brl CopyString

; char* strncpy(char* szDest, const char* szSource, size_t uiChars): the source's characters,
; then zeros, uiChars in all.
_strncpy.ax:
		.width (A, X), 16
		phy
		phx
		pha
		pea 0
		pea 0
		pea 0
		phd
		tsc
		tcd
		jsr SaveFirst
		ldx [D + Second + 4]
		lda [D + Second + 6]
		sta [D + Hi]
		sep m
		ldy 0
StrncpyChar:
		.width A, 8
		.width X, 16
		cpx 0
		bne StrncpyHave
		jsr More
		bcs StrncpyEnd
StrncpyHave:
		.width A, 8
		.width X, 16
		dex
		lda [[D + Second]:24 + Y]
		sta [[D + First]:24 + Y]
		beq StrncpyPadNext
		iny
		bne StrncpyChar
		inc [D + First + 2]
		inc [D + Second + 2]
		bra StrncpyChar
StrncpyPadNext:
		.width A, 8
		.width X, 16
		iny
		bne StrncpyPad
		inc [D + First + 2]
StrncpyPad:
		.width A, 8
		.width X, 16
		cpx 0
		bne StrncpyZero
		jsr More
		bcs StrncpyEnd
StrncpyZero:
		.width A, 8
		.width X, 16
		dex
		lda 0
		sta [[D + First]:24 + Y]
		bra StrncpyPadNext
StrncpyEnd:
		.width A, 8
		.width X, 16
		brl ReturnSaved

; char* strncat(char* szDest, const char* szSource, size_t uiChars): at most uiChars of the
; source at the destination's end, then a zero.
_strncat.ax:
		.width (A, X), 16
		phy
		phx
		pha
		pea 0
		pea 0
		pea 0
		phd
		tsc
		tcd
		jsr SaveFirst
		jsr FindEnd
		rep m
		ldx [D + Second + 4]
		lda [D + Second + 6]
		sta [D + Hi]
		sep m
StrncatChar:
		.width A, 8
		.width X, 16
		cpx 0
		bne StrncatHave
		jsr More
		bcs StrncatEnd
StrncatHave:
		.width A, 8
		.width X, 16
		dex
		lda [[D + Second]:24 + Y]
		beq StrncatEnd
		sta [[D + First]:24 + Y]
		iny
		bne StrncatChar
		inc [D + First + 2]
		inc [D + Second + 2]
		bra StrncatChar
StrncatEnd:
		.width A, 8
		.width X, 16
		lda 0
		sta [[D + First]:24 + Y]
		brl ReturnSaved

; Copies the source's characters to the destination at Y up to and with the zero (A 8 bits
; wide), then returns the saved destination.
CopyString:
		.width A, 8
		.width X, 16
		lda [[D + Second]:24 + Y]
		sta [[D + First]:24 + Y]
		beq ReturnSaved
		iny
		bne CopyString
		inc [D + First + 2]
		inc [D + Second + 2]
		bra CopyString
ReturnSaved:
		.width A, 8
		.width X, 16
		rep m
		lda [D + Save]
		ldx [D + Save + 2]
		pld
		tay
		tsc
		clc
		adc 12
		tcs
		tya
		rtl

; int strcmp(const char* sz1, const char* sz2)
_strcmp.ax:
		.width (A, X), 16
		phy
		phx
		pha
		pea 0
		pea 0
		pea 0
		phd
		tsc
		tcd
		sep m
		ldy 0
StrcmpChar:
		.width A, 8
		.width X, 16
		lda [[D + First]:24 + Y]
		cmp [[D + Second]:24 + Y]
		bne Difference
		cmp 0
		beq Same
		iny
		bne StrcmpChar
		inc [D + First + 2]
		inc [D + Second + 2]
		bra StrcmpChar

; int strncmp(const char* sz1, const char* sz2, size_t uiChars)
_strncmp.ax:
		.width (A, X), 16
		phy
		phx
		pha
		pea 0
		pea 0
		pea 0
		phd
		tsc
		tcd
		ldx [D + Second + 4]
		lda [D + Second + 6]
		sta [D + Hi]
		sep m
		ldy 0
StrncmpChar:
		.width A, 8
		.width X, 16
		cpx 0
		bne StrncmpHave
		jsr More
		bcs Same
StrncmpHave:
		.width A, 8
		.width X, 16
		dex
		lda [[D + First]:24 + Y]
		cmp [[D + Second]:24 + Y]
		bne Difference
		cmp 0
		beq Same
		iny
		bne StrncmpChar
		inc [D + First + 2]
		inc [D + Second + 2]
		bra StrncmpChar

; The result of a compare that found equal strings or memory (A 8 bits wide).
Same:
		.width A, 8
		.width X, 16
		rep m
		lda 0
		pld
		tay
		tsc
		clc
		adc 12
		tcs
		tya
		rtl

; The result of a compare that found two bytes that differ at Y: the first's (in A, 8 bits
; wide) less the second's, as unsigned chars.
Difference:
		.width A, 8
		.width X, 16
		rep m
		and 255
		sta [D + Save]
		lda [[D + Second]:24 + Y]
		and 255
		sta [D + Save + 2]
		lda [D + Save]
		sec
		sbc [D + Save + 2]
		pld
		tay
		tsc
		clc
		adc 12
		tcs
		tya
		rtl

; int memcmp(const void* pv1, const void* pv2, size_t uiBytes): words while there are two
; bytes left, then the byte left; the bytes of a word that differs are compared first to
; last.  One byte (CNumber compares a digit at a time) is compared straight away.
_memcmp.ax:
		.width (A, X), 16
		phy
		phx
		pha
		pea 0
		pea 0
		pea 0
		phd
		tsc
		tcd
		lda [D + Second + 6]
		bne MemcmpWords
		lda [D + Second + 4]
		cmp 1
		bne MemcmpWords
		ldy 0
		sep m
		lda [[D + First]:24 + Y]
		cmp [[D + Second]:24 + Y]
		bne Difference
		bra MemcmpSame
MemcmpWords:
		.width (A, X), 16
		lda [D + Second + 6]
		lsr A
		sta [D + Hi]
		lda [D + Second + 4]
		ror A
		tax
		ldy 0
MemcmpWord:
		.width (A, X), 16
		cpx 0
		bne MemcmpHave
		jsr MoreWide
		bcs MemcmpOdd
MemcmpHave:
		.width (A, X), 16
		dex
		lda [[D + First]:24 + Y]
		cmp [[D + Second]:24 + Y]
		bne MemcmpWordDiffers
		iny
		iny
		bne MemcmpWord
		inc [D + First + 2]
		inc [D + Second + 2]
		bra MemcmpWord
MemcmpWordDiffers:
		.width (A, X), 16
		sep m
		lda [[D + First]:24 + Y]
		cmp [[D + Second]:24 + Y]
		bne Difference
		iny
		lda [[D + First]:24 + Y]
		bra Difference
MemcmpOdd:
		.width (A, X), 16
		sep m
		lda [D + Second + 4]
		and 1
		beq MemcmpSame
		lda [[D + First]:24 + Y]
		cmp [[D + Second]:24 + Y]
		bne MemcmpDifference
MemcmpSame:
		.width A, 8
		.width X, 16
		rep m
		lda 0
		pld
		tay
		tsc
		clc
		adc 12
		tcs
		tya
		rtl
MemcmpDifference:
		.width A, 8
		.width X, 16
		brl Difference

; void* memchr(const void* pv, int iValue, size_t uiBytes): iValue as an unsigned char.
_memchr.axy:
		.width (A, X), 16
		phy
		phx
		pha
		pea 0
		pea 0
		pea 0
		phd
		tsc
		tcd
		ldx [D + SizeArg]
		lda [D + SizeArg + 2]
		sta [D + Hi]
		sep m
		ldy 0
MemchrByte:
		.width A, 8
		.width X, 16
		cpx 0
		bne MemchrHave
		jsr More
		bcs NotFound
MemchrHave:
		.width A, 8
		.width X, 16
		dex
		lda [[D + First]:24 + Y]
		cmp [D + IntArg]
		beq Found
		iny
		bne MemchrByte
		inc [D + First + 2]
		bra MemchrByte

; char* strchr(const char* sz, int c): c as a char, the zero at the end included.
_strchr.axy:
		.width (A, X), 16
		phy
		phx
		pha
		pea 0
		pea 0
		pea 0
		phd
		tsc
		tcd
		sep m
		ldy 0
StrchrChar:
		.width A, 8
		.width X, 16
		lda [[D + First]:24 + Y]
		cmp [D + IntArg]
		beq Found
		cmp 0
		beq NotFound
		iny
		bne StrchrChar
		inc [D + First + 2]
		bra StrchrChar

; The first argument's address plus Y (A 8 bits wide).
Found:
		.width A, 8
		.width X, 16
		rep m
		tya
		clc
		adc [D + First]
		sta [D + First]
		lda [D + First + 2]
		adc 0
		tax
		lda [D + First]
		pld
		tay
		tsc
		clc
		adc 12
		tcs
		tya
		rtl

; NULL (A 8 bits wide).
NotFound:
		.width A, 8
		.width X, 16
		rep m
		lda 0
		ldx 0
		pld
		tay
		tsc
		clc
		adc 12
		tcs
		tya
		rtl

; char* strrchr(const char* sz, int c): the last c, Save is where it is (NULL until found).
_strrchr.axy:
		.width (A, X), 16
		phy
		phx
		pha
		pea 0
		pea 0
		pea 0
		phd
		tsc
		tcd
		sep m
		ldy 0
StrrchrChar:
		.width A, 8
		.width X, 16
		lda [[D + First]:24 + Y]
		cmp [D + IntArg]
		bne StrrchrOther
		rep m
		tya
		clc
		adc [D + First]
		sta [D + Save]
		lda [D + First + 2]
		adc 0
		sta [D + Save + 2]
		sep m
		lda [[D + First]:24 + Y]
StrrchrOther:
		.width A, 8
		.width X, 16
		cmp 0
		beq StrrchrEnd
		iny
		bne StrrchrChar
		inc [D + First + 2]
		bra StrrchrChar
StrrchrEnd:
		.width A, 8
		.width X, 16
		brl ReturnSaved

; Save = the first argument (the destination to return).
SaveFirst:
		.width (A, X), 16
		lda [D + First]
		sta [D + Save]
		lda [D + First + 2]
		sta [D + Save + 2]
		rts

; Moves the destination on to its zero and Y to 0 (so the source is copied from its start),
; A 8 bits wide from then on.
FindEnd:
		.width (A, X), 16
		sep m
		ldy 0
FindEndChar:
		.width A, 8
		.width X, 16
		lda [[D + First]:24 + Y]
		beq FindEndDone
		iny
		bne FindEndChar
		inc [D + First + 2]
		bra FindEndChar
FindEndDone:
		.width A, 8
		.width X, 16
		rep m
		tya
		clc
		adc [D + First]
		sta [D + First]
		lda [D + First + 2]
		adc 0
		sta [D + First + 2]
		sep m
		ldy 0
		rts

; Called when X is 0: the next 65536 bytes from Hi.  Carry set when there are none (A 8 bits
; wide on the way in and out).
More:
		.width A, 8
		.width X, 16
		rep m
		jsr MoreWide
		sep m
		rts

; As More with A 16 bits wide.
MoreWide:
		.width (A, X), 16
		lda [D + Hi]
		beq MoreNone
		dec A
		sta [D + Hi]
		clc
		rts
MoreNone:
		.width (A, X), 16
		sec
		rts


		.section text.MemSwp816
		.global _MemSwp__FPKvPKvj, _MemSwp__FPKvPKvj.ax

; void MemSwp(const void* pv1, const void* pv2, lsize uiLength) (BaseLib's StringHelper.cpp, left
; out for the W65C816; stringW65C816_16.asm has MemSwp16, with a size length): a word at a time, then the byte left, Y running up from both pointers
; (carrying into the next bank), 64KB at a time.  The register entry (the compiler's register arguments: pv1 in A and
; X, pv2 and uiLength pushed) pushes pv1 and points D at the stack: pv1 is at D + 3 (MemSwpP1), then the return
; address, pv2 at D + 10 and the length at D + 14.
MemSwpP1	= 3
MemSwpP2	= 10
MemSwpLength	= 14

_MemSwp__FPKvPKvj.ax:
		.width (A, X), 16
		phx
		pha
		phd
		tsc
		tcd
MemSwpChunk:
		.width (A, X), 16
		lda [D + MemSwpLength + 2]
		beq MemSwpLast
		ldy 0
		ldx 32768
		jsr MemSwpWords
		sep m
		inc [D + MemSwpP1 + 2]
		inc [D + MemSwpP2 + 2]
		rep m
		dec [D + MemSwpLength + 2]
		bra MemSwpChunk
MemSwpLast:
		.width (A, X), 16
		ldy 0
		lda [D + MemSwpLength]
		lsr A
		beq MemSwpOdd
		tax
		jsr MemSwpWords
MemSwpOdd:
		.width (A, X), 16
		lda [D + MemSwpLength]
		and 1
		beq MemSwpDone
		sep m
		lda [[D + MemSwpP1]:24 + Y]
		pha
		lda [[D + MemSwpP2]:24 + Y]
		sta [[D + MemSwpP1]:24 + Y]
		pla
		sta [[D + MemSwpP2]:24 + Y]
		rep m
MemSwpDone:
		.width (A, X), 16
		pld
		ply
		ply
		rtl

; The stack entry (function pointers, assembly): pushes pv2 and the length again under its return address, loads pv1
; into A and X and calls the register entry.
_MemSwp__FPKvPKvj:
		.width (A, X), 16
		lda [S + 14]
		pha
		lda [S + 14]
		pha
		lda [S + 14]
		pha
		lda [S + 14]
		pha
		lda [S + 14]
		tax
		lda [S + 12]
		jsl _MemSwp__FPKvPKvj.ax
		tsc
		clc
		adc 8
		tcs
		rtl

; X words from Y, Y after them.
MemSwpWords:
		.width (A, X), 16
		lda [[D + MemSwpP1]:24 + Y]
		pha
		lda [[D + MemSwpP2]:24 + Y]
		sta [[D + MemSwpP1]:24 + Y]
		pla
		sta [[D + MemSwpP2]:24 + Y]
		iny
		iny
		dex
		bne MemSwpWords
		rts


; The bit array functions (BaseLib's IntegerHelper.h) with lsize (32 bit) counts and results:
; FindFirstSetBitLarge, FindFirstClearBitLarge, FindLastSetBitLarge and FindLastClearBitLarge
; (FindNextSetBitLarge and FindNextClearBitLarge are C++ on them).  stringW65C816_16.asm has the
; ones with size counts, and the uint32 and uint64 FindFirstClearBit and FindFirstSetBit.  Bit
; n is bit n % 8 of byte n / 8, so a word read from the array has its bits in order.  The
; arrays are read a word at a time (xored with Invert, 0 or $FFFF: set or clear bits),
; through their 24 bit pointer + Y; the pointer's bank moves on when Y wraps.  The last
; word's bits past the count are masked off (BitsMasks16); its other byte is read even when
; it is past the array's end.  BitsCtz16 and BitsLog2_16 (also stringW65C816_16.asm's) find the
; lowest and the highest set bit of a word through a table of a byte's.  ARRAY_LARGE_ELEMENT_NOT_FOUND
; ($FFFFFFFF) when there is none.  Each takes the compiler's register arguments (the array in A
; and X, the count pushed), pushes the array and 6 bytes of locals and D, and points D at the
; stack:
;	D + 1		the caller's D
;	D + 3		BitInvert: 0 or $FFFF
;	D + 5		BitHi: the words' count's high word (X is its low word)
;	D + 7		BitExtra: the 64KB banks passed (first), or the moved pointer's low word (last)
;	D + 9		BitArray: the array
;	D + 13		the return address
;	D + 16		the count of bits
; The stack entries (function pointers, assembly) push the count again, load the array into A
; and X and call the register entries (BitsLargeStack).

BitInvert	= 3
BitHi		= 5
BitExtra	= 7
BitArray	= 9
BitBits		= 16


		.section text.Bits816
		.relax
		.global BitsCtz16, BitsLog2_16
		.global _FindFirstSetBitLarge__FPvj, _FindFirstClearBitLarge__FPvj
		.global _FindLastSetBitLarge__FPvj, _FindLastClearBitLarge__FPvj
		.global _FindFirstSetBitLarge__FPvj.ax, _FindFirstClearBitLarge__FPvj.ax
		.global _FindLastSetBitLarge__FPvj.ax, _FindLastClearBitLarge__FPvj.ax

_FindFirstSetBitLarge__FPvj:
		.width (A, X), 16
		ldy _FindFirstSetBitLarge__FPvj.ax & 0xFFFF
		bra BitsLargeStack
_FindFirstClearBitLarge__FPvj:
		.width (A, X), 16
		ldy _FindFirstClearBitLarge__FPvj.ax & 0xFFFF
		bra BitsLargeStack
_FindLastSetBitLarge__FPvj:
		.width (A, X), 16
		ldy _FindLastSetBitLarge__FPvj.ax & 0xFFFF
		bra BitsLargeStack
_FindLastClearBitLarge__FPvj:
		.width (A, X), 16
		ldy _FindLastClearBitLarge__FPvj.ax & 0xFFFF

; Calls the register entry in this section at Y's address with the count (the 2 words above the return address)
; pushed again and the array (the 2 words above them) in A and X, and returns its result (A, X).
BitsLargeStack:
		.width (A, X), 16
		lda [S + 10]
		pha
		lda [S + 10]
		pha
		phk
		per BitsLargeStackReturn - 1
		dey
		phy
		lda [S + 15]
		tax
		lda [S + 13]
		rts
BitsLargeStackReturn:
		.width (A, X), 16
		ply
		ply
		rtl

; lsize FindFirstSetBitLarge(void* pvArray, lsize iArraySize)
_FindFirstSetBitLarge__FPvj.ax:
		.width (A, X), 16
		phx
		pha
		pea 0
		pea 0
		pea 0
		bra FindFirstLarge

; lsize FindFirstClearBitLarge(void* pvArray, lsize iArraySize)
_FindFirstClearBitLarge__FPvj.ax:
		.width (A, X), 16
		phx
		pha
		pea 0
		pea 0
		pea $FFFF
FindFirstLarge:
		.width (A, X), 16
		phd
		tsc
		tcd
		jsr BitWords
		ldy 0
FindFirstLargeWords:
		.width (A, X), 16
		txa
		bne FindFirstLargeWord
		lda [D + BitHi]
		beq FindFirstLargePart
		dec [D + BitHi]
FindFirstLargeWord:
		.width (A, X), 16
		dex
		lda [[D + BitArray]:24 + Y]
		eor [D + BitInvert]
		bne FindFirstLargeFound
		iny
		iny
		bne FindFirstLargeWords
		inc [D + BitArray + 2]
		inc [D + BitExtra]
		bra FindFirstLargeWords
FindFirstLargePart:
		.width (A, X), 16
		lda [D + BitBits]
		and 15
		beq FindFirstLargeNone
		asl A
		tax
		lda [[D + BitArray]:24 + Y]
		eor [D + BitInvert]
		and [BitsMasks16 + X]:24
		bne FindFirstLargeFound
FindFirstLargeNone:
		.width (A, X), 16
		lda $FFFF
		tax
		bra BitsLargeReturn
FindFirstLargeFound:
		.width (A, X), 16
		jsl BitsCtz16
		sta [D + BitInvert]
		tya
		xba
		and $00FF
		lsr A
		lsr A
		lsr A
		lsr A
		lsr A
		sta [D + BitHi]
		lda [D + BitExtra]
		asl A
		asl A
		asl A
		clc
		adc [D + BitHi]
		sta [D + BitHi]
		tya
		asl A
		asl A
		asl A
		clc
		adc [D + BitInvert]
		tay
		lda [D + BitHi]
		adc 0
		tax
		tya
BitsLargeReturn:
		.width (A, X), 16
		pld
		tay
		tsc
		clc
		adc 10
		tcs
		tya
		rtl

; lsize FindLastSetBitLarge(void* pvArray, lsize iArraySize)
_FindLastSetBitLarge__FPvj.ax:
		.width (A, X), 16
		phx
		pha
		pea 0
		pea 0
		pea 0
		bra FindLastLarge

; lsize FindLastClearBitLarge(void* pvArray, lsize iArraySize): the pointer is moved on to
; the last (partial) word, then Y runs down from it; the word's index is the count left.
_FindLastClearBitLarge__FPvj.ax:
		.width (A, X), 16
		phx
		pha
		pea 0
		pea 0
		pea $FFFF
FindLastLarge:
		.width (A, X), 16
		phd
		tsc
		tcd
		jsr BitWords
		txa
		asl A
		sta [D + BitExtra]
		lda [D + BitHi]
		rol A
		tay
		clc
		lda [D + BitArray]
		adc [D + BitExtra]
		sta [D + BitArray]
		tya
		adc [D + BitArray + 2]
		sta [D + BitArray + 2]
		ldy 0
		lda [D + BitBits]
		and 15
		beq FindLastLargeWords
		asl A
		phx
		tax
		lda [[D + BitArray]:24 + Y]
		eor [D + BitInvert]
		and [BitsMasks16 + X]:24
		sta [D + BitExtra]
		plx
		lda [D + BitExtra]
		bne FindLastLargeFound
FindLastLargeWords:
		.width (A, X), 16
		txa
		bne FindLastLargeWord
		lda [D + BitHi]
		beq FindLastLargeNone
		dec [D + BitHi]
FindLastLargeWord:
		.width (A, X), 16
		dex
		cpy 0
		bne FindLastLargeRead
		dec [D + BitArray + 2]
FindLastLargeRead:
		.width (A, X), 16
		dey
		dey
		lda [[D + BitArray]:24 + Y]
		eor [D + BitInvert]
		beq FindLastLargeWords
FindLastLargeFound:
		.width (A, X), 16
		phx
		jsl BitsLog2_16
		plx
		sta [D + BitExtra]
		txa
		xba
		and $00FF
		lsr A
		lsr A
		lsr A
		lsr A
		sta [D + BitInvert]
		lda [D + BitHi]
		asl A
		asl A
		asl A
		asl A
		ora [D + BitInvert]
		pha
		txa
		asl A
		asl A
		asl A
		asl A
		ora [D + BitExtra]
		plx
		bra BitsLargeReturn
FindLastLargeNone:
		.width (A, X), 16
		lda $FFFF
		tax
		bra BitsLargeReturn

; The count of words: the count of bits >> 4, the low word in X and the high in BitHi.
BitWords:
		.width (A, X), 16
		lda [D + BitBits + 2]
		sta [D + BitHi]
		lda [D + BitBits]
		ldx 4
BitWordsShift:
		.width (A, X), 16
		lsr [D + BitHi]
		ror A
		dex
		bne BitWordsShift
		tax
		rts

; BitsCtz16: the index of A's lowest set bit (A isn't 0); X changes, Y is kept.
BitsCtz16:
		.width (A, X), 16
		bit $00FF
		beq BitsCtz16High
		and $00FF
		tax
		lda [BitsCtz8 + X]:24
		and $00FF
		rtl
BitsCtz16High:
		.width (A, X), 16
		xba
		and $00FF
		tax
		lda [BitsCtz8 + X]:24
		and $00FF
		clc
		adc 8
		rtl

; BitsLog2_16: the index of A's highest set bit (A isn't 0); X changes, Y is kept.
BitsLog2_16:
		.width (A, X), 16
		bit $FF00
		beq BitsLog2_16Low
		xba
		and $00FF
		tax
		lda [BitsLog2_8 + X]:24
		and $00FF
		clc
		adc 8
		rtl
BitsLog2_16Low:
		.width (A, X), 16
		tax
		lda [BitsLog2_8 + X]:24
		and $00FF
		rtl


		.section rodata.Bits816
		.global BitsMasks16

; BitsMasks16: the low n bits set, for n from 0 to 15 (indexed by n * 2).
BitsMasks16:
		.word $0000, $0001, $0003, $0007, $000F, $001F, $003F, $007F, $00FF, $01FF, $03FF, $07FF, $0FFF, $1FFF, $3FFF, $7FFF

; A byte's lowest set bit (8 for 0), and a pad byte for the 16 bit read of the last.
BitsCtz8:
		.byte 8, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
		.byte 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
		.byte 5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
		.byte 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
		.byte 6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
		.byte 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
		.byte 5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
		.byte 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
		.byte 7, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
		.byte 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
		.byte 5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
		.byte 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
		.byte 6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
		.byte 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
		.byte 5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
		.byte 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
		.byte 0

; A byte's highest set bit (0 for 0), and a pad byte.
BitsLog2_8:
		.byte 0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3
		.byte 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4
		.byte 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5
		.byte 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5
		.byte 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6
		.byte 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6
		.byte 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6
		.byte 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6
		.byte 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7
		.byte 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7
		.byte 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7
		.byte 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7
		.byte 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7
		.byte 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7
		.byte 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7
		.byte 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7
		.byte 0
