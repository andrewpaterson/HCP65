; Garth Wilson's maths tables, which boot.exe loads from the SD card (\OS\Math\MULT.BIN and INVERT.BIN) into main
; memory at $400000 and the kernal maps into every process at the same address, so a program's multiplies and its soft
; float reach them there and its executable doesn't carry them.  Their layouts are Libc65/MathTbls' MathMult.asm and
; MathInvert.asm: MathMult is 128KB, MathInvert 256KB (MathInvertHigh is its upper half).  The kernal and boot.exe are
; built without them (MathTables="false").

MathMult = 0x400000
MathInvert = 0x420000
MathInvertHigh = 0x440000
		.global MathMult
		.global MathInvert
		.global MathInvertHigh
