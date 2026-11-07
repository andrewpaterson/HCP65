#ifndef __KERNAL_DRIVER_H__
#define __KERNAL_DRIVER_H__
#include "BaseLib/PrimitiveTypes.h"
#include "KernalDevices.h"


//What a driver's calls return.
#define KERNAL_DRIVER_SUCCESS			0
#define KERNAL_DRIVER_NOT_SUPPORTED		1		//The board doesn't do that.
#define KERNAL_DRIVER_BAD_UNIT			2
#define KERNAL_DRIVER_IN_USE			3		//Another process has the board (or the unit) open.
#define KERNAL_DRIVER_NOT_OPEN			4
#define KERNAL_DRIVER_BAD_ARGUMENT		5
#define KERNAL_DRIVER_FAILED			6
#define KERNAL_DRIVER_TIMEOUT			7

#define KERNAL_DRIVER_NO_OWNER			0		//Process IDs are $FC to $FF.
#define KERNAL_DRIVER_ALL_UNITS			0xFFFF	//Close's unit when the process has ended.
#define KERNAL_DRIVER_MAX_UNITS			4		//NumUnits is at most this.


//The driver of the board in one device slot.  A board can have several units (the SD Card Board's four card
//sockets, the sound board's two sample RAMs), numbered from 0.
//
//Init takes the board out of reset; Kill clears its interrupt handler and holds it in reset again.  A driver whose
//board interrupts calls SetInterrupt in its Init and overrides Interrupt, which the dispatch calls (KernalInterrupts.h:
//with I set, on the kernal's stack; it quietens the board first).  A trusted Interrupt never touches main memory.
//
//Open, Close, Read, Write, Control and Wait are what a process asks of the board through a handle.  Open is called for
//each handle; Close when the process's last handle on the unit is closed, and with KERNAL_DRIVER_ALL_UNITS when the
//process has ended.  Each unit has the process that opened it.  Unless the driver's Init sets mbSeparateUnits (the UART
//Board's two ports) a process has the whole board: while it has any unit open no other process can open one.  OpenUnit
//is called when a unit is opened by a process that hadn't it open, and can refuse; CloseUnit when it is let go, and
//can sleep (WaitFor: the UART Board's waits up to a second for what is queued to go), so closing a handle or a
//process ending (KernalCloseDrivers) can take that long.  Read, Write, Control and Wait refuse
//(KERNAL_DRIVER_NOT_SUPPORTED) unless the driver does them.  Read and Write move up to uiLength bytes between the
//running process's memory at uiUserAddress and the unit at uiOffset, and set *puiDone to how many they moved;
//Control's codes are the driver's own; Wait waits up to uiTicks (KERNAL_TICKS_PER_SECOND) for what uiValue says,
//which a driver can do with WaitFor and IsWaitOver.
class CKernalDriver
{
protected:
	SKernalDevice*	mpsDevice;
	uint16			muiSlot;
	volatile uint8*	mpuiRegisters;							//The slot's device registers ($400 in the slot).
	bool			mbSeparateUnits;						//Each unit can be open by a different process.
	uint8			mauiOwners[KERNAL_DRIVER_MAX_UNITS];	//The process that has each unit open, KERNAL_DRIVER_NO_OWNER if none.

public:
	virtual uint16	Init(SKernalDevice* psDevice, uint16 uiSlot);
	virtual void	Kill(void);

	virtual uint16	NumUnits(void);
	virtual void	Interrupt(void);

	uint16			Open(uint16 uiUnit, uint8 uiProcess);
	void			Close(uint16 uiUnit, uint8 uiProcess);
	bool			IsOpen(void);
	virtual uint16	Read(uint16 uiUnit, uint32 uiOffset, uint32 uiUserAddress, uint32 uiLength, uint32* puiDone);
	virtual uint16	Write(uint16 uiUnit, uint32 uiOffset, uint32 uiUserAddress, uint32 uiLength, uint32* puiDone);
	virtual uint16	Control(uint16 uiUnit, uint16 uiCode, uint32 uiValue, uint32* puiResult);
	virtual uint16	Wait(uint16 uiUnit, uint32 uiValue, uint16 uiTicks);

	SKernalDevice*		GetDevice(void);
	uint16				GetSlot(void);
	volatile uint8*		GetRegisters(void);

protected:
	virtual uint16	OpenUnit(uint16 uiUnit);
	virtual void	CloseUnit(uint16 uiUnit);
	virtual bool	IsWaitOver(uint16 uiUnit, uint32 uiValue);

	void	SetInterrupt(volatile uint8* puiFlags, bool bTrusted);
	uint16	WaitFor(uint16 uiUnit, uint32 uiValue, uint16 uiTicks);
	bool	IsOpenByOther(uint8 uiProcess);
	void	LetGo(uint16 uiUnit);
};


uint16			KernalStartDriver(CKernalDriver* pcDriver, uint16 uiSlot);
CKernalDriver*	KernalGetDriver(uint16 uiSlot);
CKernalDriver*	KernalFindDriver(const char* szManufacturer, const char* szName, uint16 uiIndex);
void			KernalCloseDrivers(uint8 uiProcess);


#endif // __KERNAL_DRIVER_H__
