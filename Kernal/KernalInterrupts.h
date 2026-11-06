#ifndef __KERNAL_INTERRUPTS_H__
#define __KERNAL_INTERRUPTS_H__
#include "BaseLib/PrimitiveTypes.h"
#include "KernalDevices.h"


//See KernalInterrupts.txt.  IRQ is low while any device's interrupt line is; KernalInterrupts.asm's dispatch reads which
//devices are interrupting (processor board registers 0 and 2) and calls each one's handler.
//
//A handler is called with I set, on the kernal's stack, and must quieten its device first (turn its interrupt enable off,
//or write its Clear Interrupt): IRQ is level triggered.  A trusted handler never touches main memory (no process
//memory, no heap, no multiply or divide: the compiler's runtime reads the tables in main memory), so it may run while
//the SD Card Board's DMA has the main memory bus (gbKernalMainBusBusy).  While the bus is busy an untrusted handler's
//device is quietened through its Flags register (Flags & Interrupt board: D7 written 0 turns its interrupt enable off)
//and the handler runs later, from KernalRunPendingInterrupts; the handler turns the device's interrupt enable back on.
//A device that interrupts with no handler is reset (its bit in register 3 or 5 written 0 then 1): a board with the
//Flags & Interrupt board comes back with Enable off, so it stops interrupting.  Its slot's bit is set in
//gsKernalStatus.uiStrayInterrupts.

typedef void (*KernalInterruptHandler)(void* pvContext);


//KernalInterrupts.asm reads these by offset: 16 bytes each.
struct SKernalInterrupt
{
	KernalInterruptHandler	fHandler;		//+0, NULL if the slot has none.
	void*					pvContext;		//+4
	volatile uint8*			puiFlags;		//+8, the device's Flags register (Flags & Interrupt board), or NULL.
	uint16					uiTrusted;		//+12, not 0: the handler never touches main memory.
	uint16					uiPending;		//+14, not 0: deferred while the main memory bus was busy.
};


extern "C" SKernalInterrupt		gasKernalInterrupts[KERNAL_NUM_DEVICE_SLOTS];
extern "C" volatile uint8		gbKernalMainBusBusy;	//Not 0 while the SD Card Board's DMA has the main memory bus.
extern "C" volatile uint32		guiKernalTicks;			//NMI timer ticks (KERNAL_TICK_SPEED).
extern "C" volatile uint32		guiKernalInterrupts;	//Handlers called.


//Timer Speed register value: a tick (an NMI) every 2^(21 - 2 * 7) = 128 crystal ticks, 3.90625ms.
#define KERNAL_TICK_SPEED		7
#define KERNAL_TICKS_PER_SECOND	256


void				KernalInitInterrupts(void);
void				KernalSetInterruptHandler(uint16 uiSlot, KernalInterruptHandler fHandler, void* pvContext, volatile uint8* puiFlags, bool bTrusted);
void				KernalClearInterruptHandler(uint16 uiSlot);
uint32				KernalTicks(void);

extern "C" void		KernalWaitForInterrupt(void);		//With I set: waits for an IRQ or NMI, then lets a pending IRQ be dispatched.
extern "C" void		KernalRunPendingInterrupts(void);	//Runs the handlers deferred while the main memory bus was busy.
extern "C" void		KernalInterruptsOn(void);			//cli
extern "C" void		KernalInterruptsOff(void);			//sei


#endif // __KERNAL_INTERRUPTS_H__
