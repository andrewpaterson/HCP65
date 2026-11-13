;  ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **
;
; Copyright (c) 2026 Andrew Paterson
;
; This file is part of The Codaphela Project: Codaphela CppCompilerLib
;
; Codaphela CppCompilerLib is free software: you can redistribute it and/or modify
; it under the terms of the GNU Lesser General Public License as published by
; the Free Software Foundation, either version 3 of the License, or
; (at your option) any later version.
;
; Codaphela CppCompilerLib is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; GNU Lesser General Public License for more details.
;
; You should have received a copy of the GNU Lesser General Public License
; along with Codaphela CppCompilerLib.  If not, see <http://www.gnu.org/licenses/>.
;
; ** ------------------------------------------------------------------------ 
;
; IRW65C816Runtime-MathTables.asm: the W65C816 back end's multiplies with Garth Wilson's product table, MathMult (linked
; into the program by CCPP65Builder::AddMathTables, or in main memory where an HCP65 program finds it:
; Libraries/UserLib/UserMathTables.asm).  IRW65C816Runtime-MathSafe.asm has the same routines without the table
; (CCPP65Builder::SetMathTables).  The others are in IRW65C816Runtime-Common.asm.
;


; The multiplies run on the caller's D (a scratch routine, IR816ScratchRoutine): a
; is at D + 15 and b at D + 19 (the scratch arguments), and they may use the caller's D + 5 to D + 22.  A 16 bit result
; is left in A, a 32 bit one in A (low) and X (high).
;
; Multiply looks bytes up in Garth Wilson's table (MathMult, Libc65/MathTbls/MathMult.asm, or a kernal's copy in main
; memory): the word at MathMult + ((x << 8) | y) * 2 is x * y.  The table is two banks from a bank's start, so the index
; doubled is X and its top bit (bit 7 of x) picks the bank.
;
; R816_Mul16: the low 16 bits of a * b, a byte product at a time (those of a high byte that is 0 skipped).
		.section text.R816_Mul16
		.global R816_Mul16
R816_Mul16:
		.width (A, X), 16
		sep m
		lda [D + 19]
		xba
		lda [D + 15]
		rep m
		asl A
		tax
		bcs R816_Mul16_LL_High
		lda [MathMult + X]:24
		bra R816_Mul16_LL_Got
R816_Mul16_LL_High:
		lda [MathMult + 0x10000 + X]:24
R816_Mul16_LL_Got:
		sta [D + 5]
		sep m
		lda [D + 20]
		beq R816_Mul16_NoHighB
		xba
		lda [D + 15]
		rep m
		asl A
		tax
		bcs R816_Mul16_LH_High
		lda [MathMult + X]:24
		bra R816_Mul16_LH_Got
R816_Mul16_LH_High:
		lda [MathMult + 0x10000 + X]:24
R816_Mul16_LH_Got:
		xba
		and 0xFF00
		clc
		adc [D + 5]
		sta [D + 5]
R816_Mul16_NoHighB:
		rep m
		sep m
		lda [D + 16]
		beq R816_Mul16_Done
		xba
		lda [D + 19]
		rep m
		asl A
		tax
		bcs R816_Mul16_HL_High
		lda [MathMult + X]:24
		bra R816_Mul16_HL_Got
R816_Mul16_HL_High:
		lda [MathMult + 0x10000 + X]:24
R816_Mul16_HL_Got:
		xba
		and 0xFF00
		clc
		adc [D + 5]
		sta [D + 5]
R816_Mul16_Done:
		rep m
		lda [D + 5]
		rtl


; R816_Mul32: the result is put together at D + 5 to D + 8, a's bytes are D + 15 to D + 18 and b's D + 19 to D + 22.
; With b under 256 (checked first) it is a's bytes times b (a's high word skipped when it is 0, its top byte when that
; is).  Otherwise the result starts at 0 and the product is up to ten byte products.  A product of a 0 byte is skipped,
; as are those of a high word that is 0.  A product a byte up adds to the result's middle word and carries into its top
; byte; two bytes up adds to its high word.
		.section text.R816_Mul32
		.global R816_Mul32
R816_Mul32:
		.width (A, X), 16
		.relax
		lda [D + 21]
		bne R816_Mul32_Wide
		lda [D + 19]
		cmp 256
		bcc R816_Mul32_X
R816_Mul32_Wide:
		stz [D + 5]
		stz [D + 7]
		sep m
		lda [D + 19]
		beq R816_Mul32_LL_Zero
		xba
		lda [D + 15]
		beq R816_Mul32_LL_Zero
		rep m
		asl A
		tax
		bcs R816_Mul32_LL_High
		lda [MathMult + X]:24
		bra R816_Mul32_LL_Got
R816_Mul32_LL_High:
		lda [MathMult + 0x10000 + X]:24
R816_Mul32_LL_Got:
		sta [D + 5]
R816_Mul32_LL_Zero:
		rep m
		sep m
		lda [D + 20]
		beq R816_Mul32_HH_Zero
		xba
		lda [D + 16]
		beq R816_Mul32_HH_Zero
		rep m
		asl A
		tax
		bcs R816_Mul32_HH_High
		lda [MathMult + X]:24
		bra R816_Mul32_HH_Got
R816_Mul32_HH_High:
		lda [MathMult + 0x10000 + X]:24
R816_Mul32_HH_Got:
		sta [D + 7]
R816_Mul32_HH_Zero:
		rep m
		sep m
		lda [D + 20]
		beq R816_Mul32_LH_Zero
		xba
		lda [D + 15]
		beq R816_Mul32_LH_Zero
		rep m
		asl A
		tax
		bcs R816_Mul32_LH_High
		lda [MathMult + X]:24
		bra R816_Mul32_LH_Got
R816_Mul32_LH_High:
		lda [MathMult + 0x10000 + X]:24
R816_Mul32_LH_Got:
		clc
		adc [D + 6]
		sta [D + 6]
		bcc C + 6
		sep m
		inc [D + 8]
		rep m
R816_Mul32_LH_Zero:
		rep m
		sep m
		lda [D + 19]
		beq R816_Mul32_HL_Zero
		xba
		lda [D + 16]
		beq R816_Mul32_HL_Zero
		rep m
		asl A
		tax
		bcs R816_Mul32_HL_High
		lda [MathMult + X]:24
		bra R816_Mul32_HL_Got
R816_Mul32_HL_High:
		lda [MathMult + 0x10000 + X]:24
R816_Mul32_HL_Got:
		clc
		adc [D + 6]
		sta [D + 6]
		bcc C + 6
		sep m
		inc [D + 8]
		rep m
R816_Mul32_HL_Zero:
		rep m
		lda [D + 21]
		beq R816_Mul32_NoHighB
		sep m
		lda [D + 21]
		beq R816_Mul32_B2_Zero
		xba
		lda [D + 15]
		beq R816_Mul32_B2_Zero
		rep m
		asl A
		tax
		bcs R816_Mul32_B2_High
		lda [MathMult + X]:24
		bra R816_Mul32_B2_Got
R816_Mul32_B2_High:
		lda [MathMult + 0x10000 + X]:24
R816_Mul32_B2_Got:
		clc
		adc [D + 7]
		sta [D + 7]
R816_Mul32_B2_Zero:
		rep m
		sep m
		lda [D + 22]
		beq R816_Mul32_B3_Zero
		xba
		lda [D + 15]
		beq R816_Mul32_B3_Zero
		rep m
		asl A
		tax
		bcs R816_Mul32_B3_High
		lda [MathMult + X]:24
		bra R816_Mul32_B3_Got
R816_Mul32_B3_High:
		lda [MathMult + 0x10000 + X]:24
R816_Mul32_B3_Got:
		xba
		and 0xFF00
		clc
		adc [D + 7]
		sta [D + 7]
R816_Mul32_B3_Zero:
		rep m
		sep m
		lda [D + 21]
		beq R816_Mul32_B2A1_Zero
		xba
		lda [D + 16]
		beq R816_Mul32_B2A1_Zero
		rep m
		asl A
		tax
		bcs R816_Mul32_B2A1_High
		lda [MathMult + X]:24
		bra R816_Mul32_B2A1_Got
R816_Mul32_B2A1_High:
		lda [MathMult + 0x10000 + X]:24
R816_Mul32_B2A1_Got:
		xba
		and 0xFF00
		clc
		adc [D + 7]
		sta [D + 7]
R816_Mul32_B2A1_Zero:
		rep m
R816_Mul32_NoHighB:
		lda [D + 17]
		beq R816_Mul32_Done
		sep m
		lda [D + 19]
		beq R816_Mul32_A2_Zero
		xba
		lda [D + 17]
		beq R816_Mul32_A2_Zero
		rep m
		asl A
		tax
		bcs R816_Mul32_A2_High
		lda [MathMult + X]:24
		bra R816_Mul32_A2_Got
R816_Mul32_A2_High:
		lda [MathMult + 0x10000 + X]:24
R816_Mul32_A2_Got:
		clc
		adc [D + 7]
		sta [D + 7]
R816_Mul32_A2_Zero:
		rep m
		sep m
		lda [D + 19]
		beq R816_Mul32_A3_Zero
		xba
		lda [D + 18]
		beq R816_Mul32_A3_Zero
		rep m
		asl A
		tax
		bcs R816_Mul32_A3_High
		lda [MathMult + X]:24
		bra R816_Mul32_A3_Got
R816_Mul32_A3_High:
		lda [MathMult + 0x10000 + X]:24
R816_Mul32_A3_Got:
		xba
		and 0xFF00
		clc
		adc [D + 7]
		sta [D + 7]
R816_Mul32_A3_Zero:
		rep m
		sep m
		lda [D + 20]
		beq R816_Mul32_A2B1_Zero
		xba
		lda [D + 17]
		beq R816_Mul32_A2B1_Zero
		rep m
		asl A
		tax
		bcs R816_Mul32_A2B1_High
		lda [MathMult + X]:24
		bra R816_Mul32_A2B1_Got
R816_Mul32_A2B1_High:
		lda [MathMult + 0x10000 + X]:24
R816_Mul32_A2B1_Got:
		xba
		and 0xFF00
		clc
		adc [D + 7]
		sta [D + 7]
R816_Mul32_A2B1_Zero:
		rep m
R816_Mul32_Done:
		lda [D + 7]
		tax
		lda [D + 5]
		rtl
R816_Mul32_X:
		sep m
		lda [D + 19]
		xba
		lda [D + 15]
		rep m
		asl A
		tax
		bcs R816_Mul32_X0_High
		lda [MathMult + X]:24
		bra R816_Mul32_X0_Got
R816_Mul32_X0_High:
		lda [MathMult + 0x10000 + X]:24
R816_Mul32_X0_Got:
		sta [D + 5]
		lda [D + 17]
		beq R816_Mul32_XShort
		sep m
		lda [D + 19]
		xba
		lda [D + 17]
		rep m
		asl A
		tax
		bcs R816_Mul32_X2_High
		lda [MathMult + X]:24
		bra R816_Mul32_X2_Got
R816_Mul32_X2_High:
		lda [MathMult + 0x10000 + X]:24
R816_Mul32_X2_Got:
		sta [D + 7]
		sep m
		lda [D + 19]
		xba
		lda [D + 18]
		beq R816_Mul32_XNo3
		rep m
		asl A
		tax
		bcs R816_Mul32_X3_High
		lda [MathMult + X]:24
		bra R816_Mul32_X3_Got
R816_Mul32_X3_High:
		lda [MathMult + 0x10000 + X]:24
R816_Mul32_X3_Got:
		xba
		and 0xFF00
		clc
		adc [D + 7]
		sta [D + 7]
R816_Mul32_XNo3:
		rep m
		bra R816_Mul32_XByte1
R816_Mul32_XShort:
		stz [D + 7]
R816_Mul32_XByte1:
		sep m
		lda [D + 19]
		xba
		lda [D + 16]
		beq R816_Mul32_XDone
		rep m
		asl A
		tax
		bcs R816_Mul32_X1_High
		lda [MathMult + X]:24
		bra R816_Mul32_X1_Got
R816_Mul32_X1_High:
		lda [MathMult + 0x10000 + X]:24
R816_Mul32_X1_Got:
		clc
		adc [D + 6]
		sta [D + 6]
		bcc C + 6
		sep m
		inc [D + 8]
		rep m
R816_Mul32_XDone:
		rep m
		lda [D + 7]
		tax
		lda [D + 5]
		rtl


; R816_UMulWide32: the 64 bit product of the 32 bit a (D + 15) and b (D + 19), at D + 5 to D + 12 (A, X and Y
; changed).  b's bytes doubled are pushed; each row (a byte of a that isn't 0) has a's byte * 512 in Y and the table's
; bank by its top bit, so a byte product is tya, ora, tax and a long load; the even products (b0 and b2) are added as
; a 32 bit word pair at byte i, the odd ones (b1 and b3) at byte i + 1.
		.section text.R816_UMulWide32
		.global R816_UMulWide32
R816_UMulWide32:
		.width (A, X), 16
		.relax
		lda [D + 22]
		and 0xFF
		asl A
		pha
		lda [D + 21]
		and 0xFF
		asl A
		pha
		lda [D + 20]
		and 0xFF
		asl A
		pha
		lda [D + 19]
		and 0xFF
		asl A
		pha
		stz [D + 5]
		stz [D + 7]
		stz [D + 9]
		stz [D + 11]
		lda [D + 15]
		and 0xFF
		beq R816_UMulWide32_Next0
		xba
		asl A
		tay
		bcs R816_UMulWide32_High0
		tya
		ora [S + 1]
		tax
		lda [MathMult + X]:24
		clc
		adc [D + 5]
		sta [D + 5]
		tya
		ora [S + 5]
		tax
		lda [MathMult + X]:24
		adc [D + 7]
		sta [D + 7]
		bcc R816_UMulWide32_C0_0
		inc [D + 9]
		bne R816_UMulWide32_C0_0
		inc [D + 11]
R816_UMulWide32_C0_0:
		tya
		ora [S + 3]
		tax
		lda [MathMult + X]:24
		clc
		adc [D + 6]
		sta [D + 6]
		tya
		ora [S + 7]
		tax
		lda [MathMult + X]:24
		adc [D + 8]
		sta [D + 8]
		bcc R816_UMulWide32_C0_1
		inc [D + 10]
		bne R816_UMulWide32_C0_1
		inc [D + 12]
R816_UMulWide32_C0_1:
		brl R816_UMulWide32_Next0
R816_UMulWide32_High0:
		tya
		ora [S + 1]
		tax
		lda [MathMult + 0x10000 + X]:24
		clc
		adc [D + 5]
		sta [D + 5]
		tya
		ora [S + 5]
		tax
		lda [MathMult + 0x10000 + X]:24
		adc [D + 7]
		sta [D + 7]
		bcc R816_UMulWide32_C1_0
		inc [D + 9]
		bne R816_UMulWide32_C1_0
		inc [D + 11]
R816_UMulWide32_C1_0:
		tya
		ora [S + 3]
		tax
		lda [MathMult + 0x10000 + X]:24
		clc
		adc [D + 6]
		sta [D + 6]
		tya
		ora [S + 7]
		tax
		lda [MathMult + 0x10000 + X]:24
		adc [D + 8]
		sta [D + 8]
		bcc R816_UMulWide32_C1_1
		inc [D + 10]
		bne R816_UMulWide32_C1_1
		inc [D + 12]
R816_UMulWide32_C1_1:
R816_UMulWide32_Next0:
		lda [D + 16]
		and 0xFF
		beq R816_UMulWide32_Next1
		xba
		asl A
		tay
		bcs R816_UMulWide32_High1
		tya
		ora [S + 1]
		tax
		lda [MathMult + X]:24
		clc
		adc [D + 6]
		sta [D + 6]
		tya
		ora [S + 5]
		tax
		lda [MathMult + X]:24
		adc [D + 8]
		sta [D + 8]
		bcc R816_UMulWide32_C2_0
		inc [D + 10]
		bne R816_UMulWide32_C2_0
		inc [D + 12]
R816_UMulWide32_C2_0:
		tya
		ora [S + 3]
		tax
		lda [MathMult + X]:24
		clc
		adc [D + 7]
		sta [D + 7]
		tya
		ora [S + 7]
		tax
		lda [MathMult + X]:24
		adc [D + 9]
		sta [D + 9]
		bcc R816_UMulWide32_C2_1
		inc [D + 11]
R816_UMulWide32_C2_1:
		brl R816_UMulWide32_Next1
R816_UMulWide32_High1:
		tya
		ora [S + 1]
		tax
		lda [MathMult + 0x10000 + X]:24
		clc
		adc [D + 6]
		sta [D + 6]
		tya
		ora [S + 5]
		tax
		lda [MathMult + 0x10000 + X]:24
		adc [D + 8]
		sta [D + 8]
		bcc R816_UMulWide32_C3_0
		inc [D + 10]
		bne R816_UMulWide32_C3_0
		inc [D + 12]
R816_UMulWide32_C3_0:
		tya
		ora [S + 3]
		tax
		lda [MathMult + 0x10000 + X]:24
		clc
		adc [D + 7]
		sta [D + 7]
		tya
		ora [S + 7]
		tax
		lda [MathMult + 0x10000 + X]:24
		adc [D + 9]
		sta [D + 9]
		bcc R816_UMulWide32_C3_1
		inc [D + 11]
R816_UMulWide32_C3_1:
R816_UMulWide32_Next1:
		lda [D + 17]
		and 0xFF
		beq R816_UMulWide32_Next2
		xba
		asl A
		tay
		bcs R816_UMulWide32_High2
		tya
		ora [S + 1]
		tax
		lda [MathMult + X]:24
		clc
		adc [D + 7]
		sta [D + 7]
		tya
		ora [S + 5]
		tax
		lda [MathMult + X]:24
		adc [D + 9]
		sta [D + 9]
		bcc R816_UMulWide32_C4_0
		inc [D + 11]
R816_UMulWide32_C4_0:
		tya
		ora [S + 3]
		tax
		lda [MathMult + X]:24
		clc
		adc [D + 8]
		sta [D + 8]
		tya
		ora [S + 7]
		tax
		lda [MathMult + X]:24
		adc [D + 10]
		sta [D + 10]
		bcc R816_UMulWide32_C4_1
		inc [D + 12]
R816_UMulWide32_C4_1:
		brl R816_UMulWide32_Next2
R816_UMulWide32_High2:
		tya
		ora [S + 1]
		tax
		lda [MathMult + 0x10000 + X]:24
		clc
		adc [D + 7]
		sta [D + 7]
		tya
		ora [S + 5]
		tax
		lda [MathMult + 0x10000 + X]:24
		adc [D + 9]
		sta [D + 9]
		bcc R816_UMulWide32_C5_0
		inc [D + 11]
R816_UMulWide32_C5_0:
		tya
		ora [S + 3]
		tax
		lda [MathMult + 0x10000 + X]:24
		clc
		adc [D + 8]
		sta [D + 8]
		tya
		ora [S + 7]
		tax
		lda [MathMult + 0x10000 + X]:24
		adc [D + 10]
		sta [D + 10]
		bcc R816_UMulWide32_C5_1
		inc [D + 12]
R816_UMulWide32_C5_1:
R816_UMulWide32_Next2:
		lda [D + 18]
		and 0xFF
		beq R816_UMulWide32_Next3
		xba
		asl A
		tay
		bcs R816_UMulWide32_High3
		tya
		ora [S + 1]
		tax
		lda [MathMult + X]:24
		clc
		adc [D + 8]
		sta [D + 8]
		tya
		ora [S + 5]
		tax
		lda [MathMult + X]:24
		adc [D + 10]
		sta [D + 10]
		bcc R816_UMulWide32_C6_0
		inc [D + 12]
R816_UMulWide32_C6_0:
		tya
		ora [S + 3]
		tax
		lda [MathMult + X]:24
		clc
		adc [D + 9]
		sta [D + 9]
		tya
		ora [S + 7]
		tax
		lda [MathMult + X]:24
		adc [D + 11]
		sta [D + 11]
		brl R816_UMulWide32_Next3
R816_UMulWide32_High3:
		tya
		ora [S + 1]
		tax
		lda [MathMult + 0x10000 + X]:24
		clc
		adc [D + 8]
		sta [D + 8]
		tya
		ora [S + 5]
		tax
		lda [MathMult + 0x10000 + X]:24
		adc [D + 10]
		sta [D + 10]
		bcc R816_UMulWide32_C7_0
		inc [D + 12]
R816_UMulWide32_C7_0:
		tya
		ora [S + 3]
		tax
		lda [MathMult + 0x10000 + X]:24
		clc
		adc [D + 9]
		sta [D + 9]
		tya
		ora [S + 7]
		tax
		lda [MathMult + 0x10000 + X]:24
		adc [D + 11]
		sta [D + 11]
R816_UMulWide32_Next3:
		tsc
		clc
		adc 8
		tcs
		rtl
