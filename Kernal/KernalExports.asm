; The kernal's exports: what a driver loaded from the SD card (KernalDriverLoader.cpp) may call, by name.  A driver is
; a relocatable module (Assembler65Lib's CAsm65Module) whose imports are the kernal names it uses: the loader finds
; each here when it puts the driver in MPU RAM, and a name that isn't here stops the driver loading.
;
; A C++ function has two entries (CppParserLib/W65C816.txt): its name, which takes its arguments on the stack (a
; virtual call's), and its name with a suffix for the registers it takes its first arguments in (a direct call's).
; Both are exported when drivers make direct calls to it.  CKernalDriver's virtuals are exported because a driver's
; vtables have the ones it doesn't override.  Add to the table as drivers need more of the kernal.
;
; gauiKernalExports is guiKernalNumExports SKernalExports (KernalDriverLoader.h): each the name's address then the
; symbol's, four bytes each.

		.section rodata.KernalExports
		.align 4
		.global _gauiKernalExports
_gauiKernalExports:
		.long ExportDriverInit
		.long _CKernalDriver__Init
		.long ExportDriverInitRegisters
		.long _CKernalDriver__Init.axssy
		.long ExportDriverKill
		.long _CKernalDriver__Kill
		.long ExportDriverNumUnits
		.long _CKernalDriver__NumUnits
		.long ExportDriverInterrupt
		.long _CKernalDriver__Interrupt
		.long ExportDriverOpen
		.long _CKernalDriver__Open
		.long ExportDriverClose
		.long _CKernalDriver__Close
		.long ExportDriverRead
		.long _CKernalDriver__Read
		.long ExportDriverWrite
		.long _CKernalDriver__Write
		.long ExportDriverControl
		.long _CKernalDriver__Control
		.long ExportDriverWait
		.long _CKernalDriver__Wait
		.long ExportDriverSetInterrupt
		.long _CKernalDriver__SetInterrupt
		.long ExportDriverSetInterruptRegisters
		.long _CKernalDriver__SetInterrupt.axssy
		.long ExportTicks
		.long _KernalTicks
		.long ExportWaitForInterrupt
		.long _KernalWaitForInterrupt
		.long ExportUserPointer
		.long _KernalUserPointer__FjPj
		.long ExportUserPointerRegisters
		.long _KernalUserPointer__FjPj.ax

		.global _guiKernalNumExports
_guiKernalNumExports:
		.word 17

		.section rodata.KernalExportNames
ExportDriverInit:
		.byte "_CKernalDriver__Init", 0
ExportDriverInitRegisters:
		.byte "_CKernalDriver__Init.axssy", 0
ExportDriverKill:
		.byte "_CKernalDriver__Kill", 0
ExportDriverNumUnits:
		.byte "_CKernalDriver__NumUnits", 0
ExportDriverInterrupt:
		.byte "_CKernalDriver__Interrupt", 0
ExportDriverOpen:
		.byte "_CKernalDriver__Open", 0
ExportDriverClose:
		.byte "_CKernalDriver__Close", 0
ExportDriverRead:
		.byte "_CKernalDriver__Read", 0
ExportDriverWrite:
		.byte "_CKernalDriver__Write", 0
ExportDriverControl:
		.byte "_CKernalDriver__Control", 0
ExportDriverWait:
		.byte "_CKernalDriver__Wait", 0
ExportDriverSetInterrupt:
		.byte "_CKernalDriver__SetInterrupt", 0
ExportDriverSetInterruptRegisters:
		.byte "_CKernalDriver__SetInterrupt.axssy", 0
ExportTicks:
		.byte "_KernalTicks", 0
ExportWaitForInterrupt:
		.byte "_KernalWaitForInterrupt", 0
ExportUserPointer:
		.byte "_KernalUserPointer__FjPj", 0
ExportUserPointerRegisters:
		.byte "_KernalUserPointer__FjPj.ax", 0
