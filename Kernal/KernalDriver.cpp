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
	mbSeparateUnits = false;
	memset(mauiOwners, KERNAL_DRIVER_NO_OWNER, sizeof(mauiOwners));
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
	memset(mauiOwners, KERNAL_DRIVER_NO_OWNER, sizeof(mauiOwners));
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
// uiProcess has uiUnit until it has closed its last handle on it.
// Without mbSeparateUnits no other process can have one of the board's
// units meanwhile.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalDriver::Open(uint16 uiUnit, uint8 uiProcess)
{
	uint16	uiResult;

	if (uiUnit >= NumUnits())
	{
		return KERNAL_DRIVER_BAD_UNIT;
	}
	if (mauiOwners[uiUnit] == uiProcess)
	{
		return KERNAL_DRIVER_SUCCESS;
	}
	if ((mauiOwners[uiUnit] != KERNAL_DRIVER_NO_OWNER) || (!mbSeparateUnits && IsOpenByOther(uiProcess)))
	{
		return KERNAL_DRIVER_IN_USE;
	}
	uiResult = OpenUnit(uiUnit);
	if (uiResult == KERNAL_DRIVER_SUCCESS)
	{
		mauiOwners[uiUnit] = uiProcess;
	}
	return uiResult;
}


//////////////////////////////////////////////////////////////////////////
//
// uiProcess has closed its last handle on uiUnit (every unit it has open
// if KERNAL_DRIVER_ALL_UNITS), which is let go.
//
//////////////////////////////////////////////////////////////////////////
void CKernalDriver::Close(uint16 uiUnit, uint8 uiProcess)
{
	uint16	i;

	if (uiUnit != KERNAL_DRIVER_ALL_UNITS)
	{
		if ((uiUnit < KERNAL_DRIVER_MAX_UNITS) && (mauiOwners[uiUnit] == uiProcess))
		{
			LetGo(uiUnit);
		}
		return;
	}
	for (i = 0; i < KERNAL_DRIVER_MAX_UNITS; i++)
	{
		if (mauiOwners[i] == uiProcess)
		{
			LetGo(i);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// CloseUnit, then nobody has it.
//
//////////////////////////////////////////////////////////////////////////
void CKernalDriver::LetGo(uint16 uiUnit)
{
	CloseUnit(uiUnit);
	mauiOwners[uiUnit] = KERNAL_DRIVER_NO_OWNER;
}


//////////////////////////////////////////////////////////////////////////
//
// A process has a unit open (KernalDeviceInfo): one other than
// KERNAL_DRIVER_NO_OWNER, which no process is.
//
//////////////////////////////////////////////////////////////////////////
bool CKernalDriver::IsOpen(void)
{
	return IsOpenByOther(KERNAL_DRIVER_NO_OWNER);
}


//////////////////////////////////////////////////////////////////////////
//
// A process other than uiProcess has a unit open.
//
//////////////////////////////////////////////////////////////////////////
bool CKernalDriver::IsOpenByOther(uint8 uiProcess)
{
	uint16	i;

	for (i = 0; i < KERNAL_DRIVER_MAX_UNITS; i++)
	{
		if ((mauiOwners[i] != KERNAL_DRIVER_NO_OWNER) && (mauiOwners[i] != uiProcess))
		{
			return true;
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalDriver::Read(uint16 uiUnit, uint32 uiOffset, uint32 uiUserAddress, uint32 uiLength, uint32* puiDone)
{
	return KERNAL_DRIVER_NOT_SUPPORTED;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalDriver::Write(uint16 uiUnit, uint32 uiOffset, uint32 uiUserAddress, uint32 uiLength, uint32* puiDone)
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
// A process that hadn't uiUnit open is opening it: the driver gets it
// ready, or refuses (KERNAL_DRIVER_NOT_SUPPORTED for a board only the
// kernal uses).
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalDriver::OpenUnit(uint16 uiUnit)
{
	return KERNAL_DRIVER_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////
//
// uiUnit is being let go.
//
//////////////////////////////////////////////////////////////////////////
void CKernalDriver::CloseUnit(uint16 uiUnit)
{
}


//////////////////////////////////////////////////////////////////////////
//
// Whether what uiValue waits for on uiUnit has happened (WaitFor).
//
//////////////////////////////////////////////////////////////////////////
bool CKernalDriver::IsWaitOver(uint16 uiUnit, uint32 uiValue)
{
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Sleeps until IsWaitOver(uiUnit, uiValue), for up to uiTicks:
// KERNAL_DRIVER_TIMEOUT if it didn't happen.  The board's interrupts come
// in while it sleeps.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalDriver::WaitFor(uint16 uiUnit, uint32 uiValue, uint16 uiTicks)
{
	uint32	uiStart;

	uiStart = KernalTicks();
	while (!IsWaitOver(uiUnit, uiValue))
	{
		if ((KernalTicks() - uiStart) > uiTicks)
		{
			return KERNAL_DRIVER_TIMEOUT;
		}
		KernalWaitForInterrupt();
	}
	return KERNAL_DRIVER_SUCCESS;
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
// The units uiProcess has open are let go (it has ended).  A driver's
// CloseUnit can sleep, so this can take a while.
//
//////////////////////////////////////////////////////////////////////////
void KernalCloseDrivers(uint8 uiProcess)
{
	uint16	uiSlot;

	for (uiSlot = 0; uiSlot < KERNAL_NUM_DEVICE_SLOTS; uiSlot++)
	{
		if (gapcKernalDrivers[uiSlot])
		{
			gapcKernalDrivers[uiSlot]->Close(KERNAL_DRIVER_ALL_UNITS, uiProcess);
		}
	}
}
