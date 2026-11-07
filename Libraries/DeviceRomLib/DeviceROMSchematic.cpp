#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "BaseLib/XMLWriter.h"
#include "KicadLib/KicadSchemFile.h"
#include "KicadLib/KicadSchemNetlist.h"
#include "DeviceROMSchematic.h"


#define DEVICE_ROM_BIT_ANY		-1		//An address bit a Stage B's enables don't look at.
#define DEVICE_ROM_BIT_NEVER	-2		//The enables can't both be active.


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CDeviceROMSchematic::Init(void)
{
	mszName.Init();
	mszManufacturer.Init();
	mszDriver.Init();
	muiRevision = 0;
	muiDeviceBus = DEVICE_BUS_32KB;
	muiMainBus = 0;
	masRegisters.Init();
	mpcFile = NULL;
	mpcNetlist = NULL;
	mpszError = NULL;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CDeviceROMSchematic::Kill(void)
{
	masRegisters.Kill();
	mszDriver.Kill();
	mszManufacturer.Kill();
	mszName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Works out the board's description from szSchematic.  False, with
// pszError saying why, if it can't.
//
//////////////////////////////////////////////////////////////////////////
bool CDeviceROMSchematic::Read(const char* szSchematic, CChars* pszError)
{
	CKicadSchemFile		cFile;
	CKicadSchemNetlist	cNetlist;
	bool				bResult;

	mpszError = pszError;
	masRegisters.Kill();
	masRegisters.Init();
	cFile.Init();
	if (!cFile.Load((char*)szSchematic))
	{
		cFile.Kill();
		return Error("Can't read the schematic ", szSchematic);
	}
	cNetlist.Init(&cFile);
	bResult = cNetlist.Build();
	if (!bResult)
	{
		Error("Can't work out the schematic's nets", NULL);
	}
	else
	{
		mpcFile = &cFile;
		mpcNetlist = &cNetlist;
		bResult = Analyse();
		mpcNetlist = NULL;
		mpcFile = NULL;
	}
	cNetlist.Kill();
	cFile.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Registers in offset order, a read before a write.
//
//////////////////////////////////////////////////////////////////////////
static int CompareDeviceROMSchematicRegisters(const void* pvLeft, const void* pvRight)
{
	SDeviceROMSchematicRegister*	psLeft;
	SDeviceROMSchematicRegister*	psRight;

	psLeft = (SDeviceROMSchematicRegister*)pvLeft;
	psRight = (SDeviceROMSchematicRegister*)pvRight;
	if (psLeft->uiOffset != psRight->uiOffset)
	{
		return (psLeft->uiOffset < psRight->uiOffset) ? -1 : 1;
	}
	return (int)psLeft->uiDirection - (int)psRight->uiDirection;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CDeviceROMSchematic::Analyse(void)
{
	CKicadSchemSymbol*	pcStageA;
	CKicadSchemSymbol*	pcSymbol;
	size				uiCount;
	size				i;

	pcStageA = FindSymbol(DEVICE_ROM_STAGE_A_SYMBOL, &uiCount);
	if (uiCount != 1)
	{
		return Error("The board must have one " DEVICE_ROM_STAGE_A_SYMBOL, NULL);
	}

	for (i = 0; i < mpcFile->NumSymbols(); i++)
	{
		pcSymbol = mpcFile->GetSymbol(i);
		if (IsSymbol(pcSymbol, DEVICE_ROM_STAGE_B_SYMBOL))
		{
			if (!ReadStageB(pcSymbol))
			{
				return false;
			}
		}
	}
	masRegisters.QuickSort(CompareDeviceROMSchematicRegisters);
	for (i = 1; i < masRegisters.NumElements(); i++)
	{
		if (CompareDeviceROMSchematicRegisters(masRegisters.Get(i - 1), masRegisters.Get(i)) == 0)
		{
			return Error("Two Stage Bs decode the same register: ", masRegisters.Get(i)->szName);
		}
	}
	if (masRegisters.NumElements() > DEVICE_ROM_MAX_REGISTERS)
	{
		return Error("Too many registers", NULL);
	}

	ReadBuses(pcStageA);
	return ReadDescription(pcStageA);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CDeviceROMSchematic::Error(const char* szMessage, const char* szDetail)
{
	mpszError->Set(szMessage);
	if (szDetail)
	{
		mpszError->Append(szDetail);
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether pcSymbol is the library symbol szLibSymbol, from any library
// ("HCP65:Device_Decode_Stage_B").
//
//////////////////////////////////////////////////////////////////////////
bool CDeviceROMSchematic::IsSymbol(CKicadSchemSymbol* pcSymbol, const char* szLibSymbol)
{
	char*	szName;
	char*	szColon;

	szName = pcSymbol->GetLibId();
	szColon = strchr(szName, ':');
	if (szColon)
	{
		szName = szColon + 1;
	}
	return strcmp(szName, szLibSymbol) == 0;
}


//////////////////////////////////////////////////////////////////////////
//
// The first symbol that is szLibSymbol, and how many there are.
//
//////////////////////////////////////////////////////////////////////////
CKicadSchemSymbol* CDeviceROMSchematic::FindSymbol(const char* szLibSymbol, size* puiCount)
{
	CKicadSchemSymbol*	pcFound;
	CKicadSchemSymbol*	pcSymbol;
	size				i;

	pcFound = NULL;
	*puiCount = 0;
	for (i = 0; i < mpcFile->NumSymbols(); i++)
	{
		pcSymbol = mpcFile->GetSymbol(i);
		if (IsSymbol(pcSymbol, szLibSymbol))
		{
			if (!pcFound)
			{
				pcFound = pcSymbol;
			}
			(*puiCount)++;
		}
	}
	return pcFound;
}


//////////////////////////////////////////////////////////////////////////
//
// The net on pcSymbol's pin called szPinName, NULL if it's on nothing.
//
//////////////////////////////////////////////////////////////////////////
CKicadSchemNet* CDeviceROMSchematic::GetPinNet(CKicadSchemSymbol* pcSymbol, const char* szPinName)
{
	CKicadSchemLibSymbol*	pcLibSymbol;
	CKicadSchemLibPin*		pcPin;
	size					i;

	pcLibSymbol = mpcFile->GetLibSymbol(pcSymbol);
	if (!pcLibSymbol)
	{
		return NULL;
	}
	for (i = 0; i < pcLibSymbol->NumPins(); i++)
	{
		pcPin = pcLibSymbol->GetPin(i);
		if (pcSymbol->IsUnitPin(pcPin) && (strcmp(pcPin->GetName(), szPinName) == 0))
		{
			return mpcNetlist->GetPinNet(pcSymbol->GetReference(), pcPin->GetNumber());
		}
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether pcNet is a supply: GND (*piLevel 0) or a positive supply (1).
//
//////////////////////////////////////////////////////////////////////////
bool CDeviceROMSchematic::IsPowerNet(CKicadSchemNet* pcNet, int* piLevel)
{
	char*	szName;

	szName = pcNet->GetName();
	if ((strcmp(szName, "GND") == 0) || (strcmp(szName, "GNDD") == 0) || (strcmp(szName, "VSS") == 0))
	{
		*piLevel = 0;
		return true;
	}
	if ((strcmp(szName, "5V") == 0) || (strcmp(szName, "+5V") == 0) || (strcmp(szName, "3.3V") == 0) || (strcmp(szName, "+3.3V") == 0) || (strcmp(szName, "+3V3") == 0) || (strcmp(szName, "VCC") == 0))
	{
		*piLevel = 1;
		return true;
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether a pin on pcNet belongs to a part other than pcExcept1 and
// pcExcept2: not a power symbol, and not a connector (a J reference) if
// bExceptConnectors.
//
//////////////////////////////////////////////////////////////////////////
bool CDeviceROMSchematic::ReachesOther(CKicadSchemNet* pcNet, CKicadSchemSymbol* pcExcept1, CKicadSchemSymbol* pcExcept2, bool bExceptConnectors)
{
	CKicadSchemSubnet*		pcSubnet;
	SKicadSchemNetPin*		psPin;
	CKicadSchemLibSymbol*	pcLibSymbol;
	char*					szReference;
	size					i;
	size					j;

	for (i = 0; i < pcNet->NumSubnets(); i++)
	{
		pcSubnet = pcNet->GetSubnet(i);
		for (j = 0; j < pcSubnet->NumPins(); j++)
		{
			psPin = pcSubnet->GetPin(j);
			if ((psPin->pcSymbol == pcExcept1) || (psPin->pcSymbol == pcExcept2))
			{
				continue;
			}
			pcLibSymbol = mpcFile->GetLibSymbol(psPin->pcSymbol);
			szReference = psPin->pcSymbol->GetReference();
			if ((pcLibSymbol && pcLibSymbol->IsPower()) || (szReference[0] == '#'))
			{
				continue;
			}
			if (bExceptConnectors && (szReference[0] == 'J'))
			{
				continue;
			}
			return true;
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// What a Stage B enable on pcNet asks of the address for the enable to be
// at iActiveLevel, ANDed into aiBits (A3, A4): 0, 1, DEVICE_ROM_BIT_ANY or
// DEVICE_ROM_BIT_NEVER.  The net is A3, A4, ~{A3} or ~{A4}, as Stage A's
// or the Device Buffer's pins and the net's name show, or a supply.  False
// if it's something else.
//
//////////////////////////////////////////////////////////////////////////
bool CDeviceROMSchematic::AddressCondition(CKicadSchemNet* pcNet, int iActiveLevel, int* aiBits)
{
	CKicadSchemSubnet*	pcSubnet;
	SKicadSchemNetPin*	psPin;
	const char*			szSignal;
	int					iLevel;
	int					iBit;
	int					iValue;
	size				i;
	size				j;

	if (!pcNet)
	{
		return false;
	}
	if (IsPowerNet(pcNet, &iLevel))
	{
		if (iLevel != iActiveLevel)
		{
			aiBits[0] = DEVICE_ROM_BIT_NEVER;
		}
		return true;
	}

	szSignal = pcNet->GetName();
	for (i = 0; i < pcNet->NumSubnets(); i++)
	{
		pcSubnet = pcNet->GetSubnet(i);
		for (j = 0; j < pcSubnet->NumPins(); j++)
		{
			psPin = pcSubnet->GetPin(j);
			if (IsSymbol(psPin->pcSymbol, DEVICE_ROM_STAGE_A_SYMBOL) || IsSymbol(psPin->pcSymbol, DEVICE_ROM_BUFFER_SYMBOL))
			{
				szSignal = psPin->pcPin->GetName();
				if ((strcmp(szSignal, "A3") == 0) || (strcmp(szSignal, "A4") == 0) || (strcmp(szSignal, "~{A3}") == 0) || (strcmp(szSignal, "~{A4}") == 0))
				{
					break;
				}
				szSignal = pcNet->GetName();
			}
		}
	}

	if (strcmp(szSignal, "A3") == 0)			{ iBit = 0; iValue = iActiveLevel; }
	else if (strcmp(szSignal, "A4") == 0)		{ iBit = 1; iValue = iActiveLevel; }
	else if (strcmp(szSignal, "~{A3}") == 0)	{ iBit = 0; iValue = 1 - iActiveLevel; }
	else if (strcmp(szSignal, "~{A4}") == 0)	{ iBit = 1; iValue = 1 - iActiveLevel; }
	else
	{
		return false;
	}

	if (aiBits[iBit] == DEVICE_ROM_BIT_ANY)
	{
		aiBits[iBit] = iValue;
	}
	else if (aiBits[iBit] != iValue)
	{
		aiBits[iBit] = DEVICE_ROM_BIT_NEVER;
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The eight registers a Device Decode Stage B can decode: those whose
// strobe goes somewhere.  An address bit its enables don't look at is
// taken as 0 (the registers repeat).
//
//////////////////////////////////////////////////////////////////////////
bool CDeviceROMSchematic::ReadStageB(CKicadSchemSymbol* pcStageB)
{
	int				aiBits[2];
	uint16			uiBase;
	char			szPin[16];
	CKicadSchemNet*	pcNet;
	int				i;

	aiBits[0] = DEVICE_ROM_BIT_ANY;
	aiBits[1] = DEVICE_ROM_BIT_ANY;
	if (!AddressCondition(GetPinNet(pcStageB, "~{E2}"), 0, aiBits) || !AddressCondition(GetPinNet(pcStageB, "E3"), 1, aiBits))
	{
		return Error("Can't tell which registers this Stage B decodes (its ~{E2} and E3 must be A3, A4, ~{A3}, ~{A4} or a supply): ", pcStageB->GetReference());
	}
	if ((aiBits[0] == DEVICE_ROM_BIT_NEVER) || (aiBits[1] == DEVICE_ROM_BIT_NEVER))
	{
		return Error("This Stage B is never enabled: ", pcStageB->GetReference());
	}
	uiBase = (uint16)(0x400 + ((aiBits[0] == 1) ? 8 : 0) + ((aiBits[1] == 1) ? 16 : 0));

	for (i = 0; i < 8; i++)
	{
		snprintf(szPin, sizeof(szPin), "Y%d_{R}", i);
		pcNet = GetPinNet(pcStageB, szPin);
		if (pcNet && ReachesOther(pcNet, pcStageB, NULL, false))
		{
			if (!AddRegister((uint16)(uiBase + i), DEVICE_REGISTER_READ, pcNet))
			{
				return false;
			}
		}
		snprintf(szPin, sizeof(szPin), "Y%d_{W}", i);
		pcNet = GetPinNet(pcStageB, szPin);
		if (pcNet && ReachesOther(pcNet, pcStageB, NULL, false))
		{
			if (!AddRegister((uint16)(uiBase + i), DEVICE_REGISTER_WRITE, pcNet))
			{
				return false;
			}
		}
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// szText without KiCad's text markup (~{overbar}, _{subscript} and
// ^{superscript} keep only what is in the braces), its runs of spaces one
// space and none at its ends, in szDest (uiSize bytes, cut short to fit).
//
//////////////////////////////////////////////////////////////////////////
static void DeviceROMPlainText(const char* szText, char* szDest, size uiSize)
{
	size	uiLength;
	char	c;

	uiLength = 0;
	while ((c = *szText) != '\0')
	{
		szText++;
		if (((c == '~') || (c == '_') || (c == '^')) && (*szText == '{'))
		{
			szText++;
			continue;
		}
		if (c == '}')
		{
			continue;
		}
		if ((c == ' ') && ((uiLength == 0) || (szDest[uiLength - 1] == ' ')))
		{
			continue;
		}
		if (uiLength + 1 < uiSize)
		{
			szDest[uiLength] = c;
			uiLength++;
		}
	}
	if ((uiLength > 0) && (szDest[uiLength - 1] == ' '))
	{
		uiLength--;
	}
	szDest[uiLength] = '\0';
}


//////////////////////////////////////////////////////////////////////////
//
// A register named by its strobe's net, without its "Read " or "Write "
// or KiCad's markup.
//
//////////////////////////////////////////////////////////////////////////
bool CDeviceROMSchematic::AddRegister(uint16 uiOffset, uint8 uiDirection, CKicadSchemNet* pcNet)
{
	SDeviceROMSchematicRegister*	psRegister;
	const char*						szName;

	psRegister = masRegisters.Add();
	psRegister->uiOffset = uiOffset;
	psRegister->uiDirection = uiDirection;
	szName = "";
	if (pcNet->GetNameSource() != KSNNS_Pin)
	{
		szName = pcNet->GetName();
		if ((uiDirection == DEVICE_REGISTER_READ) && (strncmp(szName, "Read ", 5) == 0))
		{
			szName += 5;
		}
		else if ((uiDirection == DEVICE_REGISTER_WRITE) && (strncmp(szName, "Write ", 6) == 0))
		{
			szName += 6;
		}
	}
	DeviceROMPlainText(szName, psRegister->szName, DEVICE_ROM_NAME_LENGTH);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The device bus the board needs and what it does on the main bus.
//
//////////////////////////////////////////////////////////////////////////
void CDeviceROMSchematic::ReadBuses(CKicadSchemSymbol* pcStageA)
{
	CKicadSchemNet*		pcNet;
	CKicadSchemSymbol*	pcBuffer;
	size				uiCount;
	int					iLevel;

	pcBuffer = FindSymbol(DEVICE_ROM_BUFFER_SYMBOL, &uiCount);
	muiDeviceBus = DEVICE_BUS_32KB;
	pcNet = GetPinNet(pcStageA, "A16");
	if (pcNet && !IsPowerNet(pcNet, &iLevel) && ReachesOther(pcNet, pcStageA, pcBuffer, false))
	{
		muiDeviceBus = DEVICE_BUS_128KB;
	}

	muiMainBus = 0;
	pcNet = mpcNetlist->GetNet((char*)DEVICE_ROM_MAIN_ACCESS_NET);
	if (pcNet && ReachesOther(pcNet, NULL, NULL, true))
	{
		muiMainBus |= DEVICE_MAIN_BUS_MASTER;
	}
	pcNet = mpcNetlist->GetNet((char*)DEVICE_ROM_MEMORY_A_NET);
	if (pcNet && ReachesOther(pcNet, NULL, NULL, true))
	{
		muiMainBus |= DEVICE_MAIN_BUS_WINDOW_A;
	}
	pcNet = mpcNetlist->GetNet((char*)DEVICE_ROM_MEMORY_B_NET);
	if (pcNet && ReachesOther(pcNet, NULL, NULL, true))
	{
		muiMainBus |= DEVICE_MAIN_BUS_WINDOW_B;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The name, manufacturer, revision and driver: the device ROM chip's ROM_
// properties, else the title block.
//
//////////////////////////////////////////////////////////////////////////
bool CDeviceROMSchematic::ReadDescription(CKicadSchemSymbol* pcStageA)
{
	CKicadSchemNet*			pcNet;
	CKicadSchemSubnet*		pcSubnet;
	SKicadSchemNetPin*		psPin;
	CKicadSchemSymbol*		pcROM;
	CKicadSchemProperties*	pcProperties;
	char*					szValue;
	const char*				szRevision;
	long					lRevision;
	size					i;
	size					j;

	pcROM = NULL;
	pcNet = GetPinNet(pcStageA, "~{Device_ROM}");
	for (i = 0; pcNet && !pcROM && (i < pcNet->NumSubnets()); i++)
	{
		pcSubnet = pcNet->GetSubnet(i);
		for (j = 0; j < pcSubnet->NumPins(); j++)
		{
			psPin = pcSubnet->GetPin(j);
			if ((strcmp(psPin->pcPin->GetName(), "~{CE}") == 0) || (strcmp(psPin->pcPin->GetName(), "~{CS}") == 0))
			{
				pcROM = psPin->pcSymbol;
				break;
			}
		}
	}
	if (!pcROM)
	{
		return Error("Can't find the device ROM chip (its ~{CE} on Stage A's ~{Device_ROM})", NULL);
	}
	pcProperties = pcROM->GetProperties();

	szValue = pcProperties->GetValue(DEVICE_ROM_PROPERTY_NAME);
	mszName.Set((szValue && szValue[0]) ? szValue : mpcFile->GetTitle());
	if (mszName.Empty())
	{
		return Error("The board has no name: give its title block a title or its device ROM chip " DEVICE_ROM_PROPERTY_NAME, NULL);
	}

	szValue = pcProperties->GetValue(DEVICE_ROM_PROPERTY_MANUFACTURER);
	mszManufacturer.Set((szValue && szValue[0]) ? szValue : mpcFile->GetCompany());
	if (mszManufacturer.Empty())
	{
		Error("The board has no manufacturer: give its device ROM chip, ", pcROM->GetReference());
		mpszError->Append(", a " DEVICE_ROM_PROPERTY_MANUFACTURER " property");
		return false;
	}

	szValue = pcProperties->GetValue(DEVICE_ROM_PROPERTY_DRIVER);
	mszDriver.Set(szValue ? szValue : "");

	szValue = pcProperties->GetValue(DEVICE_ROM_PROPERTY_REVISION);
	szRevision = (szValue && szValue[0]) ? szValue : mpcFile->GetRevision();
	while (szRevision && *szRevision && ((*szRevision < '0') || (*szRevision > '9')))
	{
		szRevision++;
	}
	lRevision = (szRevision && *szRevision) ? strtol(szRevision, NULL, 10) : 0;
	if ((lRevision < 0) || (lRevision > 0xFF))
	{
		return Error("The board's revision isn't 0 to 255", NULL);
	}
	muiRevision = (uint8)lRevision;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The <DeviceROM> description (DeviceROM.h).
//
//////////////////////////////////////////////////////////////////////////
void CDeviceROMSchematic::SaveXML(CXMLWriter* pcWriter)
{
	SDeviceROMSchematicRegister*	psRegister;
	SDeviceROMSchematicRegister*	psNext;
	CChars							szMainBus;
	char							szOffset[16];
	size							i;

	szMainBus.Init();
	if (muiMainBus & DEVICE_MAIN_BUS_MASTER)
	{
		szMainBus.Append("Master");
	}
	if (muiMainBus & DEVICE_MAIN_BUS_WINDOW_A)
	{
		szMainBus.Append(szMainBus.Empty() ? "WindowA" : " WindowA");
	}
	if (muiMainBus & DEVICE_MAIN_BUS_WINDOW_B)
	{
		szMainBus.Append(szMainBus.Empty() ? "WindowB" : " WindowB");
	}

	pcWriter->Begin(DEVICE_ROM_XML_TAG);
	pcWriter->Attribute("Name", mszName.Text());
	pcWriter->Attribute("Manufacturer", mszManufacturer.Text());
	pcWriter->Attribute("Revision", (int32)muiRevision);
	pcWriter->Attribute("DeviceBus", (muiDeviceBus == DEVICE_BUS_128KB) ? "128KB" : "32KB");
	if (!szMainBus.Empty())
	{
		pcWriter->Attribute("MainBus", szMainBus.Text());
	}
	if (!mszDriver.Empty())
	{
		pcWriter->Attribute("Driver", mszDriver.Text());
	}

	for (i = 0; i < masRegisters.NumElements(); i++)
	{
		psRegister = masRegisters.Get(i);
		psNext = (i + 1 < masRegisters.NumElements()) ? masRegisters.Get(i + 1) : NULL;
		pcWriter->Begin(DEVICE_ROM_XML_REGISTER_TAG);
		if (psRegister->szName[0])
		{
			pcWriter->Attribute("Name", psRegister->szName);
		}
		snprintf(szOffset, sizeof(szOffset), "0x%03X", (unsigned)psRegister->uiOffset);
		pcWriter->Attribute("Offset", szOffset);
		if (psNext && (psNext->uiOffset == psRegister->uiOffset) && (strcmp(psNext->szName, psRegister->szName) == 0))
		{
			pcWriter->Attribute("Access", "ReadWrite");
			i++;
		}
		else
		{
			pcWriter->Attribute("Access", (psRegister->uiDirection == DEVICE_REGISTER_READ) ? "Read" : "Write");
		}
		pcWriter->End();
	}
	pcWriter->End();
	szMainBus.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CDeviceROMSchematic::WriteXML(const char* szFileName)
{
	CXMLWriter	cWriter;
	bool		bResult;

	cWriter.Init();
	SaveXML(&cWriter);
	bResult = cWriter.Write(szFileName);
	cWriter.Kill();
	return bResult;
}


char* CDeviceROMSchematic::GetName(void) { return mszName.Text(); }
char* CDeviceROMSchematic::GetManufacturer(void) { return mszManufacturer.Text(); }
char* CDeviceROMSchematic::GetDriver(void) { return mszDriver.Text(); }
uint8 CDeviceROMSchematic::GetRevision(void) { return muiRevision; }
uint8 CDeviceROMSchematic::GetDeviceBus(void) { return muiDeviceBus; }
uint8 CDeviceROMSchematic::GetMainBus(void) { return muiMainBus; }
size CDeviceROMSchematic::NumRegisters(void) { return masRegisters.NumElements(); }
SDeviceROMSchematicRegister* CDeviceROMSchematic::GetRegister(size uiIndex) { return masRegisters.Get(uiIndex); }
