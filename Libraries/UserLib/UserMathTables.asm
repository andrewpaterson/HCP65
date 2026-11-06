; Garth Wilson's maths tables, which boot.exe loads from the SD card (\OS\Math\MULT.BIN and INVERT.BIN) into main
; memory and the kernal maps into every process at the same address, so a program's multiplies and its soft float
; reach them there and its executable doesn't carry them.  Their layouts are Libc65/MathTbls' MathMult.asm and
; MathInvert.asm: MathMult is 128KB at $400000; of INVERT only the upper half, MathInvertHigh (the soft float's
; divides), is loaded, 128KB at $420000.  INVERT's lower half (MathInvert) stays on the card: nothing here gives it an
; address, so a program that used it wouldn't link.  The kernal and boot.exe are built without the tables
; (MathTables="false").

MathMult = 0x400000
MathInvertHigh = 0x420000
		.global MathMult
		.global MathInvertHigh
