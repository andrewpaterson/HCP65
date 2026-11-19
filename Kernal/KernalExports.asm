; The kernal's exports: what a driver loaded from the SD card (KernalDriverLoader.cpp) may call, by name.  A driver is
; a relocatable module (Assembler65Lib's CAsm65Module) whose imports are the kernal names it uses: the loader finds
; each here when it puts the driver in MPU RAM, and a name that isn't here stops the driver loading.
;
; A C++ function has two entries (CppCompilerLib/W65C816.txt): its name, which takes its arguments on the stack (a
; virtual call's), and its name with a suffix for the registers it takes its first arguments in (a direct call's).
; Both are exported when drivers make direct calls to it.  CKernalDriver's virtuals are exported because a driver's
; vtables have the ones it doesn't override.  Add to the table as drivers need more of the kernal.
;
; gauiKernalExports is guiKernalNumExports SKernalExports (KernalDriverLoader.h): each the name's address then the
; symbol's, four bytes each.  guiKernalNumExports is worked out from the table's size, so adding to it is enough.

		.section rodata.KernalExports
		.align 4
		.global _gauiKernalExports
_gauiKernalExports:
		; CKernalDriver: the base class a driver's board class derives from.
		.long Export_CKernalDriver__Init
		.long _CKernalDriver__Init
		.long Export_CKernalDriver__Init_axssy
		.long _CKernalDriver__Init.axssy
		.long Export_CKernalDriver__Kill
		.long _CKernalDriver__Kill
		.long Export_CKernalDriver__Kill_ax
		.long _CKernalDriver__Kill.ax
		.long Export_CKernalDriver__NumUnits
		.long _CKernalDriver__NumUnits
		.long Export_CKernalDriver__NumUnits_ax
		.long _CKernalDriver__NumUnits.ax
		.long Export_CKernalDriver__Interrupt
		.long _CKernalDriver__Interrupt
		.long Export_CKernalDriver__Interrupt_ax
		.long _CKernalDriver__Interrupt.ax
		.long Export_CKernalDriver__Read
		.long _CKernalDriver__Read
		.long Export_CKernalDriver__Read_axy
		.long _CKernalDriver__Read.axy
		.long Export_CKernalDriver__Write
		.long _CKernalDriver__Write
		.long Export_CKernalDriver__Write_axy
		.long _CKernalDriver__Write.axy
		.long Export_CKernalDriver__Control
		.long _CKernalDriver__Control
		.long Export_CKernalDriver__Control_axy
		.long _CKernalDriver__Control.axy
		.long Export_CKernalDriver__Wait
		.long _CKernalDriver__Wait
		.long Export_CKernalDriver__Wait_axy
		.long _CKernalDriver__Wait.axy
		.long Export_CKernalDriver__Blit
		.long _CKernalDriver__Blit
		.long Export_CKernalDriver__SetInterrupt
		.long _CKernalDriver__SetInterrupt
		.long Export_CKernalDriver__SetInterrupt_axssy
		.long _CKernalDriver__SetInterrupt.axssy
		.long Export_CKernalDriver__GetDevice
		.long _CKernalDriver__GetDevice
		.long Export_CKernalDriver__GetDevice_ax
		.long _CKernalDriver__GetDevice.ax
		.long Export_CKernalDriver__GetSlot
		.long _CKernalDriver__GetSlot
		.long Export_CKernalDriver__GetSlot_ax
		.long _CKernalDriver__GetSlot.ax
		.long Export_CKernalDriver__GetRegisters
		.long _CKernalDriver__GetRegisters
		.long Export_CKernalDriver__GetRegisters_ax
		.long _CKernalDriver__GetRegisters.ax
		.long Export_CKernalDriver__OpenUnit
		.long _CKernalDriver__OpenUnit
		.long Export_CKernalDriver__OpenUnit_axy
		.long _CKernalDriver__OpenUnit.axy
		.long Export_CKernalDriver__CloseUnit
		.long _CKernalDriver__CloseUnit
		.long Export_CKernalDriver__CloseUnit_axy
		.long _CKernalDriver__CloseUnit.axy
		.long Export_CKernalDriver__IsWaitOver
		.long _CKernalDriver__IsWaitOver
		.long Export_CKernalDriver__IsWaitOver_axy
		.long _CKernalDriver__IsWaitOver.axy
		.long Export_CKernalDriver__WaitFor
		.long _CKernalDriver__WaitFor
		.long Export_CKernalDriver__WaitFor_axy
		.long _CKernalDriver__WaitFor.axy
		; The devices (KernalDevices.h) and their drivers (KernalDriver.h).
		.long Export_gasKernalDevices
		.long _gasKernalDevices
		.long Export_KernalReadRegister__Fh
		.long _KernalReadRegister__Fh
		.long Export_KernalReadRegister__Fh_a
		.long _KernalReadRegister__Fh.a
		.long Export_KernalWriteRegister__Fhh
		.long _KernalWriteRegister__Fhh
		.long Export_KernalWriteRegister__Fhh_ax
		.long _KernalWriteRegister__Fhh.ax
		.long Export_KernalEnableDevice__Fj
		.long _KernalEnableDevice__Fj
		.long Export_KernalEnableDevice__Fj_a
		.long _KernalEnableDevice__Fj.a
		.long Export_KernalDisableDevice__Fj
		.long _KernalDisableDevice__Fj
		.long Export_KernalDisableDevice__Fj_a
		.long _KernalDisableDevice__Fj.a
		.long Export_KernalGetDriver__Fj
		.long _KernalGetDriver__Fj
		.long Export_KernalGetDriver__Fj_a
		.long _KernalGetDriver__Fj.a
		.long Export_KernalFindDriver__FPKcPKcj
		.long _KernalFindDriver__FPKcPKcj
		.long Export_KernalFindDriver__FPKcPKcj_axssy
		.long _KernalFindDriver__FPKcPKcj.axssy
		; Interrupts and time (KernalInterrupts.h).
		.long Export_KernalSetInterruptHandler__FjPFvPvEPvPVhb
		.long _KernalSetInterruptHandler__FjPFvPvEPvPVhb
		.long Export_KernalSetInterruptHandler__FjPFvPvEPvPVhb_axy
		.long _KernalSetInterruptHandler__FjPFvPvEPvPVhb.axy
		.long Export_KernalClearInterruptHandler__Fj
		.long _KernalClearInterruptHandler__Fj
		.long Export_KernalClearInterruptHandler__Fj_a
		.long _KernalClearInterruptHandler__Fj.a
		.long Export_KernalTicks
		.long _KernalTicks
		.long Export_KernalWaitForInterrupt
		.long _KernalWaitForInterrupt
		.long Export_KernalRunPendingInterrupts
		.long _KernalRunPendingInterrupts
		.long Export_KernalInterruptsOn
		.long _KernalInterruptsOn
		.long Export_KernalInterruptsOff
		.long _KernalInterruptsOff
		.long Export_gbKernalMainBusBusy
		.long _gbKernalMainBusBusy
		.long Export_guiKernalTicks
		.long _guiKernalTicks
		; The running process's memory (KernalProcess.h).
		.long Export_KernalUserPointer__FmPm
		.long _KernalUserPointer__FmPm
		.long Export_KernalUserPointer__FmPm_ax
		.long _KernalUserPointer__FmPm.ax
		.long Export_KernalCopyToUser__FmPKhm
		.long _KernalCopyToUser__FmPKhm
		.long Export_KernalCopyToUser__FmPKhm_ax
		.long _KernalCopyToUser__FmPKhm.ax
		.long Export_KernalCopyFromUser__FPhmm
		.long _KernalCopyFromUser__FPhmm
		.long Export_KernalCopyFromUser__FPhmm_ax
		.long _KernalCopyFromUser__FPhmm.ax
		; The kernal's main memory heap (KernalMemory.h).
		.long Export_KernalMainMalloc__Fmb
		.long _KernalMainMalloc__Fmb
		.long Export_KernalMainMalloc__Fmb_axy
		.long _KernalMainMalloc__Fmb.axy
		.long Export_KernalMainFree__FPv
		.long _KernalMainFree__FPv
		.long Export_KernalMainFree__FPv_ax
		.long _KernalMainFree__FPv.ax
		; Main memory's copies (KernalDriver.h).
		.long Export_gpuiKernalBlitter
		.long _gpuiKernalBlitter
		.long Export_gpuiKernalGraphics
		.long _gpuiKernalGraphics
		.long Export_KernalMainCopy__FPhPKhjj_axssy
		.long _KernalMainCopy__FPhPKhjj.axssy
KernalExportsEnd:

		; The assembler counts them: eight bytes each.
		.global _guiKernalNumExports
_guiKernalNumExports:
		.word (KernalExportsEnd - _gauiKernalExports) / 8

		.section rodata.KernalExportNames
Export_CKernalDriver__Init:
		.byte "_CKernalDriver__Init", 0
Export_CKernalDriver__Init_axssy:
		.byte "_CKernalDriver__Init.axssy", 0
Export_CKernalDriver__Kill:
		.byte "_CKernalDriver__Kill", 0
Export_CKernalDriver__Kill_ax:
		.byte "_CKernalDriver__Kill.ax", 0
Export_CKernalDriver__NumUnits:
		.byte "_CKernalDriver__NumUnits", 0
Export_CKernalDriver__NumUnits_ax:
		.byte "_CKernalDriver__NumUnits.ax", 0
Export_CKernalDriver__Interrupt:
		.byte "_CKernalDriver__Interrupt", 0
Export_CKernalDriver__Interrupt_ax:
		.byte "_CKernalDriver__Interrupt.ax", 0
Export_CKernalDriver__Read:
		.byte "_CKernalDriver__Read", 0
Export_CKernalDriver__Read_axy:
		.byte "_CKernalDriver__Read.axy", 0
Export_CKernalDriver__Write:
		.byte "_CKernalDriver__Write", 0
Export_CKernalDriver__Write_axy:
		.byte "_CKernalDriver__Write.axy", 0
Export_CKernalDriver__Control:
		.byte "_CKernalDriver__Control", 0
Export_CKernalDriver__Control_axy:
		.byte "_CKernalDriver__Control.axy", 0
Export_CKernalDriver__Wait:
		.byte "_CKernalDriver__Wait", 0
Export_CKernalDriver__Wait_axy:
		.byte "_CKernalDriver__Wait.axy", 0
Export_CKernalDriver__Blit:
		.byte "_CKernalDriver__Blit", 0
Export_CKernalDriver__SetInterrupt:
		.byte "_CKernalDriver__SetInterrupt", 0
Export_CKernalDriver__SetInterrupt_axssy:
		.byte "_CKernalDriver__SetInterrupt.axssy", 0
Export_CKernalDriver__GetDevice:
		.byte "_CKernalDriver__GetDevice", 0
Export_CKernalDriver__GetDevice_ax:
		.byte "_CKernalDriver__GetDevice.ax", 0
Export_CKernalDriver__GetSlot:
		.byte "_CKernalDriver__GetSlot", 0
Export_CKernalDriver__GetSlot_ax:
		.byte "_CKernalDriver__GetSlot.ax", 0
Export_CKernalDriver__GetRegisters:
		.byte "_CKernalDriver__GetRegisters", 0
Export_CKernalDriver__GetRegisters_ax:
		.byte "_CKernalDriver__GetRegisters.ax", 0
Export_CKernalDriver__OpenUnit:
		.byte "_CKernalDriver__OpenUnit", 0
Export_CKernalDriver__OpenUnit_axy:
		.byte "_CKernalDriver__OpenUnit.axy", 0
Export_CKernalDriver__CloseUnit:
		.byte "_CKernalDriver__CloseUnit", 0
Export_CKernalDriver__CloseUnit_axy:
		.byte "_CKernalDriver__CloseUnit.axy", 0
Export_CKernalDriver__IsWaitOver:
		.byte "_CKernalDriver__IsWaitOver", 0
Export_CKernalDriver__IsWaitOver_axy:
		.byte "_CKernalDriver__IsWaitOver.axy", 0
Export_CKernalDriver__WaitFor:
		.byte "_CKernalDriver__WaitFor", 0
Export_CKernalDriver__WaitFor_axy:
		.byte "_CKernalDriver__WaitFor.axy", 0
Export_gasKernalDevices:
		.byte "_gasKernalDevices", 0
Export_KernalReadRegister__Fh:
		.byte "_KernalReadRegister__Fh", 0
Export_KernalReadRegister__Fh_a:
		.byte "_KernalReadRegister__Fh.a", 0
Export_KernalWriteRegister__Fhh:
		.byte "_KernalWriteRegister__Fhh", 0
Export_KernalWriteRegister__Fhh_ax:
		.byte "_KernalWriteRegister__Fhh.ax", 0
Export_KernalEnableDevice__Fj:
		.byte "_KernalEnableDevice__Fj", 0
Export_KernalEnableDevice__Fj_a:
		.byte "_KernalEnableDevice__Fj.a", 0
Export_KernalDisableDevice__Fj:
		.byte "_KernalDisableDevice__Fj", 0
Export_KernalDisableDevice__Fj_a:
		.byte "_KernalDisableDevice__Fj.a", 0
Export_KernalGetDriver__Fj:
		.byte "_KernalGetDriver__Fj", 0
Export_KernalGetDriver__Fj_a:
		.byte "_KernalGetDriver__Fj.a", 0
Export_KernalFindDriver__FPKcPKcj:
		.byte "_KernalFindDriver__FPKcPKcj", 0
Export_KernalFindDriver__FPKcPKcj_axssy:
		.byte "_KernalFindDriver__FPKcPKcj.axssy", 0
Export_KernalSetInterruptHandler__FjPFvPvEPvPVhb:
		.byte "_KernalSetInterruptHandler__FjPFvPvEPvPVhb", 0
Export_KernalSetInterruptHandler__FjPFvPvEPvPVhb_axy:
		.byte "_KernalSetInterruptHandler__FjPFvPvEPvPVhb.axy", 0
Export_KernalClearInterruptHandler__Fj:
		.byte "_KernalClearInterruptHandler__Fj", 0
Export_KernalClearInterruptHandler__Fj_a:
		.byte "_KernalClearInterruptHandler__Fj.a", 0
Export_KernalTicks:
		.byte "_KernalTicks", 0
Export_KernalWaitForInterrupt:
		.byte "_KernalWaitForInterrupt", 0
Export_KernalRunPendingInterrupts:
		.byte "_KernalRunPendingInterrupts", 0
Export_KernalInterruptsOn:
		.byte "_KernalInterruptsOn", 0
Export_KernalInterruptsOff:
		.byte "_KernalInterruptsOff", 0
Export_gbKernalMainBusBusy:
		.byte "_gbKernalMainBusBusy", 0
Export_guiKernalTicks:
		.byte "_guiKernalTicks", 0
Export_KernalUserPointer__FmPm:
		.byte "_KernalUserPointer__FmPm", 0
Export_KernalUserPointer__FmPm_ax:
		.byte "_KernalUserPointer__FmPm.ax", 0
Export_KernalCopyToUser__FmPKhm:
		.byte "_KernalCopyToUser__FmPKhm", 0
Export_KernalCopyToUser__FmPKhm_ax:
		.byte "_KernalCopyToUser__FmPKhm.ax", 0
Export_KernalCopyFromUser__FPhmm:
		.byte "_KernalCopyFromUser__FPhmm", 0
Export_KernalCopyFromUser__FPhmm_ax:
		.byte "_KernalCopyFromUser__FPhmm.ax", 0
Export_KernalMainMalloc__Fmb:
		.byte "_KernalMainMalloc__Fmb", 0
Export_KernalMainMalloc__Fmb_axy:
		.byte "_KernalMainMalloc__Fmb.axy", 0
Export_KernalMainFree__FPv:
		.byte "_KernalMainFree__FPv", 0
Export_KernalMainFree__FPv_ax:
		.byte "_KernalMainFree__FPv.ax", 0
Export_gpuiKernalBlitter:
		.byte "_gpuiKernalBlitter", 0
Export_gpuiKernalGraphics:
		.byte "_gpuiKernalGraphics", 0
Export_KernalMainCopy__FPhPKhjj_axssy:
		.byte "_KernalMainCopy__FPhPKhjj.axssy", 0
