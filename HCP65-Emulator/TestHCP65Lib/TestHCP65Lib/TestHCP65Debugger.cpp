#include <string.h>
#include "BaseLib/Chars.h"
#include "ThreadLib/SharedMemoryChannel.h"
#include "EmulatorDebugLib/DebugServer.h"
#include "EmulatorDebugLib/DebugClient.h"
#include "HCP65Lib/HCP65.h"
#include "HCP65Lib/HCP65DebugTarget.h"
#include "HCP65Lib/HCP65DebugPort.h"
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
// The kernal telling the debug port of uiProcess (running szName) started
// or ended, as KernalDebugTell writes it.
//
//////////////////////////////////////////////////////////////////////////
static void TestHCP65DebugTell(CHCP65DebugPort* pcPort, uint8 uiCommand, uint8 uiProcess, uint16 uiExitCode, const char* szName)
{
	size	i;

	pcPort->Write(HCP65_DEBUG_PORT_PROCESS, uiProcess);
	pcPort->Write(HCP65_DEBUG_PORT_EXIT_LOW, (uint8)uiExitCode);
	pcPort->Write(HCP65_DEBUG_PORT_EXIT_HIGH, (uint8)(uiExitCode >> 8));
	for (i = 0; i <= strlen(szName); i++)
	{
		pcPort->Write((uint32)(HCP65_DEBUG_PORT_NAME_BUFFER + i), (uint8)szName[i]);
	}
	pcPort->Write(HCP65_DEBUG_PORT_COMMAND, uiCommand);
}


//////////////////////////////////////////////////////////////////////////
//
// Ticks until the client has seen one more stop, whatever it is.
//
//////////////////////////////////////////////////////////////////////////
static void TestHCP65DebugWaitForAnyStop(CDebugServer* pcServer, CDebugClient* pcClient)
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
}


//////////////////////////////////////////////////////////////////////////
//
// The MPU looks at its mode's breakpoints, and in user mode the running
// process's too, once it has any; a process's go when they are cleared.
// The debug target says which space (and process) it is in.
//
//////////////////////////////////////////////////////////////////////////
static void TestHCP65DebugProcessBreakpoints(void)
{
	CHCP65				cHCP65;
	CHCP65DebugTarget	cTarget;
	CBreakpoints*		pcThree;
	static const uint8	auiNative[] = { 0x18, 0xFB, 0xDB };		//clc  xce  stp

	cHCP65.Init(TEST_HCP65_DEBUG_CLOCK_RATE);
	memcpy(&cHCP65.GetMPUROM()[0x8000], auiNative, sizeof(auiNative));
	cHCP65.GetMPUROM()[0xFFFC] = 0x00;
	cHCP65.GetMPUROM()[0xFFFD] = 0x80;
	cHCP65.Reset();
	cTarget.Init(&cHCP65, NULL);

	//Kernel mode (reset): the kernel's, and no process's.
	AssertTrue(cHCP65.IsKernelMode());
	AssertInt(1, cTarget.GetCurrentSpace());
	AssertIntHex(DEBUG_PROCESS_ANY, cTarget.GetCurrentProcess());
	AssertTrue(cHCP65.GetMPU()->GetUsedBreakpoints() == cHCP65.GetModeBreakpoints(true));
	AssertTrue(cHCP65.GetMPU()->GetMoreUsedBreakpoints() == NULL);
	pcThree = cHCP65.GetProcessBreakpoints(3);
	AssertTrue(pcThree->Add(0x012345, true));
	cHCP65.SetProcessID(3);
	AssertTrue(cHCP65.GetMPU()->GetMoreUsedBreakpoints() == NULL);

	//User mode (once native, with no interrupt): user mode's, and process 3's while it runs.
	cHCP65.Run(1000);
	AssertTrue(cHCP65.IsNativeLatched());
	cHCP65.SetInterruptDepth(0);
	AssertFalse(cHCP65.IsKernelMode());
	AssertTrue(cHCP65.GetMPU()->GetUsedBreakpoints() == cHCP65.GetModeBreakpoints(false));
	AssertTrue(cHCP65.GetMPU()->GetMoreUsedBreakpoints() == pcThree);
	AssertInt(2, cTarget.GetCurrentSpace());
	AssertInt(3, cTarget.GetCurrentProcess());
	cHCP65.SetProcessID(4);
	AssertTrue(cHCP65.GetMPU()->GetMoreUsedBreakpoints() == NULL);
	cHCP65.SetProcessID(3);
	AssertTrue(cHCP65.GetMPU()->GetMoreUsedBreakpoints() == pcThree);

	//Cleared (the process ended): none for it, and a new set for the next to have its ID.
	cHCP65.ClearProcessBreakpoints(3);
	AssertTrue(cHCP65.GetMPU()->GetMoreUsedBreakpoints() == NULL);
	AssertSize(0, cHCP65.GetProcessBreakpoints(3)->NumBreakpoints());
	AssertTrue(cHCP65.GetMPU()->GetMoreUsedBreakpoints() == cHCP65.GetProcessBreakpoints(3));

	cTarget.Kill();
	cHCP65.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A process started, told the debug port by the processor's store, stops
// Run after the store while the port is watched (and not otherwise); the
// port keeps it as running.
//
//	$8000	clc  xce
//	$8002	lda #1
//	$8004	sta $138400 (the port's command, in socket 7)
//	$8008	nop
//	$8009	stp
//
//////////////////////////////////////////////////////////////////////////
static void TestHCP65DebugPortStopRun(void)
{
	CHCP65				cHCP65;
	CHCP65DebugPort		cPort;
	EDebugProcessEvent	eEvent;
	SDebugProcess		sProcess;
	int					i;
	static const uint8	auiTell[] = { 0x18, 0xFB, 0xA9, 0x01, 0x8F, 0x00, 0x84, 0x13, 0xEA, 0xDB };

	for (i = 0; i < 2; i++)
	{
		cHCP65.Init(TEST_HCP65_DEBUG_CLOCK_RATE);
		cPort.Init();
		AssertTrue(cHCP65.SetDevice(7, &cPort));
		memcpy(&cHCP65.GetMPUROM()[0x8000], auiTell, sizeof(auiTell));
		memcpy(&cHCP65.GetMPURAM()[0x8000], auiTell, sizeof(auiTell));
		cHCP65.GetMPUROM()[0xFFFC] = 0x00;
		cHCP65.GetMPUROM()[0xFFFD] = 0x80;
		cHCP65.Reset();
		cPort.Write(HCP65_DEBUG_PORT_PROCESS, 0xFF);
		cPort.Watch(i == 0);

		cHCP65.Run(100000);
		if (i == 0)
		{
			AssertIntHex(0x8008, cHCP65.GetMPU()->GetProgramAddress());
			AssertFalse(cHCP65.IsStopped());
			AssertTrue(cPort.TakeEvent(&eEvent, &sProcess));
			AssertInt(DPE_Started, eEvent);
			AssertInt(0xFF, sProcess.uiProcess);
			AssertFalse(cPort.TakeEvent(&eEvent, &sProcess));
			cHCP65.Run(100000);
		}
		else
		{
			AssertFalse(cPort.TakeEvent(&eEvent, &sProcess));
		}
		AssertTrue(cHCP65.IsStopped());
		AssertSize(1, cPort.NumProcesses());
		AssertTrue(cPort.GetProcess(0, &sProcess));
		AssertInt(0xFF, sProcess.uiProcess);

		cHCP65.SetDevice(7, NULL);
		cPort.Kill();
		cHCP65.Kill();
	}
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
	CHCP65DebugPort			cPort;
	uint32					uiProcess;
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

	TestHCP65DebugProcessBreakpoints();
	TestHCP65DebugPortStopRun();

	cHCP65.Init(TEST_HCP65_DEBUG_CLOCK_RATE);
	cPort.Init();
	AssertTrue(cHCP65.SetDevice(7, &cPort));
	memcpy(&cHCP65.GetMPUROM()[0x8000], auiMain, sizeof(auiMain));
	memcpy(&cHCP65.GetMPURAM()[0x8000], auiMain, sizeof(auiMain));
	memcpy(&cHCP65.GetMPUROM()[0x8010], auiFunction, sizeof(auiFunction));
	memcpy(&cHCP65.GetMPURAM()[0x8010], auiFunction, sizeof(auiFunction));
	cHCP65.GetMPUROM()[0xFFFC] = 0x00;
	cHCP65.GetMPUROM()[0xFFFD] = 0x80;
	cHCP65.Reset();

	cTarget.Init(&cHCP65, &cPort);
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
	AssertInt(uiKernel, cClient.GetStopSpace());
	AssertIntHex(DEBUG_PROCESS_ANY, cClient.GetStopProcess());
	AssertTrue(cHCP65.GetModeBreakpoints(true)->Contains(0x8010));
	AssertFalse(cHCP65.GetModeBreakpoints(false)->Contains(0x8010));
	AssertTrue(cClient.RemoveBreakpoint(0x8010, uiKernel));
	AssertTrue(cClient.Continue());
	TestHCP65DebugWaitForStop(&cServer, &cClient, DS_Breakpoint, 0x8006);
	AssertLongLongIntHex(12, cClient.GetRegisterValue(D816R_X));

	//Processes: the kernal tells the debug port of one started while running.  It stops (DS_Process), and the
	//debugger is told its ID and program.
	AssertTrue(cClient.HasProcesses());
	AssertSize(0, cClient.NumProcesses());
	AssertTrue(cClient.RemoveBreakpoint(0x8006));
	AssertTrue(cClient.Continue());
	for (i = 0; (i < TEST_HCP65_DEBUG_TIMEOUT) && !cClient.IsRunning(); i++)
	{
		cServer.Tick(TEST_HCP65_DEBUG_CYCLES);
		cClient.Poll(1);
	}
	AssertTrue(cClient.IsRunning());
	TestHCP65DebugTell(&cPort, HCP65_DEBUG_PORT_STARTED, 3, 0, "\\test.exe");
	TestHCP65DebugWaitForAnyStop(&cServer, &cClient);
	AssertInt(DS_Process, cClient.GetStopReason());
	AssertSize(1, cClient.NumProcesses());
	AssertInt(3, cClient.GetProcess(0)->uiProcess);
	AssertString("\\test.exe", cClient.GetProcess(0)->szName);
	AssertTrue(cClient.FindProcess("\\TEST.EXE", &uiProcess));
	AssertInt(3, uiProcess);
	AssertFalse(cClient.FindProcess("\\other.exe", &uiProcess));

	//Its breakpoints are user mode's for it alone; kernel mode's can't be for a process.
	AssertTrue(cClient.AddBreakpoint(0x8003, true, uiUser, 3));
	AssertTrue(cClient.AddBreakpoint(0x8003, true, uiKernel, 3));
	TestHCP65DebugWait(&cServer, &cClient, &CDebugClient::NumErrors, 1);
	AssertString("No breakpoints for a process there.", cClient.GetError());
	AssertTrue(cHCP65.GetProcessBreakpoints(3)->Contains(0x8003));
	AssertFalse(cHCP65.GetModeBreakpoints(false)->Contains(0x8003));
	AssertFalse(cHCP65.GetModeBreakpoints(true)->Contains(0x8003));

	//Ended: the debugger is told its exit code, and its breakpoints go.
	TestHCP65DebugTell(&cPort, HCP65_DEBUG_PORT_ENDED, 3, 7, "\\test.exe");
	TestHCP65DebugWait(&cServer, &cClient, &CDebugClient::NumProcessEvents, 2);
	AssertSize(0, cClient.NumProcesses());
	AssertTrue(cClient.GetProcessEvent(0)->bStarted);
	AssertFalse(cClient.GetProcessEvent(1)->bStarted);
	AssertInt(3, cClient.GetProcessEvent(1)->sProcess.uiProcess);
	AssertInt(7, cClient.GetProcessEvent(1)->sProcess.iExitCode);
	AssertTrue(cClient.GetProcessEvent(2) == NULL);
	AssertSize(0, cHCP65.GetProcessBreakpoints(3)->NumBreakpoints());

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
	TestHCP65DebugWait(&cServer, &cClient, &CDebugClient::NumErrors, 2);
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
	cHCP65.SetDevice(7, NULL);
	cPort.Kill();
	cHCP65.Kill();

	TestStatistics();
}
