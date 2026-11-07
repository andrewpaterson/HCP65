#ifndef __KERNAL_DRIVER_H__
#define __KERNAL_DRIVER_H__
#include "BaseLib/PrimitiveTypes.h"
#include "KernalDevices.h"


//What a driver's calls return.
#define KERNAL_DRIVER_SUCCESS			0
#define KERNAL_DRIVER_NOT_SUPPORTED		1		//The board doesn't do that.
#define KERNAL_DRIVER_BAD_UNIT			2
#define KERNAL_DRIVER_IN_USE			3		//Another process has the board open.
#define KERNAL_DRIVER_NOT_OPEN			4
#define KERNAL_DRIVER_BAD_ARGUMENT		5
#define KERNAL_DRIVER_FAILED			6
#define KERNAL_DRIVER_TIMEOUT			7

#define KERNAL_DRIVER_NO_OWNER			0		//Process IDs are $FC to $FF.


//The driver of the board in one device slot.  A board can have several units (the SD Card Board's four card
//sockets, the sound board's two sample RAMs), numbered from 0.
//
//Init takes the board out of reset; Kill clears its interrupt handler and holds it in reset again.  A driver whose
//board interrupts calls SetInterrupt in its Init and overrides Interrupt, which the dispatch calls (KernalInterrupts.h:
//with I set, on the kernal's stack; it quietens the board first).  A trusted Interrupt never touches main memory.
//
//Open, Close, Read, Write, Control and Wait are what a process asks of the board through a handle.  A process opens
//the whole board: while it has it open no other process can.  The others refuse (KERNAL_DRIVER_NOT_SUPPORTED) unless
//the driver does them.  Read and Write move up to uiLength bytes between the running process's memory at
//uiUserAddress and the unit at uiOffset, and set *puiDone to how many they moved; Control's codes are the driver's
//own; Wait waits up to uiTicks (KERNAL_TICKS_PER_SECOND) for what uiValue says.
class CKernalDriver
{
protected:
	SKernalDevice*	mpsDevice;
	uint16			muiSlot;
	volatile uint8*	mpuiRegisters;		//The slot's device registers ($400 in the slot).
	uint8			muiOwner;			//The process that has the board open, KERNAL_DRIVER_NO_OWNER if none.

public:
	virtual uint16	Init(SKernalDevice* psDevice, uint16 uiSlot);
	virtual void	Kill(void);

	virtual uint16	NumUnits(void);
	virtual void	Interrupt(void);

	virtual uint16	Open(uint16 uiUnit, uint8 uiProcess);
	virtual void	Close(uint8 uiProcess);
	virtual uint16	Read(uint16 uiUnit, uint32 uiOffset, uint32 uiUserAddress, uint32 uiLength, uint32* puiDone);
	virtual uint16	Write(uint16 uiUnit, uint32 uiOffset, uint32 uiUserAddress, uint32 uiLength, uint32* puiDone);
	virtual uint16	Control(uint16 uiUnit, uint16 uiCode, uint32 uiValue, uint32* puiResult);
	virtual uint16	Wait(uint16 uiUnit, uint32 uiValue, uint16 uiTicks);

	SKernalDevice*		GetDevice(void);
	uint16				GetSlot(void);
	volatile uint8*		GetRegisters(void);
	uint8				GetOwner(void);

protected:
	void	SetInterrupt(volatile uint8* puiFlags, bool bTrusted);
};


uint16			KernalStartDriver(CKernalDriver* pcDriver, uint16 uiSlot);
CKernalDriver*	KernalGetDriver(uint16 uiSlot);
CKernalDriver*	KernalFindDriver(const char* szManufacturer, const char* szName, uint16 uiIndex);
void			KernalCloseDrivers(uint8 uiProcess);


#endif // __KERNAL_DRIVER_H__
