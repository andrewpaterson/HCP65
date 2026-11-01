#include "BaseLib/Logger.h"
#include "BaseLib/LogString.h"
#include "BaseLib/DebugOutput.h"
#include "BoardPins.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBoardPins::Init(void)
{
	mszName.Init();
	mszVersion.Init();

	mmPinSignals.Init();
	mmPinNames.Init();
	mmPinPowers.Init();
	mmPinNoConnects.Init();
	msOutputs.Init();
	msInverted.Init();

	miNumPins = 0;

	msWriteValues.Init();
	msReadValues.Init();

	mmBusses.Init();
	mmPinBusses.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBoardPins::Kill(void)
{
	SMapIterator	sIter;
	bool			bHasNext;
	CBusPins*		pcBus;

	mmPinBusses.Kill();

	bHasNext = mmBusses.StartIteration(&sIter, NULL, NULL, (void**)&pcBus, NULL);
	while (bHasNext)
	{
		pcBus->Kill();
		bHasNext = mmBusses.Iterate(&sIter, NULL, NULL, (void**)&pcBus, NULL);
	}
	mmBusses.Kill();
		
	msReadValues.Kill();
	msWriteValues.Kill();

	msInverted.Kill();
	msOutputs.Kill();
	mmPinNoConnects.Kill();
	mmPinPowers.Kill();
	mmPinNames.Kill();
	mmPinSignals.Kill();

	mszVersion.Kill();
	mszName.Kill();

	miNumPins = 0;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CBoardPins::ValidateCanAdd(size iPinNumber, char* szPinName)
{
	if (iPinNumber < 1)
	{
		return gcLogger.Error2(__METHOD__, " Cannot add Pin numbered [", SizeToString(iPinNumber), "].  It must be greater than zero.", NULL);
	}
	else if (iPinNumber > 128)
	{
		return gcLogger.Error2(__METHOD__, " Cannot add Pin numbered [", SizeToString(iPinNumber), "].  It must be less than 129.", NULL);
	}

	if (szPinName)
	{
		if (StrEmpty(szPinName))
		{
			return gcLogger.Error2(__METHOD__, " Cannot add an unamed Pin.", NULL);
		}
		if (mmPinNames.HasKey(szPinName))
		{
			return gcLogger.Error2(__METHOD__, " Cannot add Pin named [", szPinName, "].  It already exists.", NULL);
		}
	}

	if (mmPinSignals.HasKey((int)iPinNumber))
	{
		return gcLogger.Error2(__METHOD__, " Cannot add Pin numbered [", SizeToString(iPinNumber), "].  It already exists as a signal.", NULL);
	}
	if (mmPinPowers.HasKey((int)iPinNumber))
	{
		return gcLogger.Error2(__METHOD__, " Cannot add Pin numbered [", SizeToString(iPinNumber), "].  It already exists as power.", NULL);
	}
	if (mmPinNoConnects.HasKey((int)iPinNumber))
	{
		return gcLogger.Error2(__METHOD__, " Cannot add Pin numbered [", SizeToString(iPinNumber), "].  It already exists as a no connect.", NULL);
	}

	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CBoardPins::AddSignal(size iPinNumber, char* szPinName, EPinDirection eDirection, EPinSignal eSignal)
{
	if (!ValidateCanAdd(iPinNumber, szPinName))
	{
		return false;
	}

	mmPinNames.Put(szPinName, (int)iPinNumber);
	mmPinSignals.Put((int)iPinNumber, szPinName);

	msOutputs.GrowTo(iPinNumber);
	msOutputs.Set(iPinNumber - 1, eDirection == PD_Input);

	msInverted.GrowTo(iPinNumber);
	msInverted.Set(iPinNumber - 1, eSignal == PS_Inverted);

	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CBoardPins::AddPower(size iPinNumber, EPowerPin ePower)
{
	if (!ValidateCanAdd(iPinNumber))
	{
		return false;
	}

	mmPinPowers.Put((int)iPinNumber, (int)ePower);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CBoardPins::AddNoCon(size iPinNumber)
{
	if (!ValidateCanAdd(iPinNumber))
	{
		return false;
	}

	mmPinNoConnects.Put((int)iPinNumber, 0);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CBoardPins::GroupBus(char* szBusName, char* szPinName, int iBusOffset)
{
	CBusPins*	pcBusPins;
	int			iPinNumber;
	int			iExistingOffset;

	pcBusPins = mmBusses.Get(szBusName);
	if (!pcBusPins)
	{
		pcBusPins = mmBusses.Put(szBusName);
		pcBusPins->Init();
	}

	iPinNumber = (int)mmPinNames.Get(szPinName);
	if (iPinNumber == -1)
	{
		return gcLogger.Error2(__METHOD__, " Cannot add Pin named [", szPinName, "] into Bus [", szBusName, "].  Pin does not exist.", NULL);
	}

	iExistingOffset = pcBusPins->Get(iPinNumber);
	if (iExistingOffset != -1)
	{
		return gcLogger.Error2(__METHOD__, " Cannot add Pin named [", szPinName, "] into Bus [", szBusName, "].  It already has an offset.", NULL);
	}

	mmPinBusses.Put(iPinNumber, szBusName);

	pcBusPins->Set(iPinNumber, iBusOffset);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CBoardPins::Done(void)
{
	SMapIterator	sIter;
	bool			bHasNext;
	int*			piPinNumber;	

	miNumPins = 0;

	bHasNext = mmPinSignals.StartIteration(&sIter, (void**)&piPinNumber, NULL, NULL, NULL);
	while (bHasNext)
	{
		if (miNumPins < (size)(*piPinNumber))
		{
			miNumPins = (size)(*piPinNumber);
		}
		bHasNext = mmPinSignals.Iterate(&sIter, (void**)&piPinNumber, NULL, NULL, NULL);
	}

	bHasNext = mmPinPowers.StartIteration(&sIter, (void**)&piPinNumber, NULL, NULL, NULL);
	while (bHasNext)
	{
		if (miNumPins < (size)(*piPinNumber))
		{
			miNumPins = (size)(*piPinNumber);
		}
		bHasNext = mmPinPowers.Iterate(&sIter, (void**)&piPinNumber, NULL, NULL, NULL);
	}

	bHasNext = mmPinNoConnects.StartIteration(&sIter, (void**)&piPinNumber, NULL, NULL, NULL);
	while (bHasNext)
	{
		if (miNumPins < (size)(*piPinNumber))
		{
			miNumPins = (size)(*piPinNumber);
		}
		bHasNext = mmPinNoConnects.Iterate(&sIter, (void**)&piPinNumber, NULL, NULL, NULL);
	}

	msWriteValues.GrowTo(miNumPins);
	msWriteValues.Zero();

	msReadValues.GrowTo(miNumPins);
	msReadValues.Zero();
	
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBoardPins::SetName(char* szName)
{
	mszName.Set(szName);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBoardPins::SetVersion(char* szVersion)
{
	mszVersion.Set(szVersion);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CBoardPins::Set(char* szPinName, bool bValue)
{
	int32	iPin;

	iPin = mmPinNames.Get(szPinName);
	if (iPin == -1)
	{
		return false;
	}

	return Set(iPin, bValue);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBoardPins::SetZero(void)
{
	msWriteValues.Zero();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CBoardPins::SetBus(char* szBusName, uint32 uiBusValues)
{
	int				iPinNumber;
	int				iBusOffset;
	CBusPins*		pcBus;
	SMapIterator	sIter;
	bool			bHasNext;
	uint32			uiMask;
	bool			bValue;
	
	pcBus = mmBusses.Get(szBusName);
	if (!pcBus)
	{
		return gcLogger.Error2(__METHOD__, " Cannot find a Bus named [", szBusName, "].");
	}

	bHasNext = pcBus->StartIteration(&sIter, &iPinNumber, &iBusOffset);
	while (bHasNext)
	{
		uiMask = 1 << iBusOffset;
		bValue = uiMask & uiBusValues;
		Set(iPinNumber, bValue);
		bHasNext = pcBus->Iterate(&sIter, &iPinNumber, &iBusOffset);
	}

	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CBoardPins::Set(size iPinNumber, bool bValue)
{
	if ((iPinNumber < 1) ||
		(iPinNumber > miNumPins))
	{
		return false;
	}

	msWriteValues.Set(iPinNumber - 1, bValue);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBoardPins::GenerateLeft(CArrayBit* psArray, CChars* psz)
{
	size	ui;
	bool	bValue;
	char	c;
	uint8	uiValue;
	size	iCount;
	size	uiHalfPins;

	uiHalfPins = miNumPins / 2;
	iCount = 0;
	for (ui = uiHalfPins-1; ui != SIZE_MAX ; ui--)
	{
		bValue = psArray->Get(ui);

		if (iCount == 0)
		{
			uiValue = 0;
		}
		if (iCount <= 3)
		{
			if (bValue)
			{
				uiValue |= 1 << (3 - iCount);
			}
		}
		if (iCount == 3)
		{
			c = GetHexChar(uiValue);
			c = ToUpper(c);
			psz->Append(c);
			iCount = 0;
		}
		else
		{
			iCount++;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBoardPins::GenerateRight(CArrayBit* psArray, CChars* psz)
{
	size	ui;
	bool	bValue;
	char	c;
	uint8	uiValue;
	int		iCount;
	size	uiHalfPins;

	uiHalfPins = miNumPins / 2;
	iCount = 0;
	for (ui = uiHalfPins; ui < miNumPins; ui++)
	{
		bValue = psArray->Get(ui);

		if (iCount == 0)
		{
			uiValue = 0;
		}
		if (iCount <= 3)
		{
			if (bValue)
			{
				uiValue |= 1 << (3 - iCount);
			}
		}
		if (iCount == 3)
		{
			c = GetHexChar(uiValue);
			c = ToUpper(c);
			psz->Append(c);
			iCount = 0;
		}
		else
		{
			iCount++;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBoardPins::GenerateOutput(CChars* psz)
{
	CChars	szLeft;
	CChars	szRight;

	szRight.Init();
	szLeft.Init();

	GenerateLeft(&msOutputs, &szLeft);
	GenerateRight(&msOutputs, &szRight);

	psz->Append('O');
	psz->Append(&szRight);
	psz->Append(&szLeft);

	szLeft.Kill();
	szRight.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBoardPins::GenerateWrite(CChars* psz)
{
	CChars	szLeft;
	CChars	szRight;

	szRight.Init();
	szLeft.Init();

	GenerateLeft(&msWriteValues, &szLeft);
	GenerateRight(&msWriteValues, &szRight);

	psz->Append('W');
	psz->Append(&szRight);
	psz->Append(&szLeft);

	szLeft.Kill();
	szRight.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBoardPins::GeneratePower(CChars* psz)
{
	bool	bAny;

	bAny = false;
	psz->Append("P");

	if (IsGroundStyleD())
	{
		psz->Append("Gd1");
		bAny = true;
	}
	if (IsGroundStyleC())
	{
		psz->Append("Gc1");
		bAny = true;
	}

	if (Is5VStyleC())
	{
		psz->Append("5c1");
		bAny = true;
	}
	if (Is5VStyleB())
	{
		psz->Append("5b1");
		bAny = true;
	}

	if (!bAny)
	{
		psz->RemoveFromEnd(1);
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBoardPins::UpdateRead(CChars* pszReadResult)
{
	size		iCount;
	size		uiHalfPins;
	size		i;
	size		j;
	char		c;
	size		uiNybble;
	size		uiLength;
	size		uiMask;
	size		iPinNumber;
	bool		bBit;

	uiHalfPins = miNumPins / 2;
	iCount = miNumPins;

	msReadValues.Zero();
	uiLength = pszReadResult->Length() - 3;  //R, \r and \n
	for (i = 0; i < uiLength; i++)
	{
		c = pszReadResult->GetChar(i + 1);
		c = ToLower(c);
		uiNybble = GetCharHex(c);
		if (uiNybble == SIZE_MAX)
		{
			return;
		}

		for (j = 0; j < 4; j++)
		{
			if (iCount > uiHalfPins)
			{
				iPinNumber = miNumPins - iCount + uiHalfPins + 1;
			}
			else
			{
				iPinNumber = iCount;
			}

			uiMask = (size)1 << (3 - j);
			bBit = uiNybble & uiMask;
			msReadValues.Set(iPinNumber - 1, bBit);

			iCount--;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBoardPins::GenerateRead(CChars* psz)
{
	psz->Append("R0_");
	psz->Append((int)(miNumPins / 8) - 1);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CBoardPins::IsGroundStyleD(void)
{
	EPowerPin	*piG0, *piG1, *piG2, *piG3;
	bool		 bG0,   bG1,   bG2,   bG3;

	piG0 = NULL;
	piG1 = NULL;
	piG2 = NULL;
	piG3 = NULL;

	if (miNumPins == 24)
	{
		piG0 = (EPowerPin*)mmPinPowers.Get(4);
		piG1 = (EPowerPin*)mmPinPowers.Get(9);
		piG2 = (EPowerPin*)mmPinPowers.Get(16);
		piG3 = (EPowerPin*)mmPinPowers.Get(21);
	}
	else if (miNumPins == 32)
	{
		piG0 = (EPowerPin*)mmPinPowers.Get(5);
		piG1 = (EPowerPin*)mmPinPowers.Get(12);
		piG2 = (EPowerPin*)mmPinPowers.Get(21);
		piG3 = (EPowerPin*)mmPinPowers.Get(28);
	}
	else if (miNumPins == 40)
	{
		piG0 = (EPowerPin*)mmPinPowers.Get(7);
		piG1 = (EPowerPin*)mmPinPowers.Get(14);
		piG2 = (EPowerPin*)mmPinPowers.Get(27);
		piG3 = (EPowerPin*)mmPinPowers.Get(34);
	}
	else if (miNumPins == 56)
	{
		piG0 = (EPowerPin*)mmPinPowers.Get(9);
		piG1 = (EPowerPin*)mmPinPowers.Get(20);
		piG2 = (EPowerPin*)mmPinPowers.Get(37);
		piG3 = (EPowerPin*)mmPinPowers.Get(48);
	}	
	else if (miNumPins == 64)
	{
		piG0 = (EPowerPin*)mmPinPowers.Get(10);
		piG1 = (EPowerPin*)mmPinPowers.Get(23);
		piG2 = (EPowerPin*)mmPinPowers.Get(42);
		piG3 = (EPowerPin*)mmPinPowers.Get(55);
	}

	bG0 = piG0 && ((*piG0) == PP_Ground);
	bG1 = piG1 && ((*piG1) == PP_Ground);
	bG2 = piG2 && ((*piG2) == PP_Ground);
	bG3 = piG3 && ((*piG3) == PP_Ground);

	if (bG0 && bG1 && bG2 && bG3)
	{
		return true;
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CBoardPins::IsGroundStyleC(void)
{
	EPowerPin	*piG;
	bool		 bG;

	piG = NULL;

	if (miNumPins == 24)
	{
		piG = (EPowerPin*)mmPinPowers.Get(24);
	}
	else if (miNumPins == 32)
	{
		piG = (EPowerPin*)mmPinPowers.Get(32);
	}
	else if (miNumPins == 40)
	{
		piG = (EPowerPin*)mmPinPowers.Get(40);
	}
	else if (miNumPins == 56)
	{
		piG = (EPowerPin*)mmPinPowers.Get(56);
	}
	else if (miNumPins == 64)
	{
		piG = (EPowerPin*)mmPinPowers.Get(64);
	}

	bG = piG && ((*piG) == PP_Ground);

	if (bG)
	{
		return true;
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CBoardPins::Is5VStyleC(void)
{
	EPowerPin	*pi50, *pi51;
	bool	 	 b50,	b51;

	pi50 = NULL;
	pi51 = NULL;

	if (miNumPins == 24)
	{
		pi50 = (EPowerPin*)mmPinPowers.Get(1);
		pi51 = (EPowerPin*)mmPinPowers.Get(23);
	}
	else if (miNumPins == 32)
	{
		pi50 = (EPowerPin*)mmPinPowers.Get(1);
		pi51 = (EPowerPin*)mmPinPowers.Get(17);
	}
	else if (miNumPins == 40)
	{
		pi50 = (EPowerPin*)mmPinPowers.Get(3);
		pi51 = (EPowerPin*)mmPinPowers.Get(23);
	}
	else if (miNumPins == 56)
	{
		pi50 = (EPowerPin*)mmPinPowers.Get(3);
		pi51 = (EPowerPin*)mmPinPowers.Get(31);
	}
	else if (miNumPins == 64)
	{
		pi50 = (EPowerPin*)mmPinPowers.Get(3);
		pi51 = (EPowerPin*)mmPinPowers.Get(35);
	}

	b50 = pi50 && ((*pi50) == PP_5V);
	b51 = pi51 && ((*pi50) == PP_5V);
	if (b50 && b51)
	{
		return true;
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CBoardPins::Is5VStyleB(void)
{
	EPowerPin	*pi5;
	bool	 	 b5;

	pi5 = NULL;

	if (miNumPins == 24)
	{
		pi5 = (EPowerPin*)mmPinPowers.Get(12);

	}
	else if (miNumPins == 32)
	{
		pi5 = (EPowerPin*)mmPinPowers.Get(16);
	}
	else if (miNumPins == 40)
	{
		pi5 = (EPowerPin*)mmPinPowers.Get(20);
	}
	else if (miNumPins == 56)
	{
		pi5 = (EPowerPin*)mmPinPowers.Get(28);
	}
	else if (miNumPins == 64)
	{
		pi5 = (EPowerPin*)mmPinPowers.Get(32);
	}

	b5 = pi5 && ((*pi5) == PP_5V);
	if (b5)
	{
		return true;
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
size CBoardPins::NumPins(void)
{
	return miNumPins;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CBoardPins::Get(char* szPinName)
{
	size	iPinNumber;
	bool	bValue;

	iPinNumber = (size)mmPinNames.Get(szPinName, 0);
	if (iPinNumber == 0)
	{
		return gcLogger.Error2(__METHOD__, " Cannot find a Pin named [", szPinName, "].");
	}

	bValue = msReadValues.Get(iPinNumber - 1);
	return bValue;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CBoardPins::Get(size iPinNumber)
{
	bool	bValue;

	bValue = msReadValues.Get(iPinNumber - 1);
	return bValue;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CBoardPins::GetWrite(size iPinNumber)
{
	bool	bValue;

	bValue = msWriteValues.Get(iPinNumber - 1);
	return bValue;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint32 CBoardPins::GetBus(char* szBusName)
{
	int				iPinNumber;
	int				iBusOffset;
	CBusPins*		pcBus;
	SMapIterator	sIter;
	bool			bHasNext;
	uint32			uiMask;
	bool			bValue;
	uint32			uiBusValues;

	pcBus = mmBusses.Get(szBusName);
	if (pcBus == NULL)
	{
		gcLogger.Error2(__METHOD__, " Cannot find a Bus named [", szBusName, "].");
		return 0;
	}

	uiBusValues = 0;
	bHasNext = pcBus->StartIteration(&sIter, &iPinNumber, &iBusOffset);
	while (bHasNext)
	{
		bValue = Get(iPinNumber);
		if (bValue)
		{
			uiMask = 1 << iBusOffset;
			uiBusValues = uiMask | uiBusValues;
		}
		bHasNext = pcBus->Iterate(&sIter, &iPinNumber, &iBusOffset);
	}

	return uiBusValues;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint32 CBoardPins::GetBusWrite(char* szBusName)
{
	int				iPinNumber;
	int				iBusOffset;
	CBusPins*		pcBus;
	SMapIterator	sIter;
	bool			bHasNext;
	uint32			uiMask;
	bool			bValue;
	uint32			uiBusValues;

	pcBus = mmBusses.Get(szBusName);
	if (pcBus == NULL)
	{
		gcLogger.Error2(__METHOD__, " Cannot find a Bus named [", szBusName, "].");
		return 0;
	}

	uiBusValues = 0;
	bHasNext = pcBus->StartIteration(&sIter, &iPinNumber, &iBusOffset);
	while (bHasNext)
	{
		bValue = GetWrite(iPinNumber);
		if (bValue)
		{
			uiMask = 1 << iBusOffset;
			uiBusValues = uiMask | uiBusValues;
		}
		bHasNext = pcBus->Iterate(&sIter, &iPinNumber, &iBusOffset);
	}

	return uiBusValues;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
char* CBoardPins::GetPin(size iPinNumber)
{
	return mmPinSignals.Get((int)iPinNumber);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBoardPins::PrintPin(CChars* psz, CArrayBit* pcBits, size iPinNumber)
{
	bool		bBit;
	char*		szPinName;
	bool		bInverted;
	bool		bBusMember;

	bBusMember = mmPinBusses.HasKey((int)iPinNumber);
	if (!bBusMember)
	{
		bBit = pcBits->Get(iPinNumber - 1);
		bInverted = msInverted.Get(iPinNumber - 1);
		szPinName = GetPin(iPinNumber);

		if (bInverted)
		{
			psz->Append("/");
		}
		psz->Append(szPinName);

		psz->Append(": ");
		if (bBit)
		{
			psz->Append('1');
		}
		else
		{
			psz->Append('0');
		}
		psz->AppendNewLine();
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBoardPins::PrintPinActive(CChars* psz, CArrayBit* pcBits, size iPinNumber)
{
	bool		bBit;
	bool		bInverted;
	char*		szPinName;
	bool		bBusMember;

	bBusMember = mmPinBusses.HasKey((int)iPinNumber);
	if (!bBusMember)
	{
		bBit = pcBits->Get(iPinNumber - 1);
		bInverted = msInverted.Get(iPinNumber - 1);
		if (bBit != bInverted)
		{
			if (bInverted)
			{
				psz->Append("/");
			}
			szPinName = GetPin(iPinNumber);
			psz->Append(szPinName);
			psz->AppendNewLine();
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CBoardPins::IsBusRead(CBusPins* pcBusPins)
{
	SMapIterator	sIter;
	int				iPinNumber;
	int				iOffset;
	bool			bHasNext;
	bool			bOutput;

	bHasNext = pcBusPins->StartIteration(&sIter, &iPinNumber, &iOffset);
	while (bHasNext)
	{
		bOutput = !msOutputs.Get(iPinNumber - 1);
		if (bOutput)
		{
			return true;
		}
		bHasNext = pcBusPins->Iterate(&sIter, &iPinNumber, &iOffset);
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CBoardPins::IsBusWrite(CBusPins* pcBusPins)
{
	SMapIterator	sIter;
	int				iPinNumber;
	int				iOffset;
	bool			bHasNext;
	bool			bInput;

	bHasNext = pcBusPins->StartIteration(&sIter, &iPinNumber, &iOffset);
	while (bHasNext)
	{
		bInput = msOutputs.Get(iPinNumber - 1);
		if (bInput)
		{
			return true;
		}
		bHasNext = pcBusPins->Iterate(&sIter, &iPinNumber, &iOffset);
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBoardPins::PrintBus(CChars* psz, CArrayBit* pcBits, CBusPins* pcBusPins, char* pszName)
{
	SMapIterator	sIter;
	int				iPinNumber;
	int				iOffset;
	bool			bHasNext;
	bool			bBit;
	uint32			uiBusValue;

	psz->Append(pszName);
	psz->Append(": ");

	uiBusValue = 0;
	bHasNext = pcBusPins->StartIteration(&sIter, &iPinNumber, &iOffset);
	while (bHasNext)
	{
		bBit = pcBits->Get(iPinNumber - 1);
		if (bBit)
		{
			uiBusValue |= 1 << iOffset;
		}
		bHasNext = pcBusPins->Iterate(&sIter, &iPinNumber, &iOffset);
	}
	psz->Append(uiBusValue, 16);
	psz->AppendNewLine();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CBoardPins::PrintRead(CChars* psz, bool bOnlyActive)
{
	size			iPinNumber;
	char*			szPinName;
	bool			bOutput;
	bool			bNoConnect;
	bool			bPower;
	bool			bBit;
	bool			bExpected;
	SMapIterator	sIter;
	bool			bHasNext;
	CBusPins*		pcBus;
	char*			szBusName;
	bool			bFailed;

	bFailed = false;
	psz->Append("Outputs from board");
	if (bOnlyActive)
	{
		psz->Append(" (only active)");
	}
	psz->AppendNewLine();

	bHasNext = mmBusses.StartIteration(&sIter, (void**)&szBusName, NULL, (void**)&pcBus, NULL);
	while (bHasNext)
	{
		if (IsBusRead(pcBus))
		{
			PrintBus(psz, &msReadValues, pcBus, szBusName);
		}
		bHasNext = mmBusses.Iterate(&sIter, (void**)&szBusName, NULL, (void**)&pcBus, NULL);
	}

	for (iPinNumber = 1; iPinNumber <= miNumPins; iPinNumber++)
	{
		bOutput = !msOutputs.Get(iPinNumber - 1);
		bPower = mmPinPowers.HasKey((int)iPinNumber);
		bNoConnect = mmPinNoConnects.HasKey((int)iPinNumber);

		if (!bPower && !bNoConnect)
		{
			if (bOutput)
			{
				if (!bOnlyActive)
				{
					PrintPin(psz, &msReadValues, iPinNumber);
				}
				else
				{
					PrintPinActive(psz, &msReadValues, iPinNumber);
				}
			}
			else
			{
				bBit = msReadValues.Get(iPinNumber - 1);
				bExpected = msWriteValues.Get(iPinNumber - 1);
				if (bBit != bExpected)
				{
					szPinName = GetPin(iPinNumber);
					psz->Append(szPinName);
					psz->Append(": Write[");
					if (bExpected)
					{
						psz->Append('1');
					}
					else
					{
						psz->Append('0');
					}
					psz->Append("] Read[");
					if (bBit)
					{
						psz->Append('1');
					}
					else
					{
						psz->Append('0');
					}
					psz->Append("]");
					psz->AppendNewLine();
					bFailed = true;
				}
			}
		}
	}
	return !bFailed;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBoardPins::PrintWrite(CChars* psz, bool bOnlyActive)
{
	size			iPinNumber;
	bool			bOutput;
	bool			bNoConnect;
	bool			bPower;
	SMapIterator	sIter;
	bool			bHasNext;
	CBusPins*		pcBus;
	char*			szBusName;

	psz->Append("Inputs into board");
	if (bOnlyActive)
	{
		psz->Append(" (only active)");
	}
	psz->AppendNewLine();

	bHasNext = mmBusses.StartIteration(&sIter, (void**)&szBusName, NULL, (void**)&pcBus, NULL);
	while (bHasNext)
	{
		if (IsBusWrite(pcBus))
		{
			PrintBus(psz, &msWriteValues, pcBus, szBusName);
		}
		bHasNext = mmBusses.Iterate(&sIter, (void**)&szBusName, NULL, (void**)&pcBus, NULL);
	}

	for (iPinNumber = 1; iPinNumber <= miNumPins; iPinNumber++)
	{
		bOutput = !msOutputs.Get(iPinNumber - 1);
		bPower = mmPinPowers.HasKey((int)iPinNumber);
		bNoConnect = mmPinNoConnects.HasKey((int)iPinNumber);

		if (!bPower && !bNoConnect)
		{
			if (!bOutput)
			{
				if (!bOnlyActive)
				{
					PrintPin(psz, &msWriteValues, iPinNumber);
				}
				else
				{
					PrintPinActive(psz, &msWriteValues, iPinNumber);
				}
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBoardPins::DumpRead(bool bOnlyActive)
{
	CChars	sz;
	
	sz.Init();
	PrintRead(&sz, bOnlyActive);
	sz.DumpKill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBoardPins::DumpWrite(bool bOnlyActive)
{
	CChars	sz;

	sz.Init();
	PrintWrite(&sz, bOnlyActive);
	sz.DumpKill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBusPins::Init(void)
{
	mmBusOffsets.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBusPins::Kill(void)
{
	mmBusOffsets.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
int CBusPins::Get(int iPinNumber)
{
	int* piBusOffset;

	piBusOffset = mmBusOffsets.Get(iPinNumber);
	if (piBusOffset)
	{
		return *piBusOffset;
	}
	else
	{
		return -1;
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBusPins::Set(int iPinNumber, int iBusOffset)
{
	mmBusOffsets.Put(iPinNumber, iBusOffset);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CBusPins::StartIteration(SMapIterator* psIter, int* piPinNumber, int* piBusOffset)
{
	bool	bHasNext;
	int*	pi1;
	int*	pi2;

	bHasNext = mmBusOffsets.StartIteration(psIter, (void**)&pi1, NULL, (void**)&pi2, NULL);
	if (bHasNext)
	{
		*piPinNumber = *pi1;
		*piBusOffset = *pi2;
	}
	return bHasNext;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CBusPins::Iterate(SMapIterator* psIter, int* piPinNumber, int* piBusOffset)
{
	bool	bHasNext;
	int*	pi1;
	int*	pi2;

	bHasNext = mmBusOffsets.Iterate(psIter, (void**)&pi1, NULL, (void**)&pi2, NULL);
	if (bHasNext)
	{
		*piPinNumber = *pi1;
		*piBusOffset = *pi2;
	}
	return bHasNext;
}

