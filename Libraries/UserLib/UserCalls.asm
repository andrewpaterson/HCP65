; Kernal calls for programs (see HCP65User.h): COP n with A the address of the call's arguments on the stack (in bank 0,
; where the caller pushed them: the first at S + 4 after the jsl).  The kernal reads them through the process's remap
; page and returns a result in A, or A (low) and X (high).  COP 0 is exit (UserStart.asm).
;
; The compiler passes extern "C" arguments in registers too (CppCompilerLib/W65C816.txt): a call with arguments goes to
; the label with its register suffix.  Those entries push the register words again, and copy the caller's pushed
; words below them, so the stack holds every argument in order; then they call the stack entry and pop what they
; pushed (ply keeps the result in A and X).  The caller removes its own pushed words.

		.section text.KernalPutValue
		.global _KernalPutValue.a
_KernalPutValue.a:
		.width (A, X), 16
		pha
		jsl _KernalPutValue
		ply
		rtl

		.global _KernalPutValue
_KernalPutValue:
		.width (A, X), 16
		tsc
		clc
		adc 4
		cop 1
		rtl

		.section text.KernalExecute
		.global _KernalExecute.axy
_KernalExecute.axy:
		.width (A, X), 16
		pha
		lda [S + 8]
		pha
		lda [S + 8]
		pha
		phy
		phx
		lda [S + 9]
		pha
		jsl _KernalExecute
		ply
		ply
		ply
		ply
		ply
		ply
		rtl

		.global _KernalExecute
_KernalExecute:
		.width (A, X), 16
		tsc
		clc
		adc 4
		cop 2
		rtl

		.section text.KernalGetArgument
		.global _KernalGetArgument
_KernalGetArgument:
		.width (A, X), 16
		tsc
		clc
		adc 4
		cop 3
		rtl

		.section text.KernalFileSize
		.global _KernalFileSize.ax
_KernalFileSize.ax:
		.width (A, X), 16
		phx
		pha
		jsl _KernalFileSize
		ply
		ply
		rtl

		.global _KernalFileSize
_KernalFileSize:
		.width (A, X), 16
		tsc
		clc
		adc 4
		cop 4
		rtl

		.section text.KernalFileLoad
		.global _KernalFileLoad.ax
_KernalFileLoad.ax:
		.width (A, X), 16
		pha
		lda [S + 12]
		pha
		lda [S + 12]
		pha
		lda [S + 12]
		pha
		lda [S + 12]
		pha
		phx
		lda [S + 11]
		pha
		jsl _KernalFileLoad
		ply
		ply
		ply
		ply
		ply
		ply
		ply
		rtl

		.global _KernalFileLoad
_KernalFileLoad:
		.width (A, X), 16
		tsc
		clc
		adc 4
		cop 5
		rtl

		.section text.KernalFileRead
		.global _KernalFileRead.ax
_KernalFileRead.ax:
		.width (A, X), 16
		pha
		lda [S + 16]
		pha
		lda [S + 16]
		pha
		lda [S + 16]
		pha
		lda [S + 16]
		pha
		lda [S + 16]
		pha
		lda [S + 16]
		pha
		phx
		lda [S + 15]
		pha
		jsl _KernalFileRead
		ply
		ply
		ply
		ply
		ply
		ply
		ply
		ply
		ply
		rtl

		.global _KernalFileRead
_KernalFileRead:
		.width (A, X), 16
		tsc
		clc
		adc 4
		cop 11
		rtl

		.section text.KernalDeviceCall
		.global _KernalDeviceCall.ax
_KernalDeviceCall.ax:
		.width (A, X), 16
		phx
		pha
		jsl _KernalDeviceCall
		ply
		ply
		rtl

		.global _KernalDeviceCall
_KernalDeviceCall:
		.width (A, X), 16
		tsc
		clc
		adc 4
		cop 12
		rtl
