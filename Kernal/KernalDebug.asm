; How the kernal tells the emulator something (DebugLib/HCP65DebugMessage.h, KernalDebug.cpp).  m and x are 16 bit on entry
; and on return.  WDM is a two byte no-op on a real W65C816.

DEBUG_WDM_SIGNATURE	= 0xD6		; HCP65_DEBUG_WDM_SIGNATURE


		.section text.KernalDebug


; extern "C" void KernalDebugTell(SHCP65DebugMessage* psMessage): the emulator reads the message from X:A.
		.global _KernalDebugTell
_KernalDebugTell:
		.width (A, X), 16
		lda [S + 6]
		tax
		lda [S + 4]

		.global _KernalDebugTell.ax
_KernalDebugTell.ax:
		.width (A, X), 16
		wdm DEBUG_WDM_SIGNATURE
		rtl
