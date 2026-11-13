; Garth Wilson's reciprocal table (INVERT.HEX, from wilsonminesco.com): the long at MathInvert +
; d * 4 is round(2^32 / d) for d = 2 to $FFFF ($FFFFFFFF for 0 and 1).  The file has it at
; $40000.  It is two 128KB halves, each starting on a bank (d * 4 is then a 16 bit index, bit 14
; of d picking the bank), so a program links only the half it uses: MathInvertHigh (d >= $8000,
; the reciprocals of a normalised divisor's top 16 bits) is the soft float's divides'
; (softfloatW65C816.asm).  The runtime's integer divides read neither (shift and subtract is faster:
; IRW65C816Runtime-Common.asm).  On the HCP65 only MathInvertHigh is in main memory; the lower half
; stays in INVERT.BIN on the SD card (Libraries/UserLib/UserMathTables.asm).
	.section rodata.MathInvert
	.align $10000
	.global MathInvert
MathInvert:
	.inchex "INVERT.HEX", $40000, $20000

	.section rodata.MathInvertHigh
	.align $10000
	.global MathInvertHigh
MathInvertHigh:
	.inchex "INVERT.HEX", $60000, $20000
