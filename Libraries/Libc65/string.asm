; The memory and string functions for the 65C64, written for speed with its block and
; vector operations: one instruction steps through memory a byte (or 8 bytes) at a time,
; repeating until its count runs out or its stop condition holds.  string.cpp stays as
; their reference (its 65C64 build only has strstr, which uses these).  The results are
; the same: comparisons give the difference of the first bytes that differ, as unsigned
; chars.
;
;	vcpy W, V, +1, A, 64		copies 8 bytes [W] -> [V] a step, A + 1 steps
;	vcmp W, V, +1, A, 8, !z		compares bytes until two differ
;	btst B, V, +1, A, z			finds the zero at the end of a string (B = $FF)
;	bcmp B, V, +1, A, z			finds a byte (B the byte)
;	bcpy B, V, +1, A			fills (B 8 bytes or 1 byte wide)
;
; A step moves V (and W) on past the element, takes one from A and stops after the step
; that makes A negative or that meets the stop condition, so A is one less than the
; count and V is one element past where it stopped.
;
; The calling convention is the compiler's (CPP65Generator.txt): a function's first
; three arguments are in A, B and Y at its register label (_memcpy.aby: a letter for
; each argument's register), where compiled code calls it; at its plain label they are
; all on the stack (the first at [5 + S] above the return address, then 4 bytes each),
; for calls through pointers and from assembly.  The result is in A, A, B, X and Y are 32
; bits wide on the way in and out and V and S are kept.  A, B, X, Y, W and the status
; flags change (W is a register like any other).
;
; memcpy, memmove, memset, memcmp and strlen work from the registers: their plain labels
; load them from the stack and go on into the register entries.  The others work from
; the stack (each pushes V first, so its arguments are at [9 + S], [13 + S] and
; [17 + S]): their register labels push the registers and call them.
;
; Cycles: a step is its opcode cycle and a cycle for each 16 bit word read or written.
; For 1000 bytes on the simulator, with string.cpp's compiled code in brackets: memcpy
; 1184 (15093), unaligned 1311 (54087), memmove overlapping 1324 (52122), memset 698
; (11612), memcmp 1194 (16101), memchr 2067 (52065), strlen 2055 (22064), strcpy 3049
; (39082), strcmp 3212 (60081), strchr 4074 (51084).  The assembly's are 4 less each now:
; W is no longer pushed and pulled (2026-11-14).


; void* memcpy(void* pvDest, const void* pvSource, size_t uiBytes) and memmove, which
; copies from the end down when the destination overlaps the source above it.
		.section text.MemCopy
		.global _memcpy, _memmove, _memcpy.aby, _memmove.aby

_memcpy:
		cpy Y, [13 + S]
		cpy B, [9 + S]
		cpy A, [5 + S]
_memcpy.aby:
		psh V
		cpy X, A
		cpy V, A
		cpy W, B
		cpy A, Y

; V destination, W source, A bytes, X the destination for the result: 8 bytes a step, then
; the bytes left.
CopyUp:
		cpy B, A
		shr A, 3
		bras z, CopyUpBytes
		sub A, 1
		vcpy W, V, +1, A, 64
CopyUpBytes:
		and B, 7
		bras z, CopyDone
		sub B, 1
		cpy A, B
		vcpy W, V, +1, A, 8
CopyDone:
		cpy A, X
		pul V
		ret

_memmove:
		cpy Y, [13 + S]
		cpy B, [9 + S]
		cpy A, [5 + S]
_memmove.aby:
		psh V
		cpy X, A
		cpy V, A
		cpy W, B
		cpy A, Y

		; Destination - source is below the count only when the destination is in the
		; source (above its start): then copy down from the end.
		cpy Y, V
		sub Y, W
		cmp Y, A
		bras c, CopyUp

		cpy B, A
		add V, A
		add W, A
		shr A, 3
		bras z, CopyDownBytes
		sub V, 8
		sub W, 8
		sub A, 1
		vcpy W, V, -1, A, 64
		add V, 7
		add W, 7
		bra CopyDownTail
CopyDownBytes:
		sub V, 1
		sub W, 1
CopyDownTail:
		and B, 7
		bras z, CopyDone
		sub B, 1
		cpy A, B
		vcpy W, V, -1, A, 8
		bra CopyDone


; void* memset(void* pvDest, int iValue, size_t uiBytes): the byte made 8 bytes wide fills
; 8 bytes a step, then the bytes left.
		.section text._memset
		.global _memset, _memset.aby

_memset:
		cpy Y, [13 + S]
		cpy B, [9 + S]
		cpy A, [5 + S]
_memset.aby:
		psh V
		psh B
		psh A
		cpy V, A
		wtth (B, X), 64
		cpy X, 0
		wth X, 8
		cpy X, [5 + S]
		wth X, 64
		cpy B, X
		shl X, 8
		or B, X
		cpy X, B
		shl X, 16
		or B, X
		cpy X, B
		shl X, 32
		or B, X

		cpy A, Y
		shr A, 3
		bras z, SetBytes
		sub A, 1
		bcpy B, V, +1, A
SetBytes:
		wth B, 8
		and Y, 7
		bras z, SetDone
		sub Y, 1
		cpy A, Y
		bcpy B, V, +1, A
SetDone:
		wtth (B, X), 32
		pul A
		add S, 4
		pul V
		ret


; int memcmp(const void* pv1, const void* pv2, size_t uiBytes), and strcmp and strncmp,
; which find how many bytes to compare (up to and with the first string's zero) and
; then compare them as memcmp does.  Equal memory is passed 8 bytes a step; the 8 bytes
; that differ are compared again a byte a step to find the first byte that differs.
		.section text.MemCompare
		.global _memcmp, _strcmp, _strncmp, _memcmp.aby, _strcmp.ab, _strncmp.aby

_memcmp:
		cpy Y, [13 + S]
		cpy B, [9 + S]
		cpy A, [5 + S]
_memcmp.aby:
		psh V
		cpy V, A
		cpy W, B
		cpy B, Y

; V and W the memory, B bytes.
Compare:
		cpy A, B
		shr A, 3
		bras z, CompareTail
		sub A, 1
		vcmp W, V, +1, A, 64, !z
		bras z, CompareTail
		sub V, 8
		sub W, 8
		cpy A, 7
		bra CompareBytes
CompareTail:
		and B, 7
		bras z, CompareSame
		sub B, 1
		cpy A, B
CompareBytes:
		vcmp W, V, +1, A, 8, !z
		bras z, CompareSame

		; [V - 1] and [W - 1] differ: their difference as unsigned chars.
		wtth (A, B), 32
		cpy A, 0
		cpy B, 0
		wtth (A, B), 8
		cpy A, [-1 + V]
		cpy B, [-1 + W]
		wtth (A, B), 32
		sub A, B
		pul V
		ret
CompareSame:
		cpy A, 0
		pul V
		ret

_strcmp.ab:
		psh B
		psh A
		jsr _strcmp
		add S, 8
		ret

_strncmp.aby:
		psh Y
		psh B
		psh A
		jsr _strncmp
		add S, 12
		ret

_strcmp:
		psh V
		cpy V, [9 + S]
		cpy A, $7FFFFFFF
		wth B, 8
		cpy B, $FF
		btst B, V, +1, A, z
		wth B, 32
		cpy B, V
		cpy X, [9 + S]
		sub B, X
		cpy V, [9 + S]
		cpy W, [13 + S]
		bra Compare

_strncmp:
		psh V
		cpy V, [9 + S]
		cpy A, [17 + S]
		cmp A, 0
		bras z, CompareSame
		sub A, 1
		wth B, 8
		cpy B, $FF
		btst B, V, +1, A, z
		wth B, 32
		cpy B, V
		cpy X, [9 + S]
		sub B, X
		cpy V, [9 + S]
		cpy W, [13 + S]
		bra Compare


; void* memchr(const void* pv, int iValue, size_t uiBytes): bcmp stops at the byte.
		.section text._memchr
		.global _memchr, _memchr.aby

_memchr.aby:
		psh Y
		psh B
		psh A
		jsr _memchr
		add S, 12
		ret

_memchr:
		psh V
		cpy V, [9 + S]
		cpy A, [17 + S]
		cmp A, 0
		bras z, FindNone
		sub A, 1
		wth B, 8
		cpy B, [13 + S]
		bcmp B, V, +1, A, z
		wth B, 32
		brac z, FindNone
		cpy A, V
		sub A, 1
		pul V
		ret
FindNone:
		cpy A, 0
		pul V
		ret


; size_t strlen(const char* sz): btst stops at the zero.
		.section text._strlen
		.global _strlen, _strlen.a

_strlen:
		cpy A, [5 + S]
_strlen.a:
		psh V
		cpy X, A
		cpy V, A
		cpy A, $7FFFFFFF
		wth B, 8
		cpy B, $FF
		btst B, V, +1, A, z
		wth B, 32
		cpy A, V
		sub A, X
		sub A, 1
		pul V
		ret


; char* strcpy(char* szDest, const char* szSource): vcpy stops once it has copied the
; zero.
		.section text._strcpy
		.global _strcpy, _strcpy.ab

_strcpy.ab:
		psh B
		psh A
		jsr _strcpy
		add S, 8
		ret

_strcpy:
		psh V
		cpy V, [9 + S]
		cpy W, [13 + S]
		cpy A, $7FFFFFFF
		vcpy W, V, +1, A, 8, z
		cpy A, [9 + S]
		pul V
		ret


; char* strncpy(char* szDest, const char* szSource, size_t uiChars): copies up to and
; with the zero, at most uiChars bytes, then fills the rest of the uiChars with zeros.
		.section text._strncpy
		.global _strncpy, _strncpy.aby

_strncpy.aby:
		psh Y
		psh B
		psh A
		jsr _strncpy
		add S, 12
		ret

_strncpy:
		psh V
		cpy V, [9 + S]
		cpy W, [13 + S]
		cpy A, [17 + S]
		cmp A, 0
		bras z, CopyNDone
		sub A, 1
		vcpy W, V, +1, A, 8, z
		bras v, CopyNDone

		; Stopped at the zero with A + 1 bytes still to fill.
		wth B, 8
		cpy B, 0
		bcpy B, V, +1, A
		wth B, 32
CopyNDone:
		cpy A, [9 + S]
		pul V
		ret


; char* strcat(char* szDest, const char* szSource): btst finds the end of szDest, then
; vcpy copies szSource and its zero there.
		.section text._strcat
		.global _strcat, _strcat.ab

_strcat.ab:
		psh B
		psh A
		jsr _strcat
		add S, 8
		ret

_strcat:
		psh V
		cpy V, [9 + S]
		cpy A, $7FFFFFFF
		wth B, 8
		cpy B, $FF
		btst B, V, +1, A, z
		wth B, 32
		sub V, 1
		cpy W, [13 + S]
		cpy A, $7FFFFFFF
		vcpy W, V, +1, A, 8, z
		cpy A, [9 + S]
		pul V
		ret


; char* strncat(char* szDest, const char* szSource, size_t uiChars): copies at most
; uiChars characters of szSource to the end of szDest, and a zero after them if the zero
; was not copied.
		.section text._strncat
		.global _strncat, _strncat.aby

_strncat.aby:
		psh Y
		psh B
		psh A
		jsr _strncat
		add S, 12
		ret

_strncat:
		psh V
		cpy V, [9 + S]
		cpy A, $7FFFFFFF
		wth B, 8
		cpy B, $FF
		btst B, V, +1, A, z
		sub V, 1
		cpy A, [17 + S]
		cmp A, 0
		bras z, CatNDone
		cpy W, [13 + S]
		sub A, 1
		vcpy W, V, +1, A, 8, z
		bras z, CatNDone
		cpy B, 0
		cpy [0 + V], B
CatNDone:
		wth B, 32
		cpy A, [9 + S]
		pul V
		ret


; char* strchr(const char* sz, int c) and strrchr: btst finds the zero, then bcmp looks
; for c in the string with its zero (so c can be the zero), forwards for strchr and back
; from the zero for strrchr.
		.section text.StringFind
		.global _strchr, _strrchr, _strchr.ab, _strrchr.ab

_strchr.ab:
		psh B
		psh A
		jsr _strchr
		add S, 8
		ret

_strrchr.ab:
		psh B
		psh A
		jsr _strrchr
		add S, 8
		ret

_strchr:
		psh V
		cpy V, [9 + S]
		cpy A, $7FFFFFFF
		wth B, 8
		cpy B, $FF
		btst B, V, +1, A, z
		cpy A, V
		cpy X, [9 + S]
		sub A, X
		sub A, 1
		cpy V, [9 + S]
		cpy B, [13 + S]
		bcmp B, V, +1, A, z
		brac z, StringNone
		cpy A, V
		sub A, 1
		wth B, 32
		pul V
		ret

_strrchr:
		psh V
		cpy V, [9 + S]
		cpy A, $7FFFFFFF
		wth B, 8
		cpy B, $FF
		btst B, V, +1, A, z
		sub V, 1
		cpy A, V
		cpy X, [9 + S]
		sub A, X
		cpy B, [13 + S]
		bcmp B, V, -1, A, z
		brac z, StringNone
		cpy A, V
		add A, 1
		wth B, 32
		pul V
		ret

StringNone:
		cpy A, 0
		wth B, 32
		pul V
		ret

