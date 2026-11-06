;  ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **
;
; Copyright (c) 2026 Andrew Paterson
;
; This file is part of The Codaphela Project: Codaphela CppParserLib
;
; Codaphela CppParserLib is free software: you can redistribute it and/or modify
; it under the terms of the GNU Lesser General Public License as published by
; the Free Software Foundation, either version 3 of the License, or
; (at your option) any later version.
;
; Codaphela CppParserLib is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; GNU Lesser General Public License for more details.
;
; You should have received a copy of the GNU Lesser General Public License
; along with Codaphela CppParserLib.  If not, see <http://www.gnu.org/licenses/>.
;
; ** ------------------------------------------------------------------------ 
;
; IR816Runtime-Common.asm: the W65C816 back end's runtime routines that are the same with the math tables
; (IR816Runtime-MathTables.asm) or without them (IR816Runtime-MathSafe.asm): the divides, the 64 bit multiply, the 32
; and 64 bit shifts, the BCD adds and the leading and trailing zero counts.  CCPP65Builder links this and one of the
; math files as libraries (each routine in its own section, so only the ones a program uses are kept).  The compiler
; calls them (IR816Runtime.cpp says how each takes its operands).
;


; R816_UDiv64, R816_URem64, R816_SDiv64 and R816_SRem64: the 64 bit divides, one body, with the compiler's convention
; for a 64 bit result (D is pointed at the stack): each entry pushes its flags (bit 0: the remainder is the result, bit
; 1: signed), then D + 3 to D + 10 is the remainder, D + 11 the flags, then the caller's return address, the hidden
; result pointer at D + 16, the dividend (shifted into the quotient) at D + 20 and the divisor at D + 28.  Signed, the
; operands are made their magnitudes in place (the caller's pushed copies), the flags' bit 15 is set when the quotient
; is negative and bit 14 when the remainder is (the dividend's sign).  When both magnitudes fit in 32 bits (file
; positions and sizes nearly always do) R816_UDiv32 works it out on a scratch frame 32 bytes below; otherwise zero words
; at the top of the dividend are skipped 16 steps at a time (the remainder stays 0 through them) and the rest is a bit
; at a time.  The result is written through the pointer, and the remainder is left at the caller's D + 5 to D + 12 (its
; scratch bytes), where a urem or srem of the same operands written straight after the divide takes it
; (CIR816Backend::RemainderLeft).
		.section text.R816_Div64
		.global R816_UDiv64
		.global R816_URem64
		.global R816_SDiv64
		.global R816_SRem64
		.width (A, X), 16
		.relax
R816_UDiv64:
		pea 0
		bra R816_Div64_Body
R816_URem64:
		pea 1
		bra R816_Div64_Body
R816_SDiv64:
		pea 2
		bra R816_Div64_Body
R816_SRem64:
		pea 3
		bra R816_Div64_Body
R816_Div64_Body:
		pea 0
		pea 0
		pea 0
		pea 0
		phd
		tsc
		tcd
		lda [D + 11]
		and 2
		beq R816_Div64_Magnitudes
		lda [D + 26]
		bpl R816_Div64_APositive
		lda 0
		sec
		sbc [D + 20]
		sta [D + 20]
		lda 0
		sbc [D + 22]
		sta [D + 22]
		lda 0
		sbc [D + 24]
		sta [D + 24]
		lda 0
		sbc [D + 26]
		sta [D + 26]
		lda [D + 11]
		eor 0xC000
		sta [D + 11]
R816_Div64_APositive:
		lda [D + 34]
		bpl R816_Div64_Magnitudes
		lda 0
		sec
		sbc [D + 28]
		sta [D + 28]
		lda 0
		sbc [D + 30]
		sta [D + 30]
		lda 0
		sbc [D + 32]
		sta [D + 32]
		lda 0
		sbc [D + 34]
		sta [D + 34]
		lda [D + 11]
		eor 0x8000
		sta [D + 11]
R816_Div64_Magnitudes:
		lda [D + 24]
		ora [D + 26]
		ora [D + 32]
		ora [D + 34]
		bne R816_Div64_Long
		tsc
		sec
		sbc 32
		tcs
		tcd
		lda [D + 52]
		sta [D + 15]
		lda [D + 54]
		sta [D + 17]
		lda [D + 60]
		sta [D + 19]
		lda [D + 62]
		sta [D + 21]
		jsl R816_UDiv32
		sta [D + 52]
		txa
		sta [D + 54]
		lda [D + 5]
		sta [D + 35]
		lda [D + 7]
		sta [D + 37]
		tsc
		clc
		adc 32
		tcs
		tcd
		stz [D + 7]
		stz [D + 9]
		bra R816_Div64_Signs
R816_Div64_Long:
		ldx 64
R816_Div64_Skip:
		lda [D + 26]
		bne R816_Div64_Bit
		cpx 16
		beq R816_Div64_Bit
		lda [D + 24]
		sta [D + 26]
		lda [D + 22]
		sta [D + 24]
		lda [D + 20]
		sta [D + 22]
		stz [D + 20]
		txa
		sec
		sbc 16
		tax
		bra R816_Div64_Skip
R816_Div64_Bit:
		asl [D + 20]
		rol [D + 22]
		rol [D + 24]
		rol [D + 26]
		rol [D + 3]
		rol [D + 5]
		rol [D + 7]
		rol [D + 9]
		bcs R816_Div64_Subtract
		lda [D + 9]
		cmp [D + 34]
		bcc R816_Div64_Next
		bne R816_Div64_Subtract
		lda [D + 7]
		cmp [D + 32]
		bcc R816_Div64_Next
		bne R816_Div64_Subtract
		lda [D + 5]
		cmp [D + 30]
		bcc R816_Div64_Next
		bne R816_Div64_Subtract
		lda [D + 3]
		cmp [D + 28]
		bcc R816_Div64_Next
R816_Div64_Subtract:
		sec
		lda [D + 3]
		sbc [D + 28]
		sta [D + 3]
		lda [D + 5]
		sbc [D + 30]
		sta [D + 5]
		lda [D + 7]
		sbc [D + 32]
		sta [D + 7]
		lda [D + 9]
		sbc [D + 34]
		sta [D + 9]
		inc [D + 20]
R816_Div64_Next:
		dex
		bne R816_Div64_Bit
R816_Div64_Signs:
		lda [D + 11]
		bpl R816_Div64_QPositive
		lda 0
		sec
		sbc [D + 20]
		sta [D + 20]
		lda 0
		sbc [D + 22]
		sta [D + 22]
		lda 0
		sbc [D + 24]
		sta [D + 24]
		lda 0
		sbc [D + 26]
		sta [D + 26]
R816_Div64_QPositive:
		lda [D + 11]
		and 0x4000
		beq R816_Div64_RPositive
		lda 0
		sec
		sbc [D + 3]
		sta [D + 3]
		lda 0
		sbc [D + 5]
		sta [D + 5]
		lda 0
		sbc [D + 7]
		sta [D + 7]
		lda 0
		sbc [D + 9]
		sta [D + 9]
R816_Div64_RPositive:
		lda [D + 11]
		lsr A
		bcs R816_Div64_Remainder
		ldy 0
		lda [D + 20]
		sta [[D + 16]:24 + Y]
		ldy 2
		lda [D + 22]
		sta [[D + 16]:24 + Y]
		ldy 4
		lda [D + 24]
		sta [[D + 16]:24 + Y]
		ldy 6
		lda [D + 26]
		sta [[D + 16]:24 + Y]
		bra R816_Div64_Written
R816_Div64_Remainder:
		ldy 0
		lda [D + 3]
		sta [[D + 16]:24 + Y]
		ldy 2
		lda [D + 5]
		sta [[D + 16]:24 + Y]
		ldy 4
		lda [D + 7]
		sta [[D + 16]:24 + Y]
		ldy 6
		lda [D + 9]
		sta [[D + 16]:24 + Y]
R816_Div64_Written:
		pld
		pla
		sta [D + 5]
		pla
		sta [D + 7]
		pla
		sta [D + 9]
		pla
		sta [D + 11]
		pla
		rtl


; R816_Mul64: the low 64 bits of a * b, with the compiler's convention (the result's pointer pushed last, then a, then
; b; D pointed at its own frame of 26 bytes, scratch bytes and the cross products' sum at D + 23).  aL * bL
; (R816_UMulWide32) plus (aL * bH + aH * bL) (R816_Mul32, each skipped when the high word is 0) 32 bits up.
		.section text.R816_Mul64
		.global R816_Mul64
R816_Mul64:
		.width (A, X), 16
		.relax
		phd
		tsc
		sec
		sbc 26
		tcs
		tcd
		stz [D + 23]
		stz [D + 25]
		lda [D + 48]
		ora [D + 50]
		beq R816_Mul64_NoHighB
		lda [D + 36]
		sta [D + 15]
		lda [D + 48]
		sta [D + 19]
		lda [D + 38]
		sta [D + 17]
		lda [D + 50]
		sta [D + 21]
		jsl R816_Mul32
		sta [D + 23]
		stx [D + 25]
R816_Mul64_NoHighB:
		lda [D + 40]
		ora [D + 42]
		beq R816_Mul64_NoHighA
		lda [D + 40]
		sta [D + 15]
		lda [D + 44]
		sta [D + 19]
		lda [D + 42]
		sta [D + 17]
		lda [D + 46]
		sta [D + 21]
		jsl R816_Mul32
		clc
		adc [D + 23]
		sta [D + 23]
		txa
		adc [D + 25]
		sta [D + 25]
R816_Mul64_NoHighA:
		lda [D + 36]
		sta [D + 15]
		lda [D + 44]
		sta [D + 19]
		lda [D + 38]
		sta [D + 17]
		lda [D + 46]
		sta [D + 21]
		jsl R816_UMulWide32
		lda [D + 9]
		clc
		adc [D + 23]
		sta [D + 9]
		lda [D + 11]
		adc [D + 25]
		sta [D + 11]
		ldy 0
		lda [D + 5]
		sta [[D + 32]:24 + Y]
		ldy 2
		lda [D + 7]
		sta [[D + 32]:24 + Y]
		ldy 4
		lda [D + 9]
		sta [[D + 32]:24 + Y]
		ldy 6
		lda [D + 11]
		sta [[D + 32]:24 + Y]
		tsc
		clc
		adc 26
		tcs
		pld
		rtl


; R816_Shl64, R816_LShr64 and R816_AShr64 shift the 64 bit value in the caller's frame at D + X where it is;
; R816_Shl32, R816_LShr32 and R816_AShr32 the 32 bit value at the caller's scratch argument a (D + 15) and return it
; in A (low) and X (high).  The count is in A (its low 6 bits; a 32 bit value shifted 32 or more is filled).  The
; whole words go first (q = count / 16, so only the words left, m of them, are shifted further: a shift by 52 shifts
; one word 4 bits), then a byte (a word read a byte along), then the 1 to 7 bits left: one word in A and the others
; rolled in memory, counted down in Y (64 bits: X is the value's place), or jumped into an unrolled run of 7 steps (32
; bits).  AShr's sign word is worked out first, in the caller's scratch count word (D + 13).
		.section text.R816_Shl64
		.global R816_Shl64
R816_Shl64:
		.width (A, X), 16
		.relax
		and 63
		bne R816_Shl64_Some
		rtl
R816_Shl64_Some:
		cmp 32
		bcs R816_Shl64_Q23
		cmp 16
		bcs R816_Shl64_Q1
		cmp 8
		bcc R816_Shl64_R0
		tay
		lda [D + 5 + X]
		sta [D + 6 + X]
		lda [D + 3 + X]
		sta [D + 4 + X]
		lda [D + 1 + X]
		sta [D + 2 + X]
		lda [D + X]
		xba
		and 0xFF00
		sta [D + X]
		tya
		and 7
		bne R816_Shl64_R0
		rtl
R816_Shl64_R0:
		tay
		lda [D + X]
R816_Shl64_L0:
		asl A
		rol [D + 2 + X]
		rol [D + 4 + X]
		rol [D + 6 + X]
		dey
		bne R816_Shl64_L0
		sta [D + X]
		rtl
R816_Shl64_Q1:
		tay
		lda [D + 4 + X]
		sta [D + 6 + X]
		lda [D + 2 + X]
		sta [D + 4 + X]
		lda [D + X]
		sta [D + 2 + X]
		stz [D + X]
		tya
		and 15
		bne R816_Shl64_B1
		rtl
R816_Shl64_B1:
		cmp 8
		bcc R816_Shl64_R1
		tay
		lda [D + 5 + X]
		sta [D + 6 + X]
		lda [D + 3 + X]
		sta [D + 4 + X]
		lda [D + 2 + X]
		xba
		and 0xFF00
		sta [D + 2 + X]
		tya
		and 7
		bne R816_Shl64_R1
		rtl
R816_Shl64_R1:
		tay
		lda [D + 2 + X]
R816_Shl64_L1:
		asl A
		rol [D + 4 + X]
		rol [D + 6 + X]
		dey
		bne R816_Shl64_L1
		sta [D + 2 + X]
		rtl
R816_Shl64_Q23:
		cmp 48
		bcs R816_Shl64_Q3
R816_Shl64_Q2:
		tay
		lda [D + 2 + X]
		sta [D + 6 + X]
		lda [D + X]
		sta [D + 4 + X]
		stz [D + 2 + X]
		stz [D + X]
		tya
		and 15
		bne R816_Shl64_B2
		rtl
R816_Shl64_B2:
		cmp 8
		bcc R816_Shl64_R2
		tay
		lda [D + 5 + X]
		sta [D + 6 + X]
		lda [D + 4 + X]
		xba
		and 0xFF00
		sta [D + 4 + X]
		tya
		and 7
		bne R816_Shl64_R2
		rtl
R816_Shl64_R2:
		tay
		lda [D + 4 + X]
R816_Shl64_L2:
		asl A
		rol [D + 6 + X]
		dey
		bne R816_Shl64_L2
		sta [D + 4 + X]
		rtl
R816_Shl64_Q3:
		tay
		lda [D + X]
		sta [D + 6 + X]
		stz [D + 4 + X]
		stz [D + 2 + X]
		stz [D + X]
		tya
		and 15
		bne R816_Shl64_B3
		rtl
R816_Shl64_B3:
		cmp 8
		bcc R816_Shl64_R3
		tay
		lda [D + 6 + X]
		xba
		and 0xFF00
		sta [D + 6 + X]
		tya
		and 7
		bne R816_Shl64_R3
		rtl
R816_Shl64_R3:
		tay
		lda [D + 6 + X]
R816_Shl64_L3:
		asl A
		dey
		bne R816_Shl64_L3
		sta [D + 6 + X]
		rtl
R816_Shl64_Q4:
		stz [D + X]
		stz [D + 2 + X]
		stz [D + 4 + X]
		stz [D + 6 + X]
		rtl


;
		.section text.R816_LShr64
		.global R816_LShr64
R816_LShr64:
		.width (A, X), 16
		.relax
		and 63
		bne R816_LShr64_Some
		rtl
R816_LShr64_Some:
		cmp 32
		bcs R816_LShr64_Q23
		cmp 16
		bcs R816_LShr64_Q1
		cmp 8
		bcc R816_LShr64_R0
		tay
		lda [D + 1 + X]
		sta [D + X]
		lda [D + 3 + X]
		sta [D + 2 + X]
		lda [D + 5 + X]
		sta [D + 4 + X]
		lda [D + 6 + X]
		xba
		and 0xFF
		sta [D + 6 + X]
		tya
		and 7
		bne R816_LShr64_R0
		rtl
R816_LShr64_R0:
		tay
		lda [D + 6 + X]
R816_LShr64_L0:
		lsr A
		ror [D + 4 + X]
		ror [D + 2 + X]
		ror [D + X]
		dey
		bne R816_LShr64_L0
		sta [D + 6 + X]
		rtl
R816_LShr64_Q1:
		tay
		lda [D + 2 + X]
		sta [D + X]
		lda [D + 4 + X]
		sta [D + 2 + X]
		lda [D + 6 + X]
		sta [D + 4 + X]
		stz [D + 6 + X]
		tya
		and 15
		bne R816_LShr64_B1
		rtl
R816_LShr64_B1:
		cmp 8
		bcc R816_LShr64_R1
		tay
		lda [D + 1 + X]
		sta [D + X]
		lda [D + 3 + X]
		sta [D + 2 + X]
		lda [D + 4 + X]
		xba
		and 0xFF
		sta [D + 4 + X]
		tya
		and 7
		bne R816_LShr64_R1
		rtl
R816_LShr64_R1:
		tay
		lda [D + 4 + X]
R816_LShr64_L1:
		lsr A
		ror [D + 2 + X]
		ror [D + X]
		dey
		bne R816_LShr64_L1
		sta [D + 4 + X]
		rtl
R816_LShr64_Q23:
		cmp 48
		bcs R816_LShr64_Q3
R816_LShr64_Q2:
		tay
		lda [D + 4 + X]
		sta [D + X]
		lda [D + 6 + X]
		sta [D + 2 + X]
		stz [D + 4 + X]
		stz [D + 6 + X]
		tya
		and 15
		bne R816_LShr64_B2
		rtl
R816_LShr64_B2:
		cmp 8
		bcc R816_LShr64_R2
		tay
		lda [D + 1 + X]
		sta [D + X]
		lda [D + 2 + X]
		xba
		and 0xFF
		sta [D + 2 + X]
		tya
		and 7
		bne R816_LShr64_R2
		rtl
R816_LShr64_R2:
		tay
		lda [D + 2 + X]
R816_LShr64_L2:
		lsr A
		ror [D + X]
		dey
		bne R816_LShr64_L2
		sta [D + 2 + X]
		rtl
R816_LShr64_Q3:
		tay
		lda [D + 6 + X]
		sta [D + X]
		stz [D + 2 + X]
		stz [D + 4 + X]
		stz [D + 6 + X]
		tya
		and 15
		bne R816_LShr64_B3
		rtl
R816_LShr64_B3:
		cmp 8
		bcc R816_LShr64_R3
		tay
		lda [D + X]
		xba
		and 0xFF
		sta [D + X]
		tya
		and 7
		bne R816_LShr64_R3
		rtl
R816_LShr64_R3:
		tay
		lda [D + X]
R816_LShr64_L3:
		lsr A
		dey
		bne R816_LShr64_L3
		sta [D + X]
		rtl
R816_LShr64_Q4:
		stz [D + X]
		stz [D + 2 + X]
		stz [D + 4 + X]
		stz [D + 6 + X]
		rtl


;
		.section text.R816_AShr64
		.global R816_AShr64
R816_AShr64:
		.width (A, X), 16
		.relax
		and 63
		bne R816_AShr64_Some
		rtl
R816_AShr64_Some:
		tay
		lda [D + 6 + X]
		cmp 0x8000
		lda 0
		bcc R816_AShr64_Sign
		dec A
R816_AShr64_Sign:
		sta [D + 13]
		tya
		cmp 32
		bcs R816_AShr64_Q23
		cmp 16
		bcs R816_AShr64_Q1
		cmp 8
		bcc R816_AShr64_R0
		tay
		lda [D + 1 + X]
		sta [D + X]
		lda [D + 3 + X]
		sta [D + 2 + X]
		lda [D + 5 + X]
		sta [D + 4 + X]
		lda [D + 6 + X]
		xba
		and 0xFF
		eor 0x80
		sec
		sbc 0x80
		sta [D + 6 + X]
		tya
		and 7
		bne R816_AShr64_R0
		rtl
R816_AShr64_R0:
		tay
		lda [D + 6 + X]
R816_AShr64_L0:
		cmp 0x8000
		ror A
		ror [D + 4 + X]
		ror [D + 2 + X]
		ror [D + X]
		dey
		bne R816_AShr64_L0
		sta [D + 6 + X]
		rtl
R816_AShr64_Q1:
		tay
		lda [D + 2 + X]
		sta [D + X]
		lda [D + 4 + X]
		sta [D + 2 + X]
		lda [D + 6 + X]
		sta [D + 4 + X]
		lda [D + 13]
		sta [D + 6 + X]
		tya
		and 15
		bne R816_AShr64_B1
		rtl
R816_AShr64_B1:
		cmp 8
		bcc R816_AShr64_R1
		tay
		lda [D + 1 + X]
		sta [D + X]
		lda [D + 3 + X]
		sta [D + 2 + X]
		lda [D + 4 + X]
		xba
		and 0xFF
		eor 0x80
		sec
		sbc 0x80
		sta [D + 4 + X]
		tya
		and 7
		bne R816_AShr64_R1
		rtl
R816_AShr64_R1:
		tay
		lda [D + 4 + X]
R816_AShr64_L1:
		cmp 0x8000
		ror A
		ror [D + 2 + X]
		ror [D + X]
		dey
		bne R816_AShr64_L1
		sta [D + 4 + X]
		rtl
R816_AShr64_Q23:
		cmp 48
		bcs R816_AShr64_Q3
R816_AShr64_Q2:
		tay
		lda [D + 4 + X]
		sta [D + X]
		lda [D + 6 + X]
		sta [D + 2 + X]
		lda [D + 13]
		sta [D + 4 + X]
		sta [D + 6 + X]
		tya
		and 15
		bne R816_AShr64_B2
		rtl
R816_AShr64_B2:
		cmp 8
		bcc R816_AShr64_R2
		tay
		lda [D + 1 + X]
		sta [D + X]
		lda [D + 2 + X]
		xba
		and 0xFF
		eor 0x80
		sec
		sbc 0x80
		sta [D + 2 + X]
		tya
		and 7
		bne R816_AShr64_R2
		rtl
R816_AShr64_R2:
		tay
		lda [D + 2 + X]
R816_AShr64_L2:
		cmp 0x8000
		ror A
		ror [D + X]
		dey
		bne R816_AShr64_L2
		sta [D + 2 + X]
		rtl
R816_AShr64_Q3:
		tay
		lda [D + 6 + X]
		sta [D + X]
		lda [D + 13]
		sta [D + 2 + X]
		sta [D + 4 + X]
		sta [D + 6 + X]
		tya
		and 15
		bne R816_AShr64_B3
		rtl
R816_AShr64_B3:
		cmp 8
		bcc R816_AShr64_R3
		tay
		lda [D + X]
		xba
		and 0xFF
		eor 0x80
		sec
		sbc 0x80
		sta [D + X]
		tya
		and 7
		bne R816_AShr64_R3
		rtl
R816_AShr64_R3:
		tay
		lda [D + X]
R816_AShr64_L3:
		cmp 0x8000
		ror A
		dey
		bne R816_AShr64_L3
		sta [D + X]
		rtl
R816_AShr64_Q4:
		lda [D + 13]
		sta [D + X]
		sta [D + 2 + X]
		sta [D + 4 + X]
		sta [D + 6 + X]
		rtl


;
		.section text.R816_Shl32
		.global R816_Shl32
R816_Shl32:
		.width (A, X), 16
		.relax
		and 63
		bne R816_Shl32_Some
		brl R816_Shl32_Done
R816_Shl32_Some:
		cmp 32
		bcs R816_Shl32_Q2
		cmp 16
		bcs R816_Shl32_Q1
		cmp 8
		bcc R816_Shl32_R0
		tay
		lda [D + 16]
		sta [D + 17]
		lda [D + 15]
		xba
		and 0xFF00
		sta [D + 15]
		tya
		and 7
		bne R816_Shl32_R0
		brl R816_Shl32_Done
R816_Shl32_R0:
		asl A
		tax
		lda [D + 15]
		jmp [R816_Shl32_T0 + X]
R816_Shl32_T0:
		.word R816_Shl32_E0 & 0xFFFF
		.word R816_Shl32_S0_6 & 0xFFFF
		.word R816_Shl32_S0_5 & 0xFFFF
		.word R816_Shl32_S0_4 & 0xFFFF
		.word R816_Shl32_S0_3 & 0xFFFF
		.word R816_Shl32_S0_2 & 0xFFFF
		.word R816_Shl32_S0_1 & 0xFFFF
		.word R816_Shl32_S0_0 & 0xFFFF
R816_Shl32_S0_0:
		asl A
		rol [D + 17]
R816_Shl32_S0_1:
		asl A
		rol [D + 17]
R816_Shl32_S0_2:
		asl A
		rol [D + 17]
R816_Shl32_S0_3:
		asl A
		rol [D + 17]
R816_Shl32_S0_4:
		asl A
		rol [D + 17]
R816_Shl32_S0_5:
		asl A
		rol [D + 17]
R816_Shl32_S0_6:
		asl A
		rol [D + 17]
R816_Shl32_E0:
		sta [D + 15]
		brl R816_Shl32_Done
R816_Shl32_Q1:
		tay
		lda [D + 15]
		sta [D + 17]
		stz [D + 15]
		tya
		and 15
		bne R816_Shl32_B1
		brl R816_Shl32_Done
R816_Shl32_B1:
		cmp 8
		bcc R816_Shl32_R1
		tay
		lda [D + 17]
		xba
		and 0xFF00
		sta [D + 17]
		tya
		and 7
		bne R816_Shl32_R1
		brl R816_Shl32_Done
R816_Shl32_R1:
		asl A
		tax
		lda [D + 17]
		jmp [R816_Shl32_T1 + X]
R816_Shl32_T1:
		.word R816_Shl32_E1 & 0xFFFF
		.word R816_Shl32_S1_6 & 0xFFFF
		.word R816_Shl32_S1_5 & 0xFFFF
		.word R816_Shl32_S1_4 & 0xFFFF
		.word R816_Shl32_S1_3 & 0xFFFF
		.word R816_Shl32_S1_2 & 0xFFFF
		.word R816_Shl32_S1_1 & 0xFFFF
		.word R816_Shl32_S1_0 & 0xFFFF
R816_Shl32_S1_0:
		asl A
R816_Shl32_S1_1:
		asl A
R816_Shl32_S1_2:
		asl A
R816_Shl32_S1_3:
		asl A
R816_Shl32_S1_4:
		asl A
R816_Shl32_S1_5:
		asl A
R816_Shl32_S1_6:
		asl A
R816_Shl32_E1:
		sta [D + 17]
		brl R816_Shl32_Done
R816_Shl32_Q2:
		stz [D + 15]
		stz [D + 17]
		brl R816_Shl32_Done
R816_Shl32_Done:
		ldx [D + 17]
		lda [D + 15]
		rtl


;
		.section text.R816_LShr32
		.global R816_LShr32
R816_LShr32:
		.width (A, X), 16
		.relax
		and 63
		bne R816_LShr32_Some
		brl R816_LShr32_Done
R816_LShr32_Some:
		cmp 32
		bcs R816_LShr32_Q2
		cmp 16
		bcs R816_LShr32_Q1
		cmp 8
		bcc R816_LShr32_R0
		tay
		lda [D + 16]
		sta [D + 15]
		lda [D + 17]
		xba
		and 0xFF
		sta [D + 17]
		tya
		and 7
		bne R816_LShr32_R0
		brl R816_LShr32_Done
R816_LShr32_R0:
		asl A
		tax
		lda [D + 17]
		jmp [R816_LShr32_T0 + X]
R816_LShr32_T0:
		.word R816_LShr32_E0 & 0xFFFF
		.word R816_LShr32_S0_6 & 0xFFFF
		.word R816_LShr32_S0_5 & 0xFFFF
		.word R816_LShr32_S0_4 & 0xFFFF
		.word R816_LShr32_S0_3 & 0xFFFF
		.word R816_LShr32_S0_2 & 0xFFFF
		.word R816_LShr32_S0_1 & 0xFFFF
		.word R816_LShr32_S0_0 & 0xFFFF
R816_LShr32_S0_0:
		lsr A
		ror [D + 15]
R816_LShr32_S0_1:
		lsr A
		ror [D + 15]
R816_LShr32_S0_2:
		lsr A
		ror [D + 15]
R816_LShr32_S0_3:
		lsr A
		ror [D + 15]
R816_LShr32_S0_4:
		lsr A
		ror [D + 15]
R816_LShr32_S0_5:
		lsr A
		ror [D + 15]
R816_LShr32_S0_6:
		lsr A
		ror [D + 15]
R816_LShr32_E0:
		sta [D + 17]
		brl R816_LShr32_Done
R816_LShr32_Q1:
		tay
		lda [D + 17]
		sta [D + 15]
		stz [D + 17]
		tya
		and 15
		bne R816_LShr32_B1
		brl R816_LShr32_Done
R816_LShr32_B1:
		cmp 8
		bcc R816_LShr32_R1
		tay
		lda [D + 15]
		xba
		and 0xFF
		sta [D + 15]
		tya
		and 7
		bne R816_LShr32_R1
		brl R816_LShr32_Done
R816_LShr32_R1:
		asl A
		tax
		lda [D + 15]
		jmp [R816_LShr32_T1 + X]
R816_LShr32_T1:
		.word R816_LShr32_E1 & 0xFFFF
		.word R816_LShr32_S1_6 & 0xFFFF
		.word R816_LShr32_S1_5 & 0xFFFF
		.word R816_LShr32_S1_4 & 0xFFFF
		.word R816_LShr32_S1_3 & 0xFFFF
		.word R816_LShr32_S1_2 & 0xFFFF
		.word R816_LShr32_S1_1 & 0xFFFF
		.word R816_LShr32_S1_0 & 0xFFFF
R816_LShr32_S1_0:
		lsr A
R816_LShr32_S1_1:
		lsr A
R816_LShr32_S1_2:
		lsr A
R816_LShr32_S1_3:
		lsr A
R816_LShr32_S1_4:
		lsr A
R816_LShr32_S1_5:
		lsr A
R816_LShr32_S1_6:
		lsr A
R816_LShr32_E1:
		sta [D + 15]
		brl R816_LShr32_Done
R816_LShr32_Q2:
		stz [D + 15]
		stz [D + 17]
		brl R816_LShr32_Done
R816_LShr32_Done:
		ldx [D + 17]
		lda [D + 15]
		rtl


;
		.section text.R816_AShr32
		.global R816_AShr32
R816_AShr32:
		.width (A, X), 16
		.relax
		and 63
		bne R816_AShr32_Some
		brl R816_AShr32_Done
R816_AShr32_Some:
		tay
		lda [D + 17]
		cmp 0x8000
		lda 0
		bcc R816_AShr32_Sign
		dec A
R816_AShr32_Sign:
		sta [D + 13]
		tya
		cmp 32
		bcs R816_AShr32_Q2
		cmp 16
		bcs R816_AShr32_Q1
		cmp 8
		bcc R816_AShr32_R0
		tay
		lda [D + 16]
		sta [D + 15]
		lda [D + 17]
		xba
		and 0xFF
		eor 0x80
		sec
		sbc 0x80
		sta [D + 17]
		tya
		and 7
		bne R816_AShr32_R0
		brl R816_AShr32_Done
R816_AShr32_R0:
		asl A
		tax
		lda [D + 17]
		jmp [R816_AShr32_T0 + X]
R816_AShr32_T0:
		.word R816_AShr32_E0 & 0xFFFF
		.word R816_AShr32_S0_6 & 0xFFFF
		.word R816_AShr32_S0_5 & 0xFFFF
		.word R816_AShr32_S0_4 & 0xFFFF
		.word R816_AShr32_S0_3 & 0xFFFF
		.word R816_AShr32_S0_2 & 0xFFFF
		.word R816_AShr32_S0_1 & 0xFFFF
		.word R816_AShr32_S0_0 & 0xFFFF
R816_AShr32_S0_0:
		cmp 0x8000
		ror A
		ror [D + 15]
R816_AShr32_S0_1:
		cmp 0x8000
		ror A
		ror [D + 15]
R816_AShr32_S0_2:
		cmp 0x8000
		ror A
		ror [D + 15]
R816_AShr32_S0_3:
		cmp 0x8000
		ror A
		ror [D + 15]
R816_AShr32_S0_4:
		cmp 0x8000
		ror A
		ror [D + 15]
R816_AShr32_S0_5:
		cmp 0x8000
		ror A
		ror [D + 15]
R816_AShr32_S0_6:
		cmp 0x8000
		ror A
		ror [D + 15]
R816_AShr32_E0:
		sta [D + 17]
		brl R816_AShr32_Done
R816_AShr32_Q1:
		tay
		lda [D + 17]
		sta [D + 15]
		lda [D + 13]
		sta [D + 17]
		tya
		and 15
		bne R816_AShr32_B1
		brl R816_AShr32_Done
R816_AShr32_B1:
		cmp 8
		bcc R816_AShr32_R1
		tay
		lda [D + 15]
		xba
		and 0xFF
		eor 0x80
		sec
		sbc 0x80
		sta [D + 15]
		tya
		and 7
		bne R816_AShr32_R1
		brl R816_AShr32_Done
R816_AShr32_R1:
		asl A
		tax
		lda [D + 15]
		jmp [R816_AShr32_T1 + X]
R816_AShr32_T1:
		.word R816_AShr32_E1 & 0xFFFF
		.word R816_AShr32_S1_6 & 0xFFFF
		.word R816_AShr32_S1_5 & 0xFFFF
		.word R816_AShr32_S1_4 & 0xFFFF
		.word R816_AShr32_S1_3 & 0xFFFF
		.word R816_AShr32_S1_2 & 0xFFFF
		.word R816_AShr32_S1_1 & 0xFFFF
		.word R816_AShr32_S1_0 & 0xFFFF
R816_AShr32_S1_0:
		cmp 0x8000
		ror A
R816_AShr32_S1_1:
		cmp 0x8000
		ror A
R816_AShr32_S1_2:
		cmp 0x8000
		ror A
R816_AShr32_S1_3:
		cmp 0x8000
		ror A
R816_AShr32_S1_4:
		cmp 0x8000
		ror A
R816_AShr32_S1_5:
		cmp 0x8000
		ror A
R816_AShr32_S1_6:
		cmp 0x8000
		ror A
R816_AShr32_E1:
		sta [D + 15]
		brl R816_AShr32_Done
R816_AShr32_Q2:
		lda [D + 13]
		sta [D + 15]
		sta [D + 17]
		brl R816_AShr32_Done
R816_AShr32_Done:
		ldx [D + 17]
		lda [D + 15]
		rtl


; uint16 __builtin_bcd_add(uint16* pauiDest, uint16* pauiSource, uint16 uiWords, uint16 uiCarry) and
; __builtin_bcd_subtract (BaseLib's BCDAdd and BCDSubtract), with the compiler's calling convention: D is pointed at the
; stack, so the destination is at D + 6, the source at D + 10, the words at D + 14 and the carry (or borrow) at D + 16.
; The carry in goes to C (a borrow is C clear), then each word is added (or subtracted) in decimal mode, the least
; significant first, and the carry out is returned.
		.section text.R816_BcdAdd
		.global R816_BcdAdd
R816_BcdAdd:
		.width (A, X), 16
		phd
		tsc
		tcd
		lda [D + 16]
		lsr A
		ldy 0
		ldx [D + 14]
		beq R816_BcdAdd_Done
		sed
R816_BcdAdd_Loop:
		lda [[D + 6]:24 + Y]
		adc [[D + 10]:24 + Y]
		sta [[D + 6]:24 + Y]
		iny
		iny
		dex
		bne R816_BcdAdd_Loop
		cld
R816_BcdAdd_Done:
		lda 0
		rol A
		pld
		rtl


;
		.section text.R816_BcdSubtract
		.global R816_BcdSubtract
R816_BcdSubtract:
		.width (A, X), 16
		phd
		tsc
		tcd
		lda [D + 16]
		eor 1
		lsr A
		ldy 0
		ldx [D + 14]
		beq R816_BcdSubtract_Done
		sed
R816_BcdSubtract_Loop:
		lda [[D + 6]:24 + Y]
		sbc [[D + 10]:24 + Y]
		sta [[D + 6]:24 + Y]
		iny
		iny
		dex
		bne R816_BcdSubtract_Loop
		cld
R816_BcdSubtract_Done:
		lda 0
		rol A
		eor 1
		pld
		rtl


; R816_UDiv16 and R816_URem16: the 16 bit divide n / d by shift and subtract, n (D + 15) shifting out of its top into
; the remainder (A) as the quotient's bits shift into its bottom.  d = 0 is q = $FFFF, r = n; d >= $8000 is q = 1 or 0
; (so the remainder never overflows A); n < d is q = 0.  When n's high byte is below d the quotient is a byte: the
; remainder starts as nH and nL is 8 steps (nL put in the top of D + 15); otherwise it is 16 steps.  A d under 128 runs
; the steps 8 bits wide (the remainder fits a byte; a byte rotates in 5 cycles, a word in 7), a byte of n at a time.
; The quotient is left at D + 15 and X is 0 at the end, so the 32 bit divide can return through them.  R816_UDiv16 also
; leaves the remainder at D + 5, where a urem of the same operands written straight after it takes it
; (CIR816Backend::RemainderLeft).
		.section text.R816_UDiv16
		.global R816_UDiv16
		.global R816_URem16
		.width (A, X), 16
		.relax
R816_UDiv16:
		jsr R816_UDiv16_Divide
		sta [D + 5]
		lda [D + 15]
		ldx 0
		rtl
R816_URem16:
		jsr R816_UDiv16_Divide
		ldx 0
		rtl
R816_UDiv16_Divide:
		lda [D + 19]
		beq R816_UDiv16_Zero
		bmi R816_UDiv16_Big
		cmp 0x80
		bcc R816_UDiv16_Small
		lda [D + 15]
		cmp [D + 19]
		bcc R816_UDiv16_Less
		xba
		and 0xFF
		cmp [D + 19]
		bcs R816_UDiv16_Word
		tax
		lda [D + 14]
		and 0xFF00
		sta [D + 15]
		txa
		asl [D + 15]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 15]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 15]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 15]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 15]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 15]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 15]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 15]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 15]
		rts
R816_UDiv16_Word:
		lda 0
		asl [D + 15]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 15]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 15]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 15]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 15]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 15]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 15]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 15]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 15]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 15]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 15]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 15]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 15]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 15]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 15]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 15]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 15]
		rts
R816_UDiv16_Small:
		lda [D + 15]
		cmp [D + 19]
		bcc R816_UDiv16_Less
		sep m
		lda [D + 16]
		cmp [D + 19]
		bcs R816_UDiv16_SmallWord
		stz [D + 16]
		bra R816_UDiv16_SmallLow
R816_UDiv16_SmallWord:
		lda 0
		asl [D + 16]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 16]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 16]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 16]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 16]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 16]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 16]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 16]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 16]
R816_UDiv16_SmallLow:
		asl [D + 15]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 15]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 15]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 15]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 15]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 15]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 15]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 15]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 15]
		rep m
		and 0xFF
		rts
R816_UDiv16_Big:
		lda [D + 15]
		cmp [D + 19]
		bcc R816_UDiv16_Less
		sbc [D + 19]
		ldx 1
		stx [D + 15]
		rts
R816_UDiv16_Less:
		stz [D + 15]
		rts
R816_UDiv16_Zero:
		lda [D + 15]
		ldx 0xFFFF
		stx [D + 15]
		rts


; R816_SDiv16, R816_SRem16, R816_SDiv32 and R816_SRem32: the signed divides (the quotient and the remainder share one
; body for each width), scratch routines as the unsigned ones are: a at D + 15 and b at D + 19.  a's sign word goes to
; D + 11 and the quotient's (a's xor b's) to D + 9, a and b are made their magnitudes in place, and R816_UDiv16 or
; R816_UDiv32 (on the same D) leaves the magnitudes' quotient at D + 15 (and D + 17) and remainder at D + 5 (and D +
; 7).  The quotient is negated when the signs differ and the remainder when a is negative (C rounds towards 0: the
; remainder has the dividend's sign).  Both routines leave the signed remainder at D + 5, where a srem of the same
; operands written straight after them takes it (CIR816Backend::RemainderLeft).  The most negative value divided by -1
; wraps, as the magnitudes do.
		.section text.R816_SDiv16
		.global R816_SDiv16
		.global R816_SRem16
		.width (A, X), 16
		.relax
R816_SDiv16:
		jsr R816_SDiv16_Divide
		lda [D + 15]
		ldx 0
		rtl
R816_SRem16:
		jsr R816_SDiv16_Divide
		lda [D + 5]
		ldx 0
		rtl
R816_SDiv16_Divide:
		lda [D + 15]
		sta [D + 11]
		eor [D + 19]
		sta [D + 9]
		lda [D + 15]
		bpl R816_SDiv16_APositive
		lda 0
		sec
		sbc [D + 15]
		sta [D + 15]
R816_SDiv16_APositive:
		lda [D + 19]
		bpl R816_SDiv16_BPositive
		lda 0
		sec
		sbc [D + 19]
		sta [D + 19]
R816_SDiv16_BPositive:
		jsl R816_UDiv16
		lda [D + 9]
		bpl R816_SDiv16_QPositive
		lda 0
		sec
		sbc [D + 15]
		sta [D + 15]
R816_SDiv16_QPositive:
		lda [D + 11]
		bpl R816_SDiv16_RPositive
		lda 0
		sec
		sbc [D + 5]
		sta [D + 5]
R816_SDiv16_RPositive:
		rts


		.section text.R816_SDiv32
		.global R816_SDiv32
		.global R816_SRem32
		.width (A, X), 16
		.relax
R816_SDiv32:
		jsr R816_SDiv32_Divide
		ldx [D + 17]
		lda [D + 15]
		rtl
R816_SRem32:
		jsr R816_SDiv32_Divide
		ldx [D + 7]
		lda [D + 5]
		rtl
R816_SDiv32_Divide:
		lda [D + 17]
		sta [D + 11]
		eor [D + 21]
		sta [D + 9]
		lda [D + 17]
		bpl R816_SDiv32_APositive
		lda 0
		sec
		sbc [D + 15]
		sta [D + 15]
		lda 0
		sbc [D + 17]
		sta [D + 17]
R816_SDiv32_APositive:
		lda [D + 21]
		bpl R816_SDiv32_BPositive
		lda 0
		sec
		sbc [D + 19]
		sta [D + 19]
		lda 0
		sbc [D + 21]
		sta [D + 21]
R816_SDiv32_BPositive:
		jsl R816_UDiv32
		lda [D + 9]
		bpl R816_SDiv32_QPositive
		lda 0
		sec
		sbc [D + 15]
		sta [D + 15]
		lda 0
		sbc [D + 17]
		sta [D + 17]
R816_SDiv32_QPositive:
		lda [D + 11]
		bpl R816_SDiv32_RPositive
		lda 0
		sec
		sbc [D + 5]
		sta [D + 5]
		lda 0
		sbc [D + 7]
		sta [D + 7]
R816_SDiv32_RPositive:
		rts


; R816_UDiv32 and R816_URem32 (one body, R816_Div32_Divide, for both): the dividend is at D + 15 (and becomes the
; quotient) and the divisor at D + 19, and D + 5 is the remainder.  With both under 65536 it is the 16 bit one, whose
; operands are already in place (their low words), jumped to (above); it returns the 32 bit result (X is 0).  A divisor
; under 65536 keeps the remainder in A: 16 (or 8) unrolled steps on the dividend's low word, after 16 on its high word
; when that is not below the divisor (else the quotient's high word is 0 and the high word is the remainder to start).
; Its steps are as R816_UDiv16's: 8 bits wide a byte at a time for a divisor under 128 (a top byte below it is the
; remainder to start), without the overflow guard (bcs, sec) for one under $8000, with it otherwise.  A dividend under
; 65536 with a wider divisor is a quotient of 0.  Otherwise (both wide) the quotient is under 65536: its high word is 0,
; the dividend's high word is the remainder to start (or, when the dividend >> 8 is below the divisor, the dividend >> 8
; with 8 steps for a byte's quotient), and unrolled steps on the remainder at D + 5 and D + 7 take the dividend's low
; word, the quotient bit (C) rotated into it as its next bit comes out; a divisor of 2^31 or more (whose remainder can
; overflow) loops with the guard.  Every path leaves the remainder at D + 5 and D + 7 (the short one clears D + 7 before
; R816_UDiv16 writes D + 5), for a urem straight after R816_UDiv32 (CIR816Backend::RemainderLeft).
		.section text.R816_UDiv32
		.global R816_UDiv32
		.global R816_URem32
		.width (A, X), 16
		.relax
R816_UDiv32:
		lda [D + 17]
		ora [D + 21]
		bne R816_UDiv32_Wide
		stz [D + 7]
		jml R816_UDiv16
R816_UDiv32_Wide:
		jsr R816_Div32_Divide
		ldx [D + 17]
		lda [D + 15]
		rtl
R816_URem32:
		lda [D + 17]
		ora [D + 21]
		bne R816_URem32_Wide
		jml R816_URem16
R816_URem32_Wide:
		jsr R816_Div32_Divide
		ldx [D + 7]
		lda [D + 5]
		rtl
R816_Div32_Divide:
		stz [D + 7]
		lda [D + 21]
		bne R816_Div32_WideDivisor
		lda [D + 19]
		bmi R816_Div32_Slow
		cmp 0x80
		bcs R816_Div32_Fast
		sep m
		lda [D + 18]
		cmp [D + 19]
		bcs R816_Div32_Byte3
		stz [D + 18]
		bra R816_Div32_Byte2
R816_Div32_Byte3:
		lda 0
		asl [D + 18]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 18]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 18]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 18]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 18]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 18]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 18]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 18]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 18]
R816_Div32_Byte2:
		asl [D + 17]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 17]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 17]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 17]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 17]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 17]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 17]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 17]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 17]
		asl [D + 16]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 16]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 16]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 16]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 16]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 16]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 16]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 16]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 16]
		asl [D + 15]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 15]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 15]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 15]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 15]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 15]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 15]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 15]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 15]
		rep m
		and 0xFF
		sta [D + 5]
		rts
R816_Div32_Fast:
		lda [D + 17]
		cmp [D + 19]
		bcc R816_Div32_FastLowWord
		lda [D + 18]
		and 0xFF
		cmp [D + 19]
		bcs R816_Div32_FastHighWord
		tax
		lda [D + 16]
		and 0xFF00
		sta [D + 17]
		txa
		asl [D + 17]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 17]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 17]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 17]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 17]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 17]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 17]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 17]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 17]
		bra R816_Div32_FastLow
R816_Div32_FastHighWord:
		lda 0
		asl [D + 17]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 17]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 17]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 17]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 17]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 17]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 17]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 17]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 17]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 17]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 17]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 17]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 17]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 17]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 17]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 17]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 17]
		bra R816_Div32_FastLow
R816_Div32_FastLowWord:
		stz [D + 17]
R816_Div32_FastLow:
		asl [D + 15]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 15]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 15]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 15]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 15]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 15]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 15]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 15]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 15]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 15]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 15]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 15]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 15]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 15]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 15]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 15]
		rol A
		cmp [D + 19]
		bcc C + 2
		sbc [D + 19]
		rol [D + 15]
		sta [D + 5]
		rts
R816_Div32_Slow:
		lda [D + 17]
		cmp [D + 19]
		bcc R816_Div32_SlowLowWord
		lda [D + 18]
		and 0xFF
		cmp [D + 19]
		bcs R816_Div32_SlowHighWord
		tax
		lda [D + 16]
		and 0xFF00
		sta [D + 17]
		txa
		asl [D + 17]
		rol A
		bcs C + 4
		cmp [D + 19]
		bcc C + 3
		sbc [D + 19]
		sec
		rol [D + 17]
		rol A
		bcs C + 4
		cmp [D + 19]
		bcc C + 3
		sbc [D + 19]
		sec
		rol [D + 17]
		rol A
		bcs C + 4
		cmp [D + 19]
		bcc C + 3
		sbc [D + 19]
		sec
		rol [D + 17]
		rol A
		bcs C + 4
		cmp [D + 19]
		bcc C + 3
		sbc [D + 19]
		sec
		rol [D + 17]
		rol A
		bcs C + 4
		cmp [D + 19]
		bcc C + 3
		sbc [D + 19]
		sec
		rol [D + 17]
		rol A
		bcs C + 4
		cmp [D + 19]
		bcc C + 3
		sbc [D + 19]
		sec
		rol [D + 17]
		rol A
		bcs C + 4
		cmp [D + 19]
		bcc C + 3
		sbc [D + 19]
		sec
		rol [D + 17]
		rol A
		bcs C + 4
		cmp [D + 19]
		bcc C + 3
		sbc [D + 19]
		sec
		rol [D + 17]
		bra R816_Div32_SlowLow
R816_Div32_SlowHighWord:
		lda 0
		asl [D + 17]
		rol A
		bcs C + 4
		cmp [D + 19]
		bcc C + 3
		sbc [D + 19]
		sec
		rol [D + 17]
		rol A
		bcs C + 4
		cmp [D + 19]
		bcc C + 3
		sbc [D + 19]
		sec
		rol [D + 17]
		rol A
		bcs C + 4
		cmp [D + 19]
		bcc C + 3
		sbc [D + 19]
		sec
		rol [D + 17]
		rol A
		bcs C + 4
		cmp [D + 19]
		bcc C + 3
		sbc [D + 19]
		sec
		rol [D + 17]
		rol A
		bcs C + 4
		cmp [D + 19]
		bcc C + 3
		sbc [D + 19]
		sec
		rol [D + 17]
		rol A
		bcs C + 4
		cmp [D + 19]
		bcc C + 3
		sbc [D + 19]
		sec
		rol [D + 17]
		rol A
		bcs C + 4
		cmp [D + 19]
		bcc C + 3
		sbc [D + 19]
		sec
		rol [D + 17]
		rol A
		bcs C + 4
		cmp [D + 19]
		bcc C + 3
		sbc [D + 19]
		sec
		rol [D + 17]
		rol A
		bcs C + 4
		cmp [D + 19]
		bcc C + 3
		sbc [D + 19]
		sec
		rol [D + 17]
		rol A
		bcs C + 4
		cmp [D + 19]
		bcc C + 3
		sbc [D + 19]
		sec
		rol [D + 17]
		rol A
		bcs C + 4
		cmp [D + 19]
		bcc C + 3
		sbc [D + 19]
		sec
		rol [D + 17]
		rol A
		bcs C + 4
		cmp [D + 19]
		bcc C + 3
		sbc [D + 19]
		sec
		rol [D + 17]
		rol A
		bcs C + 4
		cmp [D + 19]
		bcc C + 3
		sbc [D + 19]
		sec
		rol [D + 17]
		rol A
		bcs C + 4
		cmp [D + 19]
		bcc C + 3
		sbc [D + 19]
		sec
		rol [D + 17]
		rol A
		bcs C + 4
		cmp [D + 19]
		bcc C + 3
		sbc [D + 19]
		sec
		rol [D + 17]
		rol A
		bcs C + 4
		cmp [D + 19]
		bcc C + 3
		sbc [D + 19]
		sec
		rol [D + 17]
		bra R816_Div32_SlowLow
R816_Div32_SlowLowWord:
		stz [D + 17]
R816_Div32_SlowLow:
		asl [D + 15]
		rol A
		bcs C + 4
		cmp [D + 19]
		bcc C + 3
		sbc [D + 19]
		sec
		rol [D + 15]
		rol A
		bcs C + 4
		cmp [D + 19]
		bcc C + 3
		sbc [D + 19]
		sec
		rol [D + 15]
		rol A
		bcs C + 4
		cmp [D + 19]
		bcc C + 3
		sbc [D + 19]
		sec
		rol [D + 15]
		rol A
		bcs C + 4
		cmp [D + 19]
		bcc C + 3
		sbc [D + 19]
		sec
		rol [D + 15]
		rol A
		bcs C + 4
		cmp [D + 19]
		bcc C + 3
		sbc [D + 19]
		sec
		rol [D + 15]
		rol A
		bcs C + 4
		cmp [D + 19]
		bcc C + 3
		sbc [D + 19]
		sec
		rol [D + 15]
		rol A
		bcs C + 4
		cmp [D + 19]
		bcc C + 3
		sbc [D + 19]
		sec
		rol [D + 15]
		rol A
		bcs C + 4
		cmp [D + 19]
		bcc C + 3
		sbc [D + 19]
		sec
		rol [D + 15]
		rol A
		bcs C + 4
		cmp [D + 19]
		bcc C + 3
		sbc [D + 19]
		sec
		rol [D + 15]
		rol A
		bcs C + 4
		cmp [D + 19]
		bcc C + 3
		sbc [D + 19]
		sec
		rol [D + 15]
		rol A
		bcs C + 4
		cmp [D + 19]
		bcc C + 3
		sbc [D + 19]
		sec
		rol [D + 15]
		rol A
		bcs C + 4
		cmp [D + 19]
		bcc C + 3
		sbc [D + 19]
		sec
		rol [D + 15]
		rol A
		bcs C + 4
		cmp [D + 19]
		bcc C + 3
		sbc [D + 19]
		sec
		rol [D + 15]
		rol A
		bcs C + 4
		cmp [D + 19]
		bcc C + 3
		sbc [D + 19]
		sec
		rol [D + 15]
		rol A
		bcs C + 4
		cmp [D + 19]
		bcc C + 3
		sbc [D + 19]
		sec
		rol [D + 15]
		rol A
		bcs C + 4
		cmp [D + 19]
		bcc C + 3
		sbc [D + 19]
		sec
		rol [D + 15]
		sta [D + 5]
		rts
R816_Div32_WideDivisor:
		lda [D + 17]
		bne R816_Div32_Long
		lda [D + 15]
		sta [D + 5]
		stz [D + 15]
		rts
R816_Div32_Long:
		lda [D + 21]
		bmi R816_Div32_Loop
		lda [D + 18]
		and 0xFF
		cmp [D + 21]
		bcc R816_Div32_Short
		bne R816_Div32_Full
		lda [D + 16]
		cmp [D + 19]
		bcs R816_Div32_Full
R816_Div32_Short:
		lda [D + 16]
		sta [D + 5]
		lda [D + 18]
		and 0xFF
		sta [D + 7]
		lda [D + 14]
		and 0xFF00
		sta [D + 15]
		stz [D + 17]
		asl [D + 15]
		bra R816_Div32_Step8
R816_Div32_Full:
		lda [D + 17]
		sta [D + 5]
		stz [D + 17]
		asl [D + 15]
		rol [D + 5]
		rol [D + 7]
		sec
		lda [D + 5]
		sbc [D + 19]
		tay
		lda [D + 7]
		sbc [D + 21]
		bcc R816_Div32_Keep0
		sta [D + 7]
		sty [D + 5]
R816_Div32_Keep0:
		rol [D + 15]
		rol [D + 5]
		rol [D + 7]
		sec
		lda [D + 5]
		sbc [D + 19]
		tay
		lda [D + 7]
		sbc [D + 21]
		bcc R816_Div32_Keep1
		sta [D + 7]
		sty [D + 5]
R816_Div32_Keep1:
		rol [D + 15]
		rol [D + 5]
		rol [D + 7]
		sec
		lda [D + 5]
		sbc [D + 19]
		tay
		lda [D + 7]
		sbc [D + 21]
		bcc R816_Div32_Keep2
		sta [D + 7]
		sty [D + 5]
R816_Div32_Keep2:
		rol [D + 15]
		rol [D + 5]
		rol [D + 7]
		sec
		lda [D + 5]
		sbc [D + 19]
		tay
		lda [D + 7]
		sbc [D + 21]
		bcc R816_Div32_Keep3
		sta [D + 7]
		sty [D + 5]
R816_Div32_Keep3:
		rol [D + 15]
		rol [D + 5]
		rol [D + 7]
		sec
		lda [D + 5]
		sbc [D + 19]
		tay
		lda [D + 7]
		sbc [D + 21]
		bcc R816_Div32_Keep4
		sta [D + 7]
		sty [D + 5]
R816_Div32_Keep4:
		rol [D + 15]
		rol [D + 5]
		rol [D + 7]
		sec
		lda [D + 5]
		sbc [D + 19]
		tay
		lda [D + 7]
		sbc [D + 21]
		bcc R816_Div32_Keep5
		sta [D + 7]
		sty [D + 5]
R816_Div32_Keep5:
		rol [D + 15]
		rol [D + 5]
		rol [D + 7]
		sec
		lda [D + 5]
		sbc [D + 19]
		tay
		lda [D + 7]
		sbc [D + 21]
		bcc R816_Div32_Keep6
		sta [D + 7]
		sty [D + 5]
R816_Div32_Keep6:
		rol [D + 15]
		rol [D + 5]
		rol [D + 7]
		sec
		lda [D + 5]
		sbc [D + 19]
		tay
		lda [D + 7]
		sbc [D + 21]
		bcc R816_Div32_Keep7
		sta [D + 7]
		sty [D + 5]
R816_Div32_Keep7:
		rol [D + 15]
R816_Div32_Step8:
		rol [D + 5]
		rol [D + 7]
		sec
		lda [D + 5]
		sbc [D + 19]
		tay
		lda [D + 7]
		sbc [D + 21]
		bcc R816_Div32_Keep8
		sta [D + 7]
		sty [D + 5]
R816_Div32_Keep8:
		rol [D + 15]
		rol [D + 5]
		rol [D + 7]
		sec
		lda [D + 5]
		sbc [D + 19]
		tay
		lda [D + 7]
		sbc [D + 21]
		bcc R816_Div32_Keep9
		sta [D + 7]
		sty [D + 5]
R816_Div32_Keep9:
		rol [D + 15]
		rol [D + 5]
		rol [D + 7]
		sec
		lda [D + 5]
		sbc [D + 19]
		tay
		lda [D + 7]
		sbc [D + 21]
		bcc R816_Div32_Keep10
		sta [D + 7]
		sty [D + 5]
R816_Div32_Keep10:
		rol [D + 15]
		rol [D + 5]
		rol [D + 7]
		sec
		lda [D + 5]
		sbc [D + 19]
		tay
		lda [D + 7]
		sbc [D + 21]
		bcc R816_Div32_Keep11
		sta [D + 7]
		sty [D + 5]
R816_Div32_Keep11:
		rol [D + 15]
		rol [D + 5]
		rol [D + 7]
		sec
		lda [D + 5]
		sbc [D + 19]
		tay
		lda [D + 7]
		sbc [D + 21]
		bcc R816_Div32_Keep12
		sta [D + 7]
		sty [D + 5]
R816_Div32_Keep12:
		rol [D + 15]
		rol [D + 5]
		rol [D + 7]
		sec
		lda [D + 5]
		sbc [D + 19]
		tay
		lda [D + 7]
		sbc [D + 21]
		bcc R816_Div32_Keep13
		sta [D + 7]
		sty [D + 5]
R816_Div32_Keep13:
		rol [D + 15]
		rol [D + 5]
		rol [D + 7]
		sec
		lda [D + 5]
		sbc [D + 19]
		tay
		lda [D + 7]
		sbc [D + 21]
		bcc R816_Div32_Keep14
		sta [D + 7]
		sty [D + 5]
R816_Div32_Keep14:
		rol [D + 15]
		rol [D + 5]
		rol [D + 7]
		sec
		lda [D + 5]
		sbc [D + 19]
		tay
		lda [D + 7]
		sbc [D + 21]
		bcc R816_Div32_Keep15
		sta [D + 7]
		sty [D + 5]
R816_Div32_Keep15:
		rol [D + 15]
		rts
R816_Div32_Loop:
		lda [D + 17]
		sta [D + 5]
		stz [D + 17]
		ldx 16
		asl [D + 15]
R816_Div32_Bit:
		rol [D + 5]
		rol [D + 7]
		bcs R816_Div32_Over
		sec
		lda [D + 5]
		sbc [D + 19]
		tay
		lda [D + 7]
		sbc [D + 21]
		bcc R816_Div32_Next
		bra R816_Div32_Subtracted
R816_Div32_Over:
		sec
		lda [D + 5]
		sbc [D + 19]
		tay
		lda [D + 7]
		sbc [D + 21]
		sec
R816_Div32_Subtracted:
		sta [D + 7]
		sty [D + 5]
R816_Div32_Next:
		rol [D + 15]
		dex
		bne R816_Div32_Bit
		rts


; R816_Clz16, R816_Clz32, R816_Ctz16 and R816_Ctz32: __builtin_clz and __builtin_ctz (of unsigned int and unsigned long,
; 16 and 32 bits on the W65C816), the compiler's convention (the value pushed: at S + 4 after the jsl, low word first),
; the count returned in A (the width for 0).  The word and then the byte the count starts in are found by tests, and the
; byte's leading or trailing zeros read from a 256 byte table (R816_Clz8, R816_Ctz8; 8 for 0) with a long,X load.
		.section rodata.R816_Clz8
		.global R816_Clz8
R816_Clz8:
		.byte 8, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4
		.byte 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3
		.byte 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2
		.byte 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2
		.byte 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
		.byte 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
		.byte 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
		.byte 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
		.byte 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
		.byte 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
		.byte 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
		.byte 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
		.byte 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
		.byte 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
		.byte 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
		.byte 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0

		.section rodata.R816_Ctz8
		.global R816_Ctz8
R816_Ctz8:
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

		.section text.R816_Clz16
		.global R816_Clz16
		.global R816_Clz32
		.width (A, X), 16
R816_Clz16:
		lda [S + 4]
		beq R816_Clz16_Zero
		cmp 0x100
		bcc R816_Clz16_Low
		xba
		and 0xFF
		tax
		lda [R816_Clz8 + X]:24
		and 0xFF
		rtl
R816_Clz16_Low:
		tax
		lda [R816_Clz8 + X]:24
		and 0xFF
		clc
		adc 8
		rtl
R816_Clz16_Zero:
		lda 16
		rtl
R816_Clz32:
		lda [S + 6]
		beq R816_Clz32_LowWord
		cmp 0x100
		bcc R816_Clz32_HighLow
		xba
		and 0xFF
		tax
		lda [R816_Clz8 + X]:24
		and 0xFF
		rtl
R816_Clz32_HighLow:
		tax
		lda [R816_Clz8 + X]:24
		and 0xFF
		clc
		adc 8
		rtl
R816_Clz32_LowWord:
		lda [S + 4]
		beq R816_Clz32_Zero
		cmp 0x100
		bcc R816_Clz32_LowLow
		xba
		and 0xFF
		tax
		lda [R816_Clz8 + X]:24
		and 0xFF
		clc
		adc 16
		rtl
R816_Clz32_LowLow:
		tax
		lda [R816_Clz8 + X]:24
		and 0xFF
		clc
		adc 24
		rtl
R816_Clz32_Zero:
		lda 32
		rtl

		.section text.R816_Ctz16
		.global R816_Ctz16
		.global R816_Ctz32
		.width (A, X), 16
R816_Ctz16:
		lda [S + 4]
		beq R816_Ctz16_Zero
		and 0xFF
		beq R816_Ctz16_High
		tax
		lda [R816_Ctz8 + X]:24
		and 0xFF
		rtl
R816_Ctz16_High:
		lda [S + 4]
		xba
		and 0xFF
		tax
		lda [R816_Ctz8 + X]:24
		and 0xFF
		clc
		adc 8
		rtl
R816_Ctz16_Zero:
		lda 16
		rtl
R816_Ctz32:
		lda [S + 4]
		beq R816_Ctz32_HighWord
		and 0xFF
		beq R816_Ctz32_LowHigh
		tax
		lda [R816_Ctz8 + X]:24
		and 0xFF
		rtl
R816_Ctz32_LowHigh:
		lda [S + 4]
		xba
		and 0xFF
		tax
		lda [R816_Ctz8 + X]:24
		and 0xFF
		clc
		adc 8
		rtl
R816_Ctz32_HighWord:
		lda [S + 6]
		beq R816_Ctz32_Zero
		and 0xFF
		beq R816_Ctz32_HighHigh
		tax
		lda [R816_Ctz8 + X]:24
		and 0xFF
		clc
		adc 16
		rtl
R816_Ctz32_HighHigh:
		lda [S + 6]
		xba
		and 0xFF
		tax
		lda [R816_Ctz8 + X]:24
		and 0xFF
		clc
		adc 24
		rtl
R816_Ctz32_Zero:
		lda 32
		rtl
