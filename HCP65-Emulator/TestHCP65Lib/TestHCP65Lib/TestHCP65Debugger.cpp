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
	uint32					uiKernel;
	uint32					uiUser;
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
	AssertInt(DAF_Bank, cClient.GetAddressFormat());

	//After the processor's registers, the process the MMU maps for: the kernal's (zero) until it runs one.
	AssertSize(HCP65_DEBUG_NUM_REGISTERS, cClient.NumRegisters());
	AssertString("PID", cClient.GetRegister(HCP65_DEBUG_REGISTER_PROCESS)->szName);
	AssertInt(8, cClient.GetRegister(HCP65_DEBUG_REGISTER_PROCESS)->uiBits);
	AssertLongLongIntHex(0, cClient.GetRegisterValue(HCP65_DEBUG_REGISTER_PROCESS));
	AssertFalse(cTarget.SetRegisterValue(HCP65_DEBUG_REGISTER_PROCESS, 3));
	AssertTrue(cClient.CanDecode());

	//The emulator disassembles with the assembler's knowledge, in emulation mode (8 bit immediates).
	AssertTrue(cClient.Disassemble(0x8000, 4));
	TestHCP65DebugWait(&cServer, &cClient, &CDebugClient::NumDisassemblies, 1);
	AssertSize(4, cClient.NumInstructions());
	AssertString("clc", cClient.GetInstruction(0)->szText);
	AssertString("xce", cClient.GetInstruction(1)->szText);
	AssertString("inx", cClient.GetInstruction(2)->szText);
	AssertString("jsr $8010", cClient.GetInstruction(3)->szText);
	AssertInt(DI_Call, cClient.GetInstruction(3)->uiKind);
	AssertIntHex(0x8010, cClient.GetInstruction(3)->uiTarget);

	//Breakpoint spaces: kernel mode and user mode.  It stays in kernel mode (the reset counts as an interrupt, and
	//nothing returns from it), so a user mode breakpoint doesn't stop it.
	AssertSize(2, cClient.NumBreakpointSpaces());
	AssertString("Kernel", cClient.GetBreakpointSpaceName(0));
	AssertString("User", cClient.GetBreakpointSpaceName(1));
	AssertTrue(cClient.FindBreakpointSpace("kernel", &uiKernel));
	AssertInt(1, uiKernel);
	AssertTrue(cClient.FindBreakpointSpace("User", &uiUser));
	AssertInt(2, uiUser);
	AssertFalse(cClient.FindBreakpointSpace("Supervisor", &uiUser));
	AssertTrue(cClient.AddBreakpoint(0x8002, true, uiUser));

	//To the breakpoint after the call (in every space), once round the loop and again.
	AssertTrue(cClient.AddBreakpoint(0x8006, true));
	AssertTrue(cClient.Continue());
	TestHCP65DebugWaitForStop(&cServer, &cClient, DS_Breakpoint, 0x8006);
	AssertLongLongIntHex(2, cClient.GetRegisterValue(D816R_X));
	AssertLongLongIntHex(0, cClient.GetRegisterValue(D816R_E));
	AssertTrue(cHCP65.IsNativeLatched());
	AssertTrue(cHCP65.IsKernelMode());

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

	//Over the jsr, and into it and out again.
	AssertTrue(cClient.Step(3));
	TestHCP65DebugWaitForStop(&cServer, &cClient, DS_Step, 0x8003);
	AssertTrue(cClient.StepOver());
	TestHCP65DebugWaitForStop(&cServer, &cClient, DS_Step, 0x8006);
	AssertLongLongIntHex(8, cClient.GetRegisterValue(D816R_X));
	AssertTrue(cClient.Step(4));
	TestHCP65DebugWaitForStop(&cServer, &cClient, DS_Step, 0x8010);
	AssertTrue(cClient.StepOut());
	TestHCP65DebugWaitForStop(&cServer, &cClient, DS_Step, 0x8006);
	AssertLongLongIntHex(10, cClient.GetRegisterValue(D816R_X));

	//A kernel mode breakpoint stops it in the function; removed, it goes on to the one after the call.
	AssertTrue(cClient.AddBreakpoint(0x8010, true, uiKernel));
	AssertTrue(cClient.Continue());
	TestHCP65DebugWaitForStop(&cServer, &cClient, DS_Breakpoint, 0x8010);
	AssertLongLongIntHex(11, cClient.GetRegisterValue(D816R_X));
	AssertTrue(cHCP65.GetModeBreakpoints(true)->Contains(0x8010));
	AssertFalse(cHCP65.GetModeBreakpoints(false)->Contains(0x8010));
	AssertTrue(cClient.RemoveBreakpoint(0x8010, uiKernel));
	AssertTrue(cClient.Continue());
	TestHCP65DebugWaitForStop(&cServer, &cClient, DS_Breakpoint, 0x8006);
	AssertLongLongIntHex(12, cClient.GetRegisterValue(D816R_X));

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
	AssertInt(DM_WriteMemory, cClient.GetErrorType());
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
	AssertSize(0, cHCP65.GetModeBreakpoints(true)->NumBreakpoints());
	AssertSize(0, cHCP65.GetModeBreakpoints(false)->NumBreakpoints());

	cClient.Kill();
	cServer.Kill();
	cClientChannel.Kill();
	cServerChannel.Kill();
	cTarget.Kill();
	cHCP65.Kill();

	TestStatistics();
}
