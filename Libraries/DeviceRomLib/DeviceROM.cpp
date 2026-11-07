#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "BaseLib/Chars.h"
#include "BaseLib/MarkupTag.h"
#include "DeviceROM.h"


//MainBus's words, in the order they are written.
struct SDeviceROMMainBusFlag
{
	const char*	szWord;
	uint8		uiFlag;
};

#define DEVICE_ROM_NUM_MAIN_BUS_FLAGS	4

static const SDeviceROMMainBusFlag	gasDeviceROMMainBusFlags[DEVICE_ROM_NUM_MAIN_BUS_FLAGS] =
{
	{ "Master", DEVICE_MAIN_BUS_MASTER },
	{ "WindowA", DEVICE_MAIN_BUS_WINDOW_A },
	{ "WindowB", DEVICE_MAIN_BUS_WINDOW_B },
	{ "Private", DEVICE_MAIN_BUS_PRIVATE },
};


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


//////////////////////////////////////////////////////////////////////////
//
// Sets pszError to "<message><detail> (<tag> line <n>)" and returns false.
//
//////////////////////////////////////////////////////////////////////////
static bool DeviceROMError(CChars* pszError, CMarkupTag* pcTag, const char* szMessage, const char* szDetail)
{
	char	szLine[96];

	pszError->Set(szMessage);
	if (szDetail)
	{
		pszError->Append(szDetail);
	}
	snprintf(szLine, sizeof(szLine), " (<%s> line %lld)", pcTag->GetName(), (long long)pcTag->GetLine() + 1);
	pszError->Append(szLine);
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// The number in pcTag's szName attribute (decimal or 0x hexadecimal), up
// to uiMax; uiDefault if the attribute isn't there.
//
//////////////////////////////////////////////////////////////////////////
static bool DeviceROMNumber(CMarkupTag* pcTag, const char* szName, uint32 uiMax, uint32 uiDefault, uint32* puiValue, CChars* pszError)
{
	char*		szValue;
	char*		szEnd;
	long long	lli;

	szValue = pcTag->GetStringAttribute(szName);
	if (szValue == NULL)
	{
		*puiValue = uiDefault;
		return true;
	}
	lli = strtoll(szValue, &szEnd, 0);
	if ((*szValue == '\0') || (*szEnd != '\0') || (lli < 0) || (lli > (long long)uiMax))
	{
		return DeviceROMError(pszError, pcTag, "Bad number in ", szName);
	}
	*puiValue = (uint32)lli;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// A string attribute that fits a field of uiLength bytes with its zero;
// "" if it isn't there and isn't required.
//
//////////////////////////////////////////////////////////////////////////
static bool DeviceROMString(CMarkupTag* pcTag, const char* szName, uint32 uiLength, bool bRequired, const char** pszValue, CChars* pszError)
{
	char*	szValue;

	szValue = pcTag->GetStringAttribute(szName);
	if ((szValue == NULL) || (szValue[0] == '\0'))
	{
		if (bRequired)
		{
			return DeviceROMError(pszError, pcTag, "Missing ", szName);
		}
		*pszValue = "";
		return true;
	}
	if (strlen(szValue) > uiLength - 1)
	{
		return DeviceROMError(pszError, pcTag, "Too long: ", szName);
	}
	*pszValue = szValue;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The DEVICE_MAIN_BUS_* flags named in szMainBus, separated by spaces.
// False, with pszUnknown (if given) set to the word, if one isn't a flag's.
//
//////////////////////////////////////////////////////////////////////////
bool DeviceROMParseMainBus(const char* szMainBus, uint8* puiMainBus, CChars* pszUnknown)
{
	const char*	sz;
	size		uiLength;
	size		i;

	*puiMainBus = 0;
	sz = szMainBus;
	for (;;)
	{
		while (*sz == ' ')
		{
			sz++;
		}
		if (*sz == '\0')
		{
			return true;
		}
		uiLength = 0;
		while ((sz[uiLength] != '\0') && (sz[uiLength] != ' '))
		{
			uiLength++;
		}
		for (i = 0; i < DEVICE_ROM_NUM_MAIN_BUS_FLAGS; i++)
		{
			if ((strlen(gasDeviceROMMainBusFlags[i].szWord) == uiLength) && (memcmp(gasDeviceROMMainBusFlags[i].szWord, sz, uiLength) == 0))
			{
				*puiMainBus |= gasDeviceROMMainBusFlags[i].uiFlag;
				break;
			}
		}
		if (i == DEVICE_ROM_NUM_MAIN_BUS_FLAGS)
		{
			if (pszUnknown)
			{
				pszUnknown->Clear();
				pszUnknown->Append(sz, uiLength);
			}
			return false;
		}
		sz += uiLength;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The words of uiMainBus's DEVICE_MAIN_BUS_* flags, separated by spaces,
// as MainBus has them.
//
//////////////////////////////////////////////////////////////////////////
void DeviceROMMainBusText(uint8 uiMainBus, CChars* pszDest)
{
	size	i;

	pszDest->Clear();
	for (i = 0; i < DEVICE_ROM_NUM_MAIN_BUS_FLAGS; i++)
	{
		if (uiMainBus & gasDeviceROMMainBusFlags[i].uiFlag)
		{
			if (!pszDest->Empty())
			{
				pszDest->Append(' ');
			}
			pszDest->Append(gasDeviceROMMainBusFlags[i].szWord);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// MainBus's flags, an error naming the word that isn't one.
//
//////////////////////////////////////////////////////////////////////////
static bool DeviceROMMainBus(CMarkupTag* pcTag, const char* szMainBus, uint8* puiMainBus, CChars* pszError)
{
	CChars	szUnknown;
	bool	bResult;

	szUnknown.Init();
	bResult = DeviceROMParseMainBus(szMainBus, puiMainBus, &szUnknown);
	if (!bResult)
	{
		DeviceROMError(pszError, pcTag, "Unknown MainBus: ", szUnknown.Text());
	}
	szUnknown.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The ROM from a <DeviceROM> description (see DeviceROM.h).  False, with
// pszError saying why, if the description is wrong.
//
//////////////////////////////////////////////////////////////////////////
bool CDeviceROM::LoadXML(CMarkupTag* pcTag, CChars* pszError)
{
	const char*		szName;
	const char*		szManufacturer;
	const char*		szDriver;
	const char*		szMainBus;
	char*			szDeviceBus;
	char*			szAccess;
	uint32			uiRevision;
	uint8			uiDeviceBus;
	uint8			uiMainBus;
	uint32			uiSlotEnd;
	CMarkupTag*		pcRegister;
	STagIterator	sIterator;
	uint32			uiOffset;
	uint32			uiCount;
	uint32			i;
	bool			bRead;
	bool			bWrite;

	if (strcmp(pcTag->GetName(), DEVICE_ROM_XML_TAG) != 0)
	{
		return DeviceROMError(pszError, pcTag, "Expected <" DEVICE_ROM_XML_TAG ">", NULL);
	}
	if (!DeviceROMString(pcTag, "Name", DEVICE_ROM_NAME_LENGTH, true, &szName, pszError) ||
		!DeviceROMString(pcTag, "Manufacturer", DEVICE_ROM_MAKER_LENGTH, true, &szManufacturer, pszError) ||
		!DeviceROMString(pcTag, "Driver", DEVICE_ROM_DRIVER_LENGTH, false, &szDriver, pszError) ||
		!DeviceROMString(pcTag, "MainBus", 0x100, false, &szMainBus, pszError) ||
		!DeviceROMNumber(pcTag, "Revision", 0xFF, 0, &uiRevision, pszError) ||
		!DeviceROMMainBus(pcTag, szMainBus, &uiMainBus, pszError))
	{
		return false;
	}

	szDeviceBus = pcTag->GetStringAttribute("DeviceBus");
	if (szDeviceBus && (strcmp(szDeviceBus, "32KB") == 0))
	{
		uiDeviceBus = DEVICE_BUS_32KB;
		uiSlotEnd = 0x8000;
	}
	else if (szDeviceBus && (strcmp(szDeviceBus, "128KB") == 0))
	{
		uiDeviceBus = DEVICE_BUS_128KB;
		uiSlotEnd = 0x10000;	//A register's offset is a word.
	}
	else
	{
		return DeviceROMError(pszError, pcTag, "DeviceBus must be 32KB or 128KB", NULL);
	}

	Init(szName, szManufacturer, (uint8)uiRevision, uiDeviceBus, uiMainBus, szDriver);

	for (pcRegister = pcTag->GetTag(&sIterator); pcRegister; pcRegister = pcTag->GetNextTag(&sIterator))
	{
		if (strcmp(pcRegister->GetName(), DEVICE_ROM_XML_REGISTER_TAG) != 0)
		{
			return DeviceROMError(pszError, pcRegister, "Expected <" DEVICE_ROM_XML_REGISTER_TAG ">", NULL);
		}
		if (!pcRegister->GetStringAttribute("Offset"))
		{
			return DeviceROMError(pszError, pcRegister, "Missing Offset", NULL);
		}
		if (!DeviceROMNumber(pcRegister, "Offset", 0xFFFF, 0, &uiOffset, pszError) ||
			!DeviceROMNumber(pcRegister, "Count", 0xFFFF, 1, &uiCount, pszError))
		{
			return false;
		}
		if ((uiOffset < DEVICE_ROM_SIZE) || (uiCount == 0) || (uiOffset + uiCount > uiSlotEnd))
		{
			return DeviceROMError(pszError, pcRegister, "Registers must be after the ROM and inside the slot", NULL);
		}

		szAccess = pcRegister->GetStringAttribute("Access");
		bRead = szAccess && ((strcmp(szAccess, "Read") == 0) || (strcmp(szAccess, "ReadWrite") == 0));
		bWrite = szAccess && ((strcmp(szAccess, "Write") == 0) || (strcmp(szAccess, "ReadWrite") == 0));
		if (!bRead && !bWrite)
		{
			return DeviceROMError(pszError, pcRegister, "Access must be Read, Write or ReadWrite", NULL);
		}

		for (i = 0; i < uiCount; i++)
		{
			if ((bRead && !AddRegister((uint16)(uiOffset + i), DEVICE_REGISTER_READ)) ||
				(bWrite && !AddRegister((uint16)(uiOffset + i), DEVICE_REGISTER_WRITE)))
			{
				return DeviceROMError(pszError, pcRegister, (NumRegisters() >= DEVICE_ROM_MAX_REGISTERS) ? "Too many registers" : "Register listed twice", NULL);
			}
		}
	}
	return true;
}
