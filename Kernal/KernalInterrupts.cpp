#include "KernalDevices.h"
#include "KernalInterrupts.h"


#define KERNAL_REGISTER_TIMER_SPEED		6
#define KERNAL_REGISTER_NMI_STATE		7
#define KERNAL_NMI_CLEAR_SOURCES		0x0F
#define KERNAL_NMI_ENABLE				0x10


SKernalInterrupt	gasKernalInterrupts[KERNAL_NUM_DEVICE_SLOTS];
volatile uint8		gbKernalMainBusBusy;
volatile uint32		guiKernalTicks;
volatile uint32		guiKernalInterrupts;

//Start.asm's KernalIrq and KernalNmi save registers here (without the stack).
extern "C" uint16	gauiKernalIrqSave[5];
extern "C" uint16	guiKernalNmiSave;
uint16				gauiKernalIrqSave[5];
uint16				guiKernalNmiSave;


//////////////////////////////////////////////////////////////////////////
//
// No handlers yet (the table is bss), and the NMI timer ticking every
// 3.90625ms: KernalNmi counts the ticks, and they end a wait.
//
//////////////////////////////////////////////////////////////////////////
void KernalInitInterrupts(void)
{
	gbKernalMainBusBusy = 0;
	KernalWriteRegister(KERNAL_REGISTER_TIMER_SPEED, KERNAL_TICK_SPEED);
	KernalWriteRegister(KERNAL_REGISTER_NMI_STATE, KERNAL_NMI_CLEAR_SOURCES | KERNAL_NMI_ENABLE);
}


//////////////////////////////////////////////////////////////////////////
//
// The device in uiSlot interrupts through fHandler (given pvContext).
// puiFlags is its Flags register (Flags & Interrupt board), or NULL if it
// hasn't one (then its handler must be trusted).
//
//////////////////////////////////////////////////////////////////////////
void KernalSetInterruptHandler(uint16 uiSlot, KernalInterruptHandler fHandler, void* pvContext, volatile uint8* puiFlags, bool bTrusted)
{
	SKernalInterrupt*	psInterrupt;

	if (uiSlot >= KERNAL_NUM_DEVICE_SLOTS)
	{
		return;
	}
	KernalInterruptsOff();
	psInterrupt = &gasKernalInterrupts[uiSlot];
	psInterrupt->pvContext = pvContext;
	psInterrupt->puiFlags = puiFlags;
	psInterrupt->uiTrusted = bTrusted ? 1 : 0;
	psInterrupt->uiPending = 0;
	psInterrupt->fHandler = fHandler;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void KernalClearInterruptHandler(uint16 uiSlot)
{
	if (uiSlot >= KERNAL_NUM_DEVICE_SLOTS)
	{
		return;
	}
	KernalInterruptsOff();
	gasKernalInterrupts[uiSlot].fHandler = NULL;
	gasKernalInterrupts[uiSlot].uiPending = 0;
}


//////////////////////////////////////////////////////////////////////////
//
// The tick count, read until two reads agree (an NMI can count it between
// its two words).
//
//////////////////////////////////////////////////////////////////////////
uint32 KernalTicks(void)
{
	uint32	uiTicks;

	do
	{
		uiTicks = guiKernalTicks;
	}
	while (uiTicks != guiKernalTicks);
	return uiTicks;
}
