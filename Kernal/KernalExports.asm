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
		.long Export_KernalEnableDevice__Ft
		.long _KernalEnableDevice__Ft
		.long Export_KernalEnableDevice__Ft_a
		.long _KernalEnableDevice__Ft.a
		.long Export_KernalDisableDevice__Ft
		.long _KernalDisableDevice__Ft
		.long Export_KernalDisableDevice__Ft_a
		.long _KernalDisableDevice__Ft.a
		.long Export_KernalGetDriver__Ft
		.long _KernalGetDriver__Ft
		.long Export_KernalGetDriver__Ft_a
		.long _KernalGetDriver__Ft.a
		.long Export_KernalFindDriver__FPKcPKct
		.long _KernalFindDriver__FPKcPKct
		.long Export_KernalFindDriver__FPKcPKct_axssy
		.long _KernalFindDriver__FPKcPKct.axssy
		; Interrupts and time (KernalInterrupts.h).
		.long Export_KernalSetInterruptHandler__FtPFvPvEPvPVhb
		.long _KernalSetInterruptHandler__FtPFvPvEPvPVhb
		.long Export_KernalSetInterruptHandler__FtPFvPvEPvPVhb_axy
		.long _KernalSetInterruptHandler__FtPFvPvEPvPVhb.axy
		.long Export_KernalClearInterruptHandler__Ft
		.long _KernalClearInterruptHandler__Ft
		.long Export_KernalClearInterruptHandler__Ft_a
		.long _KernalClearInterruptHandler__Ft.a
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
		.long Export_KernalUserPointer__FjPj
		.long _KernalUserPointer__FjPj
		.long Export_KernalUserPointer__FjPj_ax
		.long _KernalUserPointer__FjPj.ax
		.long Export_KernalCopyToUser__FjPKhj
		.long _KernalCopyToUser__FjPKhj
		.long Export_KernalCopyToUser__FjPKhj_ax
		.long _KernalCopyToUser__FjPKhj.ax
		.long Export_KernalCopyFromUser__FPhjj
		.long _KernalCopyFromUser__FPhjj
		.long Export_KernalCopyFromUser__FPhjj_ax
		.long _KernalCopyFromUser__FPhjj.ax
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
Export_KernalEnableDevice__Ft:
		.byte "_KernalEnableDevice__Ft", 0
Export_KernalEnableDevice__Ft_a:
		.byte "_KernalEnableDevice__Ft.a", 0
Export_KernalDisableDevice__Ft:
		.byte "_KernalDisableDevice__Ft", 0
Export_KernalDisableDevice__Ft_a:
		.byte "_KernalDisableDevice__Ft.a", 0
Export_KernalGetDriver__Ft:
		.byte "_KernalGetDriver__Ft", 0
Export_KernalGetDriver__Ft_a:
		.byte "_KernalGetDriver__Ft.a", 0
Export_KernalFindDriver__FPKcPKct:
		.byte "_KernalFindDriver__FPKcPKct", 0
Export_KernalFindDriver__FPKcPKct_axssy:
		.byte "_KernalFindDriver__FPKcPKct.axssy", 0
Export_KernalSetInterruptHandler__FtPFvPvEPvPVhb:
		.byte "_KernalSetInterruptHandler__FtPFvPvEPvPVhb", 0
Export_KernalSetInterruptHandler__FtPFvPvEPvPVhb_axy:
		.byte "_KernalSetInterruptHandler__FtPFvPvEPvPVhb.axy", 0
Export_KernalClearInterruptHandler__Ft:
		.byte "_KernalClearInterruptHandler__Ft", 0
Export_KernalClearInterruptHandler__Ft_a:
		.byte "_KernalClearInterruptHandler__Ft.a", 0
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
Export_KernalUserPointer__FjPj:
		.byte "_KernalUserPointer__FjPj", 0
Export_KernalUserPointer__FjPj_ax:
		.byte "_KernalUserPointer__FjPj.ax", 0
Export_KernalCopyToUser__FjPKhj:
		.byte "_KernalCopyToUser__FjPKhj", 0
Export_KernalCopyToUser__FjPKhj_ax:
		.byte "_KernalCopyToUser__FjPKhj.ax", 0
Export_KernalCopyFromUser__FPhjj:
		.byte "_KernalCopyFromUser__FPhjj", 0
Export_KernalCopyFromUser__FPhjj_ax:
		.byte "_KernalCopyFromUser__FPhjj.ax", 0
