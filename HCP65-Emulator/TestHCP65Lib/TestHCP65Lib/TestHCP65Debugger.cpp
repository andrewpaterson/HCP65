#include <string.h>
#include "BaseLib/Chars.h"
#include "ThreadLib/SharedMemoryChannel.h"
#include "EmulatorDebugLib/DebugServer.h"
#include "EmulatorDebugLib/DebugClient.h"
#include "HCP65Lib/HCP65.h"
#include "HCP65Lib/HCP65DebugTarget.h"
#include "TestLib/Assert.h"


#define TEST_HCP65_DEBUG_CLOCK_RATE		10000000
#define TEST_HCP65_DEBUG_CYCLES			1000		//The cycles each Tick runs.
#define TEST_HCP65_DEBUG_TIMEOUT		5000


//////////////////////////////////////////////////////////////////////////
//
// Ticks the server and polls the client until the client has seen one
// more stop, which must be at uiAddress for eReason.
//
//////////////////////////////////////////////////////////////////////////
static void TestHCP65DebugWaitForStop(CDebugServer* pcServer, CDebugClient* pcClient, EDebugStop eReason, uint32 uiAddress)
{
	size	uiStops;
	int		i;

	uiStops = pcClient->NumStops() + 1;
	for (i = 0; (i < TEST_HCP65_DEBUG_TIMEOUT) && (pcClient->NumStops() < uiStops); i++)
	{
		pcServer->Tick(TEST_HCP65_DEBUG_CYCLES);
		pcClient->Poll(1);
	}
	AssertSize(uiStops, pcClient->NumStops());
	AssertInt(eReason, pcClient->GetStopReason());
	AssertIntHex(uiAddress, pcClient->GetStopAddress());
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
static void TestHCP65DebugWait(CDebugServer* pcServer, CDebugClient* pcClient, size (CDebugClient::*fCount)(void), size uiCount)
{
	int		i;

	for (i = 0; (i < TEST_HCP65_DEBUG_TIMEOUT) && ((pcClient->*fCount)() < uiCount); i++)
	{
		pcServer->Tick(TEST_HCP65_DEBUG_CYCLES);
		pcClient->Poll(1);
	}
	AssertSize(uiCount, (pcClient->*fCount)());
}


//////////////////////////////////////////////////////////////////////////
//
// The HCP65 debugged through CHCP65DebugTarget: it boots from ROM, goes
// native (RAM at 0) and loops calling a function.
//
//	$8000	clc  xce
//	$8002	inx
//	$8003	jsr $8010
//	$8006	iny
//	$8007	bra $8002
//
//	$8010	inx
//	$8011	rts
//
//////////////////////////////////////////////////////////////////////////
void TestHCP65Debugger(void)
{
	CHCP65					cHCP65;
	CHCP65DebugTarget		cTarget;
	CSharedMemoryChannel	cServerChannel;
	CSharedMemoryChannel	cClientChannel;
	CDebugServer			cServer;
	CDebugClient			cClient;
	SDebugRange				sLine;
	uint32					uiInstructions;
	uint8					uiByte;
	int						i;
	static const uint8		auiMain[] = { 0x18, 0xFB, 0xE8, 0x20, 0x10, 0x80, 0xC8, 0x80, 0xF9 };
	static const uint8		auiFunction[] = { 0xE8, 0x60 };

	BeginTests();

	cHCP65.Init(TEST_HCP65_DEBUG_CLOCK_RATE);
	memcpy(&cHCP65.GetMPUROM()[0x8000], auiMain, sizeof(auiMain));
	memcpy(&cHCP65.GetMPURAM()[0x8000], auiMain, sizeof(auiMain));
	memcpy(&cHCP65.GetMPUROM()[0x8010], auiFunction, sizeof(auiFunction));
	memcpy(&cHCP65.GetMPURAM()[0x8010], auiFunction, sizeof(auiFunction));
	cHCP65.GetMPUROM()[0xFFFC] = 0x00;
	cHCP65.GetMPUROM()[0xFFFD] = 0x80;
	cHCP65.Reset();

	cTarget.Init(&cHCP65);
	cServerChannel.Init();
	cClientChannel.Init();
	AssertTrue(cServerChannel.Create("TestHCP65Debugger"));
	AssertTrue(cClientChannel.Connect("TestHCP65Debugger"));
	cServer.Init(&cServerChannel, &cTarget);
	cClient.Init(&cClientChannel);

	AssertTrue(cClient.Hello());
	TestHCP65DebugWaitForStop(&cServer, &cClient, DS_Start, 0x8000);
	AssertString("W65C816", cClient.GetProcessorName());

	//To the breakpoint after the call, once round the loop and again.
	AssertTrue(cClient.AddBreakpoint(0x8006, true));
	AssertTrue(cClient.Continue());
	TestHCP65DebugWaitForStop(&cServer, &cClient, DS_Breakpoint, 0x8006);
	AssertLongLongIntHex(2, cClient.GetRegisterValue(D816R_X));
	AssertLongLongIntHex(0, cClient.GetRegisterValue(D816R_E));
	AssertTrue(cHCP65.IsNativeLatched());

	//The return before the breakpoint isn't an instruction: once round is six.
	uiInstructions = cHCP65.GetInstructionCount();
	AssertTrue(cClient.Continue());
	TestHCP65DebugWaitForStop(&cServer, &cClient, DS_Breakpoint, 0x8006);
	AssertInt(uiInstructions + 6, cHCP65.GetInstructionCount());
	AssertLongLongIntHex(4, cClient.GetRegisterValue(D816R_X));

	//Over the line with the call, with the devices kept up with each step.
	AssertTrue(cClient.Step(2));
	TestHCP65DebugWaitForStop(&cServer, &cClient, DS_Step, 0x8002);
	sLine.uiStart = 0x8002;
	sLine.uiEnd = 0x8006;
	AssertTrue(cClient.StepRange(&sLine, 1, true));
	TestHCP65DebugWaitForStop(&cServer, &cClient, DS_Step, 0x8006);
	AssertLongLongIntHex(6, cClient.GetRegisterValue(D816R_X));

	//Memory through the map: RAM at 0 once native.  A device reads as $FF and isn't written.
	AssertTrue(cClient.ReadMemory(0x8000, sizeof(auiMain)));
	TestHCP65DebugWait(&cServer, &cClient, &CDebugClient::NumMemoryReplies, 1);
	AssertMemory(auiMain, cClient.GetMemory(), sizeof(auiMain));
	AssertTrue(cClient.ReadMemory(0x100008, 1));
	TestHCP65DebugWait(&cServer, &cClient, &CDebugClient::NumMemoryReplies, 2);
	AssertSize(1, cClient.GetMemorySize());
	AssertIntHex(0xFF, cClient.GetMemory()[0]);
	uiByte = 0x55;
	AssertTrue(cClient.WriteMemory(0x100008, &uiByte, 1));
	TestHCP65DebugWait(&cServer, &cClient, &CDebugClient::NumErrors, 1);
	AssertInt(DM_WriteMemory, cClient.GetErrorRequest());
	AssertInt(0, cHCP65.GetProcessID());

	//Loaded into RAM: the loop now ends at stp.
	uiByte = 0xDB;
	AssertTrue(cClient.Load(0x8006, &uiByte, 1));
	AssertTrue(cClient.Continue());
	TestHCP65DebugWaitForStop(&cServer, &cClient, DS_Stopped, 0x8007);
	AssertTrue(cHCP65.IsStopped());

	AssertTrue(cClient.Detach());
	for (i = 0; (i < TEST_HCP65_DEBUG_TIMEOUT) && !cServer.IsClosed(); i++)
	{
		cServer.Tick(TEST_HCP65_DEBUG_CYCLES);
	}
	AssertTrue(cServer.IsClosed());
	AssertSize(0, cHCP65.GetMPU()->GetBreakpoints()->NumBreakpoints());

	cClient.Kill();
	cServer.Kill();
	cClientChannel.Kill();
	cServerChannel.Kill();
	cTarget.Kill();
	cHCP65.Kill();

	TestStatistics();
}
