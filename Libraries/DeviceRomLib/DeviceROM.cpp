#include <string.h>
#include "DeviceROM.h"


//////////////////////////////////////////////////////////////////////////
//
// Erased flash with the board's description and no registers.  Strings
// too long for their fields are cut short.
//
//////////////////////////////////////////////////////////////////////////
void CDeviceROM::Init(const char* szName, const char* szManufacturer, uint8 uiRevision, uint8 uiDeviceBus, uint8 uiMainBus, const char* szDriverFile)
{
	uint32	uiMagic;

	memset(mauiROM, 0xFF, DEVICE_ROM_SIZE);
	uiMagic = DEVICE_ROM_MAGIC;
	mauiROM[DEVICE_ROM_MAGIC_OFFSET + 0] = (uint8)uiMagic;
	mauiROM[DEVICE_ROM_MAGIC_OFFSET + 1] = (uint8)(uiMagic >> 8);
	mauiROM[DEVICE_ROM_MAGIC_OFFSET + 2] = (uint8)(uiMagic >> 16);
	mauiROM[DEVICE_ROM_MAGIC_OFFSET + 3] = (uint8)(uiMagic >> 24);
	mauiROM[DEVICE_ROM_FORMAT_OFFSET] = DEVICE_ROM_FORMAT;
	mauiROM[DEVICE_ROM_REVISION_OFFSET] = uiRevision;
	mauiROM[DEVICE_ROM_DEVICE_BUS_OFFSET] = uiDeviceBus;
	mauiROM[DEVICE_ROM_MAIN_BUS_OFFSET] = uiMainBus;
	SetString(DEVICE_ROM_NAME_OFFSET, DEVICE_ROM_NAME_LENGTH, szName);
	SetString(DEVICE_ROM_MAKER_OFFSET, DEVICE_ROM_MAKER_LENGTH, szManufacturer);
	SetString(DEVICE_ROM_DRIVER_OFFSET, DEVICE_ROM_DRIVER_LENGTH, szDriverFile);
	mauiROM[DEVICE_ROM_COUNT_OFFSET] = 0;
	mauiROM[DEVICE_ROM_COUNT_OFFSET + 1] = 0;
	SetChecksum();
}


//////////////////////////////////////////////////////////////////////////
//
// Lists a register: uiOffset in the slot, DEVICE_REGISTER_READ or
// DEVICE_REGISTER_WRITE.  It is put in order.  False if the list is full
// or it is listed already.
//
//////////////////////////////////////////////////////////////////////////
bool CDeviceROM::AddRegister(uint16 uiOffset, uint8 uiDirection)
{
	uint16	uiCount;
	uint16	i;
	uint32	uiKey;
	uint32	uiEntryKey;
	uint8*	puiEntry;

	uiCount = NumRegisters();
	if (uiCount >= DEVICE_ROM_MAX_REGISTERS)
	{
		return false;
	}

	uiKey = ((uint32)uiOffset << 8) | uiDirection;
	for (i = 0; i < uiCount; i++)
	{
		puiEntry = &mauiROM[DEVICE_ROM_REGISTERS_OFFSET + i * DEVICE_ROM_REGISTER_SIZE];
		uiEntryKey = ((uint32)(puiEntry[0] | (puiEntry[1] << 8)) << 8) | puiEntry[2];
		if (uiEntryKey == uiKey)
		{
			return false;
		}
		if (uiEntryKey > uiKey)
		{
			break;
		}
	}

	puiEntry = &mauiROM[DEVICE_ROM_REGISTERS_OFFSET + i * DEVICE_ROM_REGISTER_SIZE];
	memmove(puiEntry + DEVICE_ROM_REGISTER_SIZE, puiEntry, (uiCount - i) * DEVICE_ROM_REGISTER_SIZE);
	puiEntry[0] = (uint8)uiOffset;
	puiEntry[1] = (uint8)(uiOffset >> 8);
	puiEntry[2] = uiDirection;

	uiCount++;
	mauiROM[DEVICE_ROM_COUNT_OFFSET] = (uint8)uiCount;
	mauiROM[DEVICE_ROM_COUNT_OFFSET + 1] = (uint8)(uiCount >> 8);
	SetChecksum();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// uiCount registers from uiOffset, one byte apart.
//
//////////////////////////////////////////////////////////////////////////
void CDeviceROM::AddRegisters(uint16 uiOffset, uint16 uiCount, uint8 uiDirection)
{
	uint16	i;

	for (i = 0; i < uiCount; i++)
	{
		AddRegister((uint16)(uiOffset + i), uiDirection);
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint8 CDeviceROM::Read(uint32 uiOffset)
{
	return mauiROM[uiOffset & (DEVICE_ROM_SIZE - 1)];
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint16 CDeviceROM::NumRegisters(void)
{
	return (uint16)(mauiROM[DEVICE_ROM_COUNT_OFFSET] | (mauiROM[DEVICE_ROM_COUNT_OFFSET + 1] << 8));
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint8* CDeviceROM::GetROM(void)
{
	return mauiROM;
}


//////////////////////////////////////////////////////////////////////////
//
// Zero padded, with at least one zero.
//
//////////////////////////////////////////////////////////////////////////
void CDeviceROM::SetString(uint32 uiOffset, uint32 uiLength, const char* sz)
{
	size	uiStringLength;

	memset(&mauiROM[uiOffset], 0, uiLength);
	uiStringLength = sz ? strlen(sz) : 0;
	if (uiStringLength > uiLength - 1)
	{
		uiStringLength = uiLength - 1;
	}
	if (uiStringLength > 0)
	{
		memcpy(&mauiROM[uiOffset], sz, uiStringLength);
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CDeviceROM::SetChecksum(void)
{
	uint32	i;
	uint8	uiSum;

	uiSum = 0;
	for (i = 0; i < DEVICE_ROM_CHECKSUM_OFFSET; i++)
	{
		uiSum = (uint8)(uiSum + mauiROM[i]);
	}
	mauiROM[DEVICE_ROM_CHECKSUM_OFFSET] = (uint8)(0 - uiSum);
}

