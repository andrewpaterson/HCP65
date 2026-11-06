#include <string.h>
#include "KernalInterrupts.h"
#include "KernalDriver.h"


//The driver of each slot's board, NULL if it has none.
static CKernalDriver*	gapcKernalDrivers[KERNAL_NUM_DEVICE_SLOTS];


//////////////////////////////////////////////////////////////////////////
//
// The dispatch's handler for a driver's board: its Interrupt.
//
//////////////////////////////////////////////////////////////////////////
static void KernalDriverInterrupt(void* pvContext)
{
	((CKernalDriver*)pvContext)->Interrupt();
}


//////////////////////////////////////////////////////////////////////////
//
// Takes the board in uiSlot out of reset.  Returns KERNAL_DRIVER_SUCCESS
// or why the driver can't drive it.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalDriver::Init(SKernalDevice* psDevice, uint16 uiSlot)
{
	mpsDevice = psDevice;
	muiSlot = uiSlot;
	mpuiRegisters = (volatile uint8*)(psDevice->uiAddress + DEVICE_ROM_SIZE);
	muiOwner = KERNAL_DRIVER_NO_OWNER;
	KernalEnableDevice(uiSlot);
	return KERNAL_DRIVER_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CKernalDriver::Kill(void)
{
	KernalClearInterruptHandler(muiSlot);
	KernalDisableDevice(muiSlot);
	muiOwner = KERNAL_DRIVER_NO_OWNER;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalDriver::NumUnits(void)
{
	return 1;
}


//////////////////////////////////////////////////////////////////////////
//
// Only called for a driver that SetInterrupt: such a driver overrides it.
//
//////////////////////////////////////////////////////////////////////////
void CKernalDriver::Interrupt(void)
{
}


//////////////////////////////////////////////////////////////////////////
//
// uiProcess has the board until it closes it.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalDriver::Open(uint16 uiUnit, uint8 uiProcess)
{
	if (uiUnit >= NumUnits())
	{
		return KERNAL_DRIVER_BAD_UNIT;
	}
	if ((muiOwner != KERNAL_DRIVER_NO_OWNER) && (muiOwner != uiProcess))
	{
		return KERNAL_DRIVER_IN_USE;
	}
	muiOwner = uiProcess;
	return KERNAL_DRIVER_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////
//
// uiProcess lets the board go, if it has it.
//
//////////////////////////////////////////////////////////////////////////
void CKernalDriver::Close(uint8 uiProcess)
{
	if (muiOwner == uiProcess)
	{
		muiOwner = KERNAL_DRIVER_NO_OWNER;
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalDriver::Read(uint16 uiUnit, uint32 uiOffset, uint32 uiUserAddress, uint32 uiLength)
{
	return KERNAL_DRIVER_NOT_SUPPORTED;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalDriver::Write(uint16 uiUnit, uint32 uiOffset, uint32 uiUserAddress, uint32 uiLength)
{
	return KERNAL_DRIVER_NOT_SUPPORTED;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalDriver::Control(uint16 uiUnit, uint16 uiCode, uint32 uiValue, uint32* puiResult)
{
	return KERNAL_DRIVER_NOT_SUPPORTED;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalDriver::Wait(uint16 uiUnit, uint32 uiValue, uint16 uiTicks)
{
	return KERNAL_DRIVER_NOT_SUPPORTED;
}


//////////////////////////////////////////////////////////////////////////
//
// The board's interrupt goes to Interrupt.  puiFlags is its Flags &
// Interrupt board's Flags register, so the dispatch can quieten it (or
// NULL); bTrusted: Interrupt never touches main memory.
//
//////////////////////////////////////////////////////////////////////////
void CKernalDriver::SetInterrupt(volatile uint8* puiFlags, bool bTrusted)
{
	KernalSetInterruptHandler(muiSlot, KernalDriverInterrupt, (void*)this, puiFlags, bTrusted);
}


SKernalDevice* CKernalDriver::GetDevice(void) { return mpsDevice; }
uint16 CKernalDriver::GetSlot(void) { return muiSlot; }
volatile uint8* CKernalDriver::GetRegisters(void) { return mpuiRegisters; }
uint8 CKernalDriver::GetOwner(void) { return muiOwner; }


//////////////////////////////////////////////////////////////////////////
//
// pcDriver drives the present board in uiSlot.  Returns its Init's result;
// it is only the slot's driver if that is KERNAL_DRIVER_SUCCESS.
//
//////////////////////////////////////////////////////////////////////////
uint16 KernalStartDriver(CKernalDriver* pcDriver, uint16 uiSlot)
{
	SKernalDevice*	psDevice;
	uint16			uiResult;

	psDevice = &gasKernalDevices[uiSlot];
	if ((psDevice->uiState != KERNAL_DEVICE_PRESENT) || gapcKernalDrivers[uiSlot])
	{
		return KERNAL_DRIVER_BAD_UNIT;
	}
	uiResult = pcDriver->Init(psDevice, uiSlot);
	if (uiResult == KERNAL_DRIVER_SUCCESS)
	{
		gapcKernalDrivers[uiSlot] = pcDriver;
	}
	return uiResult;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
CKernalDriver* KernalGetDriver(uint16 uiSlot)
{
	return (uiSlot < KERNAL_NUM_DEVICE_SLOTS) ? gapcKernalDrivers[uiSlot] : NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The driver of the uiIndex'th board (by slot) made by szManufacturer
// called szName, NULL if there aren't that many with drivers.
//
//////////////////////////////////////////////////////////////////////////
CKernalDriver* KernalFindDriver(const char* szManufacturer, const char* szName, uint16 uiIndex)
{
	SKernalDeviceROM*	psROM;
	uint16				uiSlot;

	for (uiSlot = 0; uiSlot < KERNAL_NUM_DEVICE_SLOTS; uiSlot++)
	{
		if (gapcKernalDrivers[uiSlot])
		{
			psROM = gasKernalDevices[uiSlot].psROM;
			if ((strcmp(psROM->szName, szName) == 0) && (strcmp(psROM->szManufacturer, szManufacturer) == 0))
			{
				if (uiIndex == 0)
				{
					return gapcKernalDrivers[uiSlot];
				}
				uiIndex--;
			}
		}
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The boards uiProcess has open are let go (it has ended).
//
//////////////////////////////////////////////////////////////////////////
void KernalCloseDrivers(uint8 uiProcess)
{
	uint16	uiSlot;

	for (uiSlot = 0; uiSlot < KERNAL_NUM_DEVICE_SLOTS; uiSlot++)
	{
		if (gapcKernalDrivers[uiSlot] && (gapcKernalDrivers[uiSlot]->GetOwner() == uiProcess))
		{
			gapcKernalDrivers[uiSlot]->Close(uiProcess);
		}
	}
}
