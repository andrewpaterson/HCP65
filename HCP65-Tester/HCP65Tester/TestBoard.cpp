#include "TestBoard.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SetupTestBoard(CBoardPins* pcBoard, size uiNumPins)
{
	size	i;
	CChars	sz;

	sz.Init("Test Board ");
	sz.Append(uiNumPins);
	pcBoard->SetName(sz.Text());
	sz.Kill();

	pcBoard->SetVersion("V1");

	for (i = 1; i <= uiNumPins; i++)
	{
		pcBoard->AddSignal(i, SizeToString(i), PD_Input, PS_Normal);  //Remember PD_Input is from the board under tests point of view not the testers point of view.
	}
	for (i = 1; i <= uiNumPins; i++)
	{
		pcBoard->GroupBus("Board", SizeToString(i), (int)i - 1);
	}


	pcBoard->Done();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void GenerateBoardTestWrite(CBoardPins* pcBoard, size uiStep, size uiOffset)
{
	size	uiNumPins;
	size	i;
	size	bState;
	size	uiSwap;

	uiNumPins = pcBoard->NumPins();

	bState = false;
	uiSwap = uiStep - uiOffset;
	for (i = 0; i < uiNumPins; i++)
	{
		if (i == uiSwap)
		{
			bState = !bState;
			uiSwap += uiStep;
		}

		pcBoard->Set(i + 1, bState);
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool TestTestBoard(CUART* pcUART, CBoardPins* pcBoard, size uiStep, size uiOffset)
{
	CChars	szResult;
	CChars	szWriteCommand;
	bool	bResult;
	uint32	uiValueRead1;
	uint32	uiValueRead2;
	uint32	uiValueWrite;

	GenerateBoardTestWrite(pcBoard, uiStep, uiOffset);

	szWriteCommand.Init();
	pcBoard->GenerateWrite(&szWriteCommand);
	uiValueWrite = pcBoard->GetBusWrite("Board");
	szResult.Init();
	bResult = pcUART->Send(szWriteCommand.Text(), &szResult);
	szWriteCommand.AppendNewLine();
	szWriteCommand.Kill();
	if (!bResult)
	{
		szResult.Kill();
		return false;
	}
	szResult.Kill();
	
	szResult.Init();
	bResult = pcUART->Send("RR", &szResult);
	if (!bResult)
	{
		szResult.Kill();
		return false;
	}
	pcBoard->UpdateRead(&szResult);
	uiValueRead1 = pcBoard->GetBus("Board");
	szResult.Kill();

	szResult.Init();
	bResult = pcUART->Send("RR", &szResult);
	if (!bResult)
	{
		szResult.Kill();
		return false;
	}
	pcBoard->UpdateRead(&szResult);
	uiValueRead2 = pcBoard->GetBus("Board");
	szResult.Kill();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool TestTestBoard(CUART* pcUART, CBoardPins* pcBoard, size uiStep)
{
	int		i;
	bool	bResult;

	for (i = 0; i <= (int)uiStep; i++)
	{
		bResult = TestTestBoard(pcUART, pcBoard, uiStep, i);
		if (!bResult)
		{
			return false;
		}
	}

	for (i = (int)uiStep - 1; i > 0; i--)
	{
		bResult = TestTestBoard(pcUART, pcBoard, uiStep, i);
		if (!bResult)
		{
			return false;
		}
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool TestTestBoard(CUART* pcUART, CBoardPins* pcBoard)
{
	//TestTestBoard(pcUART, pcBoard, 1);
	//TestTestBoard(pcUART, pcBoard, 2);
	//TestTestBoard(pcUART, pcBoard, 4);
	TestTestBoard(pcUART, pcBoard, 8);
	return true;
}

