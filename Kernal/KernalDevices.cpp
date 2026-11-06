#include <string.h>
#include "KernalDevices.h"


SKernalDevice	gasKernalDevices[KERNAL_NUM_DEVICE_SLOTS];


//////////////////////////////////////////////////////////////////////////
//
// Kernel mode addresses: devices 1 to 7 are 32KB from $108000, devices 16
// to 19 are 128KB from $180000.
//
//////////////////////////////////////////////////////////////////////////
static void KernalSlotAddress(uint16 uiSlot, uint32* puiAddress, uint32* puiSize)
{
	if ((uiSlot >= 1) && (uiSlot <= 7))
	{
		*puiAddress = 0x100000 + ((uint32)uiSlot << 15);
		*puiSize = KERNAL_SMALL_SLOT_SIZE;
	}
	else if ((uiSlot >= 16) && (uiSlot <= 19))
	{
		*puiAddress = 0x180000 + ((uint32)(uiSlot - 16) << 17);
		*puiSize = KERNAL_LARGE_SLOT_SIZE;
	}
	else
	{
		*puiAddress = 0;
		*puiSize = 0;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// What the device ROM at the start of psDevice's slot says is there
// (KERNAL_DEVICE_*).  The whole 1KB is summed for the checksum.
//
//////////////////////////////////////////////////////////////////////////
static uint16 KernalCheckDeviceROM(SKernalDevice* psDevice)
{
	SKernalDeviceROM*	psROM;
	uint8*				puiROM;
	uint16				i;
	uint8				uiSum;

	psROM = (SKernalDeviceROM*)psDevice->uiAddress;
	if (psROM->uiMagic == KERNAL_DEVICE_ROM_EMPTY)
	{
		return KERNAL_DEVICE_NONE;
	}
	if ((psROM->uiMagic != DEVICE_ROM_MAGIC) || (psROM->uiFormat != DEVICE_ROM_FORMAT))
	{
		return KERNAL_DEVICE_UNRECOGNISED;
	}

	puiROM = (uint8*)psROM;
	uiSum = 0;
	for (i = 0; i < DEVICE_ROM_SIZE; i++)
	{
		uiSum = (uint8)(uiSum + puiROM[i]);
	}
	if (uiSum != 0)
	{
		return KERNAL_DEVICE_UNRECOGNISED;
	}

	if ((psROM->szName[0] == 0) ||
		(psROM->szName[DEVICE_ROM_NAME_LENGTH - 1] != 0) ||
		(psROM->szManufacturer[DEVICE_ROM_MAKER_LENGTH - 1] != 0) ||
		(psROM->szDriverFile[DEVICE_ROM_DRIVER_LENGTH - 1] != 0) ||
		(psROM->uiNumRegisters > DEVICE_ROM_MAX_REGISTERS) ||
		(psROM->uiDeviceBus > DEVICE_BUS_128KB))
	{
		return KERNAL_DEVICE_UNRECOGNISED;
	}

	if ((psROM->uiDeviceBus == DEVICE_BUS_128KB) && (psDevice->uiSize < KERNAL_LARGE_SLOT_SIZE))
	{
		return KERNAL_DEVICE_WRONG_SOCKET;
	}
	return KERNAL_DEVICE_PRESENT;
}


//////////////////////////////////////////////////////////////////////////
//
// Reads each socket's device ROM.  Devices stay held in reset: only a
// present device's driver enables it.
//
//////////////////////////////////////////////////////////////////////////
void KernalScanDevices(void)
{
	SKernalDevice*		psDevice;
	uint16				uiSlot;

	for (uiSlot = 0; uiSlot < KERNAL_NUM_DEVICE_SLOTS; uiSlot++)
	{
		psDevice = &gasKernalDevices[uiSlot];
		KernalSlotAddress(uiSlot, &psDevice->uiAddress, &psDevice->uiSize);
		psDevice->uiState = KERNAL_DEVICE_NONE;
		psDevice->psROM = NULL;
		if (psDevice->uiAddress != 0)
		{
			psDevice->uiState = KernalCheckDeviceROM(psDevice);
			if (psDevice->uiState == KERNAL_DEVICE_PRESENT)
			{
				psDevice->psROM = (SKernalDeviceROM*)psDevice->uiAddress;
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The first present device made by szManufacturer called szName, or NULL.
//
//////////////////////////////////////////////////////////////////////////
SKernalDevice* KernalFindDevice(const char* szManufacturer, const char* szName)
{
	SKernalDevice*	psDevice;
	uint16			uiSlot;

	for (uiSlot = 0; uiSlot < KERNAL_NUM_DEVICE_SLOTS; uiSlot++)
	{
		psDevice = &gasKernalDevices[uiSlot];
		if (psDevice->psROM && (strcmp(psDevice->psROM->szName, szName) == 0) && (strcmp(psDevice->psROM->szManufacturer, szManufacturer) == 0))
		{
			return psDevice;
		}
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The processor board's registers (device slot 0).
//
//////////////////////////////////////////////////////////////////////////
uint8 KernalReadRegister(uint8 uiRegister)
{
	return ((volatile uint8*)KERNAL_REGISTERS)[uiRegister];
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void KernalWriteRegister(uint8 uiRegister, uint8 uiValue)
{
	((volatile uint8*)KERNAL_REGISTERS)[uiRegister] = uiValue;
}


//////////////////////////////////////////////////////////////////////////
//
// Lets the device in uiSlot run (the processor board holds every device in
// reset until it is enabled).
//
//////////////////////////////////////////////////////////////////////////
void KernalEnableDevice(uint16 uiSlot)
{
	uint8	uiRegister;
	uint8	uiBit;

	if (uiSlot >= 16)
	{
		uiRegister = 5;
		uiBit = (uint8)(1 << (uiSlot - 16));
	}
	else
	{
		uiRegister = 3;
		uiBit = (uint8)(1 << uiSlot);
	}
	KernalWriteRegister(uiRegister, (uint8)(KernalReadRegister(uiRegister) | uiBit));
}
