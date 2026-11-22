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
#define KERNAL_DRIVER_NOT_FOUND			8		//A board it needs isn't there (a scaled blit's Graphics Board).

struct SKernalBlit;


//An image the kernal has a handle to (KernalScreen.h): uiWidth by uiHeight pixels in the running process's memory at
//uiPixels, a row after another, drawn without its 0s if uiFlags has KERNAL_IMAGE_TRANSPARENT (UserLib/HCP65User.h).
struct SKernalImage
{
	uint8	uiProcess;		//The process whose it is, KERNAL_DRIVER_NO_OWNER if the entry is free.
	uint8	uiPalette;		//The palette its pixels are drawn with, KERNAL_SCREEN_NO_PALETTE for the one set.
	uint16	uiGeneration;	//Counted each time the entry is used again: a handle is its index and this.
	uint32	uiPixels;
	uint16	uiWidth;
	uint16	uiHeight;
	uint16	uiFlags;
};


#define KERNAL_DRIVER_NO_OWNER			0		//Process IDs are $FC to $FF.
#define KERNAL_DRIVER_ALL_UNITS			0xFFFF	//Close's unit when the process has ended.
#define KERNAL_DRIVER_MAX_UNITS			8		//NumUnits is at most this: the VIA Board's eight VIAs.


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
//which a driver can do with WaitFor and IsWaitOver.  Blit draws an image, which is in the running process's memory
//(UserLib/HCP65User.h's SKernalBlit), on a unit that is a picture: the screen's calls ask it (KernalScreen.h).
//BlitImage draws an image the kernal has a handle to (KERNAL_BLIT_IMAGE) as Blit draws an SFT: psBlit's position,
//clip and spans, psImage's pixels.
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

	virtual uint16	NumUnits(void) const;
	virtual void	Interrupt(void);

	uint16			Open(uint16 uiUnit, uint8 uiProcess);
	void			Close(uint16 uiUnit, uint8 uiProcess);
	bool			IsOpen(void);
	virtual uint16	Read(uint16 uiUnit, uint32 uiOffset, uint32 uiUserAddress, uint32 uiLength, uint32* puiDone);
	virtual uint16	Write(uint16 uiUnit, uint32 uiOffset, uint32 uiUserAddress, uint32 uiLength, uint32* puiDone);
	virtual uint16	Control(uint16 uiUnit, uint16 uiCode, uint32 uiValue, uint32* puiResult);
	virtual uint16	Wait(uint16 uiUnit, uint32 uiValue, uint16 uiTicks);
	virtual uint16	Blit(uint16 uiUnit, const SKernalBlit* psBlit);
	virtual uint16	BlitImage(uint16 uiUnit, const SKernalBlit* psBlit, const SKernalImage* psImage);

	SKernalDevice*		GetDevice(void) const;
	uint16				GetSlot(void) const;
	volatile uint8*		GetRegisters(void) const;

protected:
	virtual uint16	OpenUnit(uint16 uiUnit);
	virtual void	CloseUnit(uint16 uiUnit);
	virtual bool	IsWaitOver(uint16 uiUnit, uint32 uiValue);

	void	SetInterrupt(volatile uint8* puiFlags, bool bTrusted);
	uint16	WaitFor(uint16 uiUnit, uint32 uiValue, uint16 uiTicks);
	bool	IsOpenByOther(uint8 uiProcess);
	void	LetGo(uint16 uiUnit);
};


//A copy in main memory (kernel mode's addresses of it, which are where it really is): uiCount bytes from puiSource to
//puiDest, lowest first, without those that are uiTransparent unless it is KERNAL_COPY_OPAQUE.  A Blitter Board makes it
//if there is one and the copy is long enough for that to be quicker, the processor if not (KernalCopy.asm).  The Video
//Board's driver draws a blit's runs with it.  Neither end may be in MPU RAM, which the board can't reach.  It can't be
//called through a pointer.
#define KERNAL_COPY_OPAQUE				0xFFFF

void			KernalMainCopy(uint8* puiDest, const uint8* puiSource, uint16 uiCount, uint16 uiTransparent);

//The Blitter Board's registers while its driver is loaded, NULL when it isn't: its driver sets it.  The kernal's copies
//and a driver that gives the board copies itself (the Video Board's text) write them; each sets all that its copy needs
//but the count's start, which stays 0.
extern "C" volatile uint8*	gpuiKernalBlitter;

//The Graphics Board's registers while its driver is loaded, NULL when it isn't: its driver sets it.  Its offsets are
//device RAM in the same slot, from the registers: each count's low byte at KERNAL_GRAPHICS_OFFSETS_LOW, its high byte at
//KERNAL_GRAPHICS_OFFSETS_HIGH, written only while no copy is under way.
extern "C" volatile uint8*	gpuiKernalGraphics;
#define KERNAL_GRAPHICS_OFFSETS_LOW		0x07C00		//$08000 in the slot.
#define KERNAL_GRAPHICS_OFFSETS_HIGH	0x0FC00		//$10000 in the slot.
#define KERNAL_GRAPHICS_COUNTS			0x8000		//The count is 15 bits: the offsets there are.

//A Graphics Board copy in main memory (kernel mode's addresses): uiCount bytes to puiDest, each from puiSource plus the
//offset of its count, the counts from uiStart (wrapping at KERNAL_GRAPHICS_COUNTS), without those that are
//uiTransparent unless it is KERNAL_COPY_OPAQUE.  gpuiKernalGraphics mustn't be NULL; the offsets are the caller's to
//write.  It can't be called through a pointer (KernalCopy.asm).
void			KernalMainScaledCopy(uint8* puiDest, const uint8* puiSource, uint16 uiStart, uint16 uiCount, uint16 uiTransparent);

//Rows of a scaled blit, each a KernalMainScaledCopy of uiCount bytes from count uiStart to puiDest, which then moves on
//uiDestStride, while there are uiRows and the row's uiOffset is from uiLow to uiHigh: its source is puiBase plus
//uiOffset.  Each row adds uiStep to uiRemainder, and while that is uiDenominator or more it loses uiDenominator and
//uiOffset gains iRowStep.  KernalMainScaledRows leaves the block where it stopped: uiRows is 0, or the row at uiOffset
//isn't in the range (another bank's, for the caller to find).  uiStop is its own.  Every member is a whole number of
//words, so the assembly's offsets are the compiler's (KernalCopy.asm).
struct SKernalScaledRows
{
	uint8*	puiDest;		//+0
	uint8*	puiBase;		//+4
	uint32	uiOffset;		//+8
	uint32	uiLow;			//+12
	uint32	uiHigh;			//+16
	int32	iRowStep;		//+20
	uint32	uiRemainder;	//+24
	uint32	uiStep;			//+28
	uint32	uiDenominator;	//+32
	uint16	uiDestStride;	//+36
	uint16	uiRows;			//+38
	uint16	uiStart;		//+40
	uint16	uiCount;		//+42
	uint16	uiTransparent;	//+44
	uint16	uiStop;			//+46
};

void			KernalMainScaledRows(SKernalScaledRows* psRows);

uint16			KernalStartDriver(CKernalDriver* pcDriver, uint16 uiSlot);
CKernalDriver*	KernalGetDriver(uint16 uiSlot);
CKernalDriver*	KernalFindDriver(const char* szManufacturer, const char* szName, uint16 uiIndex);
void			KernalCloseDrivers(uint8 uiProcess);


#endif // __KERNAL_DRIVER_H__
