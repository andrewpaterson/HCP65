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
; IR816Runtime-MathSafe.asm: the W65C816 back end's multiplies without Garth Wilson's maths tables
; (CCPP65Builder::SetMathTables(false)): for a program that can't count on the tables being in memory, such as the
; HCP65's kernal and its operating system (which loads them).  IR816Runtime-MathTables.asm has the same routines with
; MathMult; the others (the divides too: shift and subtract beats a reciprocal from MathInvert) are in
; IR816Runtime-Common.asm.
;
; They run on the caller's D (a scratch routine, IR816ScratchRoutine): a is at D + 15 and b at D + 19 (the scratch
; arguments), and they may use the caller's D + 5 to D + 22 (and read D + 3 to D + 24).  A 16 bit result is left in A,
; a 32 bit one in A (low) and X (high).  The loops are unrolled, and the steps a byte operand doesn't need are skipped.


; R816_Mul16: the low 16 bits of a * b, shift and add from the multiplier's top bit down (A = 2A, plus the multiplicand
; when the bit is set).  A byte multiplier (a, else b, under 256) is 8 steps: its byte is put in the top of the shifted
; word at D + 5 (the byte below it, read from D + 14 or D + 18, shifts out unseen).  Otherwise the product is
; aL * b + (aH * bL) << 8: aH * bL's low byte (8 steps in 8 bit mode, aH shifted in place) starts A, and the 8 steps of
; aL * b shift it out of the top as the 8 bits it needs.
		.section text.R816_Mul16
		.global R816_Mul16
R816_Mul16:
		.width (A, X), 16
		.relax
		lda [D + 15]
		cmp 0x100
		bcs R816_Mul16_AWide
		lda [D + 14]
		sta [D + 5]
		lda 0
		bra R816_Mul16_ByB
R816_Mul16_AWide:
		lda [D + 19]
		cmp 0x100
		bcs R816_Mul16_Wide
		lda [D + 18]
		sta [D + 5]
		lda 0
		asl [D + 5]
		bcc R816_Mul16_A_0
		clc
		adc [D + 15]
R816_Mul16_A_0:
		asl A
		asl [D + 5]
		bcc R816_Mul16_A_1
		clc
		adc [D + 15]
R816_Mul16_A_1:
		asl A
		asl [D + 5]
		bcc R816_Mul16_A_2
		clc
		adc [D + 15]
R816_Mul16_A_2:
		asl A
		asl [D + 5]
		bcc R816_Mul16_A_3
		clc
		adc [D + 15]
R816_Mul16_A_3:
		asl A
		asl [D + 5]
		bcc R816_Mul16_A_4
		clc
		adc [D + 15]
R816_Mul16_A_4:
		asl A
		asl [D + 5]
		bcc R816_Mul16_A_5
		clc
		adc [D + 15]
R816_Mul16_A_5:
		asl A
		asl [D + 5]
		bcc R816_Mul16_A_6
		clc
		adc [D + 15]
R816_Mul16_A_6:
		asl A
		asl [D + 5]
		bcc R816_Mul16_A_7
		clc
		adc [D + 15]
R816_Mul16_A_7:
		rtl
R816_Mul16_Wide:
		sep m
		lda 0
		asl [D + 16]
		bcc R816_Mul16_H_0
		clc
		adc [D + 19]
R816_Mul16_H_0:
		asl A
		asl [D + 16]
		bcc R816_Mul16_H_1
		clc
		adc [D + 19]
R816_Mul16_H_1:
		asl A
		asl [D + 16]
		bcc R816_Mul16_H_2
		clc
		adc [D + 19]
R816_Mul16_H_2:
		asl A
		asl [D + 16]
		bcc R816_Mul16_H_3
		clc
		adc [D + 19]
R816_Mul16_H_3:
		asl A
		asl [D + 16]
		bcc R816_Mul16_H_4
		clc
		adc [D + 19]
R816_Mul16_H_4:
		asl A
		asl [D + 16]
		bcc R816_Mul16_H_5
		clc
		adc [D + 19]
R816_Mul16_H_5:
		asl A
		asl [D + 16]
		bcc R816_Mul16_H_6
		clc
		adc [D + 19]
R816_Mul16_H_6:
		asl A
		asl [D + 16]
		bcc R816_Mul16_H_7
		clc
		adc [D + 19]
R816_Mul16_H_7:
		rep m
		ldx [D + 14]
		stx [D + 5]
		asl A
R816_Mul16_ByB:
		asl [D + 5]
		bcc R816_Mul16_B_0
		clc
		adc [D + 19]
R816_Mul16_B_0:
		asl A
		asl [D + 5]
		bcc R816_Mul16_B_1
		clc
		adc [D + 19]
R816_Mul16_B_1:
		asl A
		asl [D + 5]
		bcc R816_Mul16_B_2
		clc
		adc [D + 19]
R816_Mul16_B_2:
		asl A
		asl [D + 5]
		bcc R816_Mul16_B_3
		clc
		adc [D + 19]
R816_Mul16_B_3:
		asl A
		asl [D + 5]
		bcc R816_Mul16_B_4
		clc
		adc [D + 19]
R816_Mul16_B_4:
		asl A
		asl [D + 5]
		bcc R816_Mul16_B_5
		clc
		adc [D + 19]
R816_Mul16_B_5:
		asl A
		asl [D + 5]
		bcc R816_Mul16_B_6
		clc
		adc [D + 19]
R816_Mul16_B_6:
		asl A
		asl [D + 5]
		bcc R816_Mul16_B_7
		clc
		adc [D + 19]
R816_Mul16_B_7:
		rtl


; R816_UMul16: the 32 bit product of the 16 bit words at D + 13 (the multiplier) and D + 19 (the multiplicand), low in X
; and high in A, for R816_Mul32 and R816_UMulWide32 (D + 13 is changed).  Shift and add from the multiplier's bottom
; bit up: the sum rotates down through A (the high word) into D + 13 as the multiplier's bits leave it.  A multiplier
; under 256 is 8 steps, after which the low word is A's low byte over D + 14 (the product's low byte) and the high
; word is A's high byte.
		.section text.R816_UMul16
		.global R816_UMul16
R816_UMul16:
		.width (A, X), 16
		.relax
		lda [D + 13]
		cmp 0x100
		bcc R816_UMul16_Byte
		lda 0
		lsr [D + 13]
		bcc R816_UMul16_W_0
		clc
		adc [D + 19]
R816_UMul16_W_0:
		ror A
		ror [D + 13]
		bcc R816_UMul16_W_1
		clc
		adc [D + 19]
R816_UMul16_W_1:
		ror A
		ror [D + 13]
		bcc R816_UMul16_W_2
		clc
		adc [D + 19]
R816_UMul16_W_2:
		ror A
		ror [D + 13]
		bcc R816_UMul16_W_3
		clc
		adc [D + 19]
R816_UMul16_W_3:
		ror A
		ror [D + 13]
		bcc R816_UMul16_W_4
		clc
		adc [D + 19]
R816_UMul16_W_4:
		ror A
		ror [D + 13]
		bcc R816_UMul16_W_5
		clc
		adc [D + 19]
R816_UMul16_W_5:
		ror A
		ror [D + 13]
		bcc R816_UMul16_W_6
		clc
		adc [D + 19]
R816_UMul16_W_6:
		ror A
		ror [D + 13]
		bcc R816_UMul16_W_7
		clc
		adc [D + 19]
R816_UMul16_W_7:
		ror A
		ror [D + 13]
		bcc R816_UMul16_W_8
		clc
		adc [D + 19]
R816_UMul16_W_8:
		ror A
		ror [D + 13]
		bcc R816_UMul16_W_9
		clc
		adc [D + 19]
R816_UMul16_W_9:
		ror A
		ror [D + 13]
		bcc R816_UMul16_W_10
		clc
		adc [D + 19]
R816_UMul16_W_10:
		ror A
		ror [D + 13]
		bcc R816_UMul16_W_11
		clc
		adc [D + 19]
R816_UMul16_W_11:
		ror A
		ror [D + 13]
		bcc R816_UMul16_W_12
		clc
		adc [D + 19]
R816_UMul16_W_12:
		ror A
		ror [D + 13]
		bcc R816_UMul16_W_13
		clc
		adc [D + 19]
R816_UMul16_W_13:
		ror A
		ror [D + 13]
		bcc R816_UMul16_W_14
		clc
		adc [D + 19]
R816_UMul16_W_14:
		ror A
		ror [D + 13]
		bcc R816_UMul16_W_15
		clc
		adc [D + 19]
R816_UMul16_W_15:
		ror A
		ror [D + 13]
		ldx [D + 13]
		rtl
R816_UMul16_Byte:
		lda 0
		lsr [D + 13]
		bcc R816_UMul16_B_0
		clc
		adc [D + 19]
R816_UMul16_B_0:
		ror A
		ror [D + 13]
		bcc R816_UMul16_B_1
		clc
		adc [D + 19]
R816_UMul16_B_1:
		ror A
		ror [D + 13]
		bcc R816_UMul16_B_2
		clc
		adc [D + 19]
R816_UMul16_B_2:
		ror A
		ror [D + 13]
		bcc R816_UMul16_B_3
		clc
		adc [D + 19]
R816_UMul16_B_3:
		ror A
		ror [D + 13]
		bcc R816_UMul16_B_4
		clc
		adc [D + 19]
R816_UMul16_B_4:
		ror A
		ror [D + 13]
		bcc R816_UMul16_B_5
		clc
		adc [D + 19]
R816_UMul16_B_5:
		ror A
		ror [D + 13]
		bcc R816_UMul16_B_6
		clc
		adc [D + 19]
R816_UMul16_B_6:
		ror A
		ror [D + 13]
		bcc R816_UMul16_B_7
		clc
		adc [D + 19]
R816_UMul16_B_7:
		ror A
		ror [D + 13]
		xba
		tay
		lda [D + 14]
		and 0xFF
		sta [D + 13]
		tya
		and 0xFF00
		ora [D + 13]
		tax
		tya
		and 0xFF
		rtl


; R816_Mul32: the low 32 bits of a * b: aL * bL (R816_UMul16) plus aH * bL and aL * bH (their low words) in the high
; word, each skipped when its high word is 0.  The result's low word is kept at D + 5 and its high word at D + 7; b's
; high word is moved down to D + 19 for aL * bH once bL has been used.
		.section text.R816_Mul32
		.global R816_Mul32
R816_Mul32:
		.width (A, X), 16
		.relax
		lda [D + 15]
		sta [D + 13]
		jsl R816_UMul16
		stx [D + 5]
		sta [D + 7]
		lda [D + 17]
		beq R816_Mul32_NoAH
		sta [D + 13]
		jsl R816_UMul16
		txa
		clc
		adc [D + 7]
		sta [D + 7]
R816_Mul32_NoAH:
		lda [D + 21]
		beq R816_Mul32_Done
		sta [D + 19]
		lda [D + 15]
		sta [D + 13]
		jsl R816_UMul16
		txa
		clc
		adc [D + 7]
		sta [D + 7]
R816_Mul32_Done:
		ldx [D + 7]
		lda [D + 5]
		rtl


; R816_UMulWide32: the 64 bit product of the 32 bit a (D + 15) and b (D + 19), at D + 5 to D + 12 (A, X and Y changed):
; the four 16 bit products (R816_UMul16), those of a high word that is 0 skipped.  aL * bL is words 0 and 1 and aH * bL
; is added a word up; then b's high word is moved down to D + 19, and aH * bH is added two words up and aL * bH one.
		.section text.R816_UMulWide32
		.global R816_UMulWide32
R816_UMulWide32:
		.width (A, X), 16
		.relax
		lda [D + 15]
		sta [D + 13]
		jsl R816_UMul16
		stx [D + 5]
		sta [D + 7]
		stz [D + 9]
		stz [D + 11]
		lda [D + 17]
		beq R816_UMulWide32_NoAHBL
		sta [D + 13]
		jsl R816_UMul16
		sta [D + 9]
		txa
		clc
		adc [D + 7]
		sta [D + 7]
		bcc R816_UMulWide32_NoAHBL
		inc [D + 9]
R816_UMulWide32_NoAHBL:
		lda [D + 21]
		beq R816_UMulWide32_Done
		sta [D + 19]
		lda [D + 17]
		beq R816_UMulWide32_NoAHBH
		sta [D + 13]
		jsl R816_UMul16
		sta [D + 11]
		txa
		clc
		adc [D + 9]
		sta [D + 9]
		bcc R816_UMulWide32_NoAHBH
		inc [D + 11]
R816_UMulWide32_NoAHBH:
		lda [D + 15]
		sta [D + 13]
		jsl R816_UMul16
		tay
		txa
		clc
		adc [D + 7]
		sta [D + 7]
		tya
		adc [D + 9]
		sta [D + 9]
		bcc R816_UMulWide32_Done
		inc [D + 11]
R816_UMulWide32_Done:
		rtl
