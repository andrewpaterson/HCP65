#include "AddressDecodeBoard.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SetupAddressDecode(CBoardPins* pcBoard)
{
	SetupStandard56PinBoard(pcBoard, "Address Decode", "V1");

	pcBoard->AddSignal(1, "Register11", PD_Output, PS_Inverted);
	pcBoard->AddSignal(2, "Register10", PD_Output, PS_Inverted);
	pcBoard->AddSignal(4, "Register9", PD_Output, PS_Inverted);
	pcBoard->AddSignal(5, "Register8", PD_Output, PS_Inverted);
	pcBoard->AddSignal(6, "Register7", PD_Output, PS_Inverted);
	pcBoard->AddSignal(7, "Register6", PD_Output, PS_Inverted);
	pcBoard->AddSignal(8, "Register5", PD_Output, PS_Inverted);
	pcBoard->AddSignal(10, "Register4", PD_Output, PS_Inverted);
	pcBoard->AddSignal(11, "Register3", PD_Output, PS_Inverted);
	pcBoard->AddSignal(12, "Register2", PD_Output, PS_Inverted);
	pcBoard->AddSignal(13, "Register1", PD_Output, PS_Inverted);
	pcBoard->AddSignal(14, "Register0", PD_Output, PS_Inverted);
	pcBoard->AddSignal(15, "Device19", PD_Output, PS_Inverted);
	pcBoard->AddSignal(16, "Device18", PD_Output, PS_Inverted);
	pcBoard->AddSignal(17, "Device17", PD_Output, PS_Inverted);
	pcBoard->AddSignal(18, "Device16", PD_Output, PS_Inverted);
	pcBoard->AddSignal(19, "Device7", PD_Output, PS_Inverted);
	pcBoard->AddSignal(21, "Device6", PD_Output, PS_Inverted);
	pcBoard->AddSignal(22, "Device5", PD_Output, PS_Inverted);
	pcBoard->AddSignal(23, "Device4", PD_Output, PS_Inverted);
	pcBoard->AddSignal(24, "Device3", PD_Output, PS_Inverted);
	pcBoard->AddSignal(25, "Device2", PD_Output, PS_Inverted);
	pcBoard->AddSignal(26, "Reset", PD_Input, PS_Inverted);
	pcBoard->AddSignal(27, "Device1", PD_Output, PS_Inverted);
	pcBoard->AddSignal(28, "Registers", PD_Output, PS_Inverted);

	pcBoard->AddSignal(29, "Devices", PD_Output, PS_Inverted);
	pcBoard->AddSignal(30, "RAM Swap", PD_Input, PS_Normal);
	pcBoard->AddSignal(32, "RAM", PD_Output, PS_Inverted);
	pcBoard->AddSignal(33, "ROM", PD_Output, PS_Inverted);
	pcBoard->AddNoCon(34);
	pcBoard->AddSignal(35, "Kernal Mode", PD_Input, PS_Normal);
	pcBoard->AddNoCon(36);
	pcBoard->AddSignal(38, "Main Memory", PD_Output, PS_Inverted);
	pcBoard->AddSignal(39, "A23", PD_Input, PS_Normal);
	pcBoard->AddSignal(40, "A22", PD_Input, PS_Normal);
	pcBoard->AddSignal(41, "A21", PD_Input, PS_Normal);
	pcBoard->AddSignal(42, "A20", PD_Input, PS_Normal);
	pcBoard->AddSignal(43, "A19", PD_Input, PS_Normal);
	pcBoard->AddSignal(44, "A18", PD_Input, PS_Normal);
	pcBoard->AddSignal(45, "A17", PD_Input, PS_Normal);
	pcBoard->AddSignal(46, "A16", PD_Input, PS_Normal);
	pcBoard->AddSignal(47, "A15", PD_Input, PS_Normal);
	pcBoard->AddSignal(49, "A3", PD_Input, PS_Normal);
	pcBoard->AddSignal(50, "A2", PD_Input, PS_Normal);
	pcBoard->AddSignal(51, "A1", PD_Input, PS_Normal);
	pcBoard->AddSignal(52, "A0", PD_Input, PS_Normal);
	pcBoard->AddSignal(53, "Register12", PD_Output, PS_Inverted);
	pcBoard->AddSignal(54, "Register13", PD_Output, PS_Inverted);
	pcBoard->AddSignal(55, "Register14", PD_Output, PS_Inverted);
	pcBoard->AddSignal(56, "Register15", PD_Output, PS_Inverted);

	pcBoard->GroupBus("Address", "A0", 0);
	pcBoard->GroupBus("Address", "A1", 1);
	pcBoard->GroupBus("Address", "A2", 2);
	pcBoard->GroupBus("Address", "A3", 3);
	pcBoard->GroupBus("Address", "A15", 15);
	pcBoard->GroupBus("Address", "A16", 16);
	pcBoard->GroupBus("Address", "A17", 17);
	pcBoard->GroupBus("Address", "A18", 18);
	pcBoard->GroupBus("Address", "A19", 19);
	pcBoard->GroupBus("Address", "A20", 20);
	pcBoard->GroupBus("Address", "A21", 21);
	pcBoard->GroupBus("Address", "A22", 22);
	pcBoard->GroupBus("Address", "A23", 23);

	pcBoard->Done();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool TestAddressDecode(CUART* pcUART, CBoardPins* pcBoard)
{
	CChars		szReadResult;
	CChars		szPrevRead;
	size		uiFlags;
	bool		bSuccess;
	CChars		szRead;
	CChars		szWriteCommand;
	size		uiTick;
	size		uiAddress;

	bSuccess = true;
	uiTick = 0;
	szPrevRead.Init();
	for (uiFlags = 0; uiFlags < 8; uiFlags++)
	{
		pcBoard->Set("Reset", uiFlags & 4);
		pcBoard->Set("Kernal Mode", uiFlags & 2);
		pcBoard->Set("RAM Swap", uiFlags & 1);

		uiAddress = 0x10'0000;
		for (;;)
		{
			pcBoard->SetBus("Address", (uint32)uiAddress);
			szWriteCommand.Init();
			pcBoard->GenerateWrite(&szWriteCommand);
			if (!pcUART->Send(szWriteCommand.Text()))
			{
				szWriteCommand.Kill();
				return false;
			}
			szWriteCommand.Kill();

			szReadResult.Init();
			if (!pcUART->Send("RR", &szReadResult))
			{
				szReadResult.Kill();
				return false;
			}
			pcBoard->UpdateRead(&szReadResult);
			szRead.Init();
			bSuccess = pcBoard->PrintRead(&szRead, true);
			if (!szRead.Equals(&szPrevRead))
			{
				EngineOutput("===========================\n");
				szPrevRead.Set(&szRead);
				pcBoard->DumpWrite(false);
				EngineOutput("\n");
				szRead.Dump();
				EngineOutput("===========================\n");
			}
			szRead.Kill();
			szReadResult.Kill();

			if (!bSuccess)
			{
				break;
			}

			if (uiAddress < 0x10'0010)
			{
				uiAddress += 0x0001;
			}
			else
			{
				if (uiAddress == 0x10'0010)
				{
					uiAddress = 0x10'8000;
				}
				else
				{
					uiAddress += 0x8000;
				}
			}
			if (uiAddress == 0x20'0000)
			{
				break;
			}
			uiTick++;
		}
		if (!bSuccess)
		{
			break;
		}
	}

	szPrevRead.Kill();

	if (bSuccess)
	{
		EngineOutput(SizeToString(uiTick));
		EngineOutput(" ");
		EngineOutput(BoolToString(bSuccess));
		EngineOutput("\n");
	}

	return bSuccess;
}

