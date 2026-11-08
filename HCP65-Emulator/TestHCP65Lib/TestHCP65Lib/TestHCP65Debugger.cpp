#include <stddef.h>
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
#define TEST_HCP65_DEBUG_MESSAGE		0x1000		//Where the WDM programs' message is, in bank 0.
#define TEST_HCP65_DEBUG_MEMORY_SOCKET	18


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
// The message telling of uiCommand.
//
//////////////////////////////////////////////////////////////////////////
static void TestHCP65DebugSetMessage(SHCP65DebugMessage* psMessage, uint8 uiCommand, uint8 uiValue, uint16 uiExitCode, uint32 uiAddress, uint32 uiSize, const char* szName)
{
	memset(psMessage, 0, sizeof(SHCP65DebugMessage));
	psMessage->uiCommand = uiCommand;
	psMessage->uiValue = uiValue;
	psMessage->uiExitCode = uiExitCode;
	psMessage->uiAddress = uiAddress;
	psMessage->uiSize = uiSize;
	strcpy(psMessage->szName, szName);
}


//////////////////////////////////////////////////////////////////////////
//
// The kernal telling the debug port of uiProcess (running szName) started
// or ended, as the emulator hears it.
//
//////////////////////////////////////////////////////////////////////////
static void TestHCP65DebugTell(CHCP65DebugPort* pcPort, uint8 uiCommand, uint8 uiProcess, uint16 uiExitCode, const char* szName)
{
	SHCP65DebugMessage	sMessage;

	TestHCP65DebugSetMessage(&sMessage, uiCommand, uiProcess, uiExitCode, 0, 0, szName);
	pcPort->Tell(&sMessage);
}


//////////////////////////////////////////////////////////////////////////
//
// The kernal telling the debug port of a driver it loaded (szName, for
// the board in uiSlot) at uiAddress, as the emulator hears it.
//
//////////////////////////////////////////////////////////////////////////
static void TestHCP65DebugTellDriver(CHCP65DebugPort* pcPort, uint8 uiSlot, uint32 uiAddress, uint32 uiSize, const char* szName)
{
	SHCP65DebugMessage	sMessage;

	TestHCP65DebugSetMessage(&sMessage, HCP65_DEBUG_DRIVER, uiSlot, 0, uiAddress, uiSize, szName);
	pcPort->Tell(&sMessage);
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
	cTarget.Init(&cHCP65);

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
// A machine whose kernel mode program, from $8000 in ROM and RAM, goes
// native and then runs auiProgram, with psMessage in MPU RAM at
// TEST_HCP65_DEBUG_MESSAGE.
//
//////////////////////////////////////////////////////////////////////////
static void TestHCP65DebugWDMMachine(CHCP65* pcHCP65, const uint8* auiProgram, size uiProgramSize, SHCP65DebugMessage* psMessage)
{
	static const uint8	auiNative[] = { 0x18, 0xFB };	//clc  xce

	pcHCP65->Init(TEST_HCP65_DEBUG_CLOCK_RATE);
	memcpy(&pcHCP65->GetMPUROM()[0x8000], auiNative, sizeof(auiNative));
	memcpy(&pcHCP65->GetMPURAM()[0x8000], auiNative, sizeof(auiNative));
	memcpy(&pcHCP65->GetMPUROM()[0x8002], auiProgram, uiProgramSize);
	memcpy(&pcHCP65->GetMPURAM()[0x8002], auiProgram, uiProgramSize);
	pcHCP65->GetMPUROM()[0xFFFC] = 0x00;
	pcHCP65->GetMPUROM()[0xFFFD] = 0x80;
	pcHCP65->Reset();
	memcpy(&pcHCP65->GetMPURAM()[TEST_HCP65_DEBUG_MESSAGE], psMessage, sizeof(SHCP65DebugMessage));
}


//////////////////////////////////////////////////////////////////////////
//
// A process started, told by the kernal's WDM, stops Run after the WDM
// while the port is watched (and not otherwise); the port keeps it as
// running, with the message's name.
//
//	$8000	clc  xce
//	$8002	rep #$30
//	$8004	lda #TEST_HCP65_DEBUG_MESSAGE
//	$8007	ldx #0
//	$800A	wdm #HCP65_DEBUG_WDM_SIGNATURE
//	$800C	nop
//	$800D	stp
//
//////////////////////////////////////////////////////////////////////////
static void TestHCP65DebugWDMStopRun(void)
{
	CHCP65				cHCP65;
	CHCP65DebugPort*	pcPort;
	SHCP65DebugMessage	sMessage;
	EDebugProcessEvent	eEvent;
	SDebugProcess		sProcess;
	int					i;
	static const uint8	auiTell[] = { 0xC2, 0x30, 0xA9, 0x00, 0x10, 0xA2, 0x00, 0x00, 0x42, HCP65_DEBUG_WDM_SIGNATURE, 0xEA, 0xDB };

	TestHCP65DebugSetMessage(&sMessage, HCP65_DEBUG_STARTED, 0xFF, 0, 0, 0, "\\boot.exe");
	for (i = 0; i < 2; i++)
	{
		TestHCP65DebugWDMMachine(&cHCP65, auiTell, sizeof(auiTell), &sMessage);
		pcPort = cHCP65.GetDebugPort();
		pcPort->Watch(i == 0);

		cHCP65.Run(100000);
		if (i == 0)
		{
			AssertIntHex(0x800C, cHCP65.GetMPU()->GetProgramAddress());
			AssertFalse(cHCP65.IsStopped());
			AssertTrue(pcPort->TakeEvent(&eEvent, &sProcess));
			AssertInt(DPE_Started, eEvent);
			AssertInt(0xFF, sProcess.uiProcess);
			AssertString("\\boot.exe", sProcess.szName);
			AssertFalse(pcPort->TakeEvent(&eEvent, &sProcess));
			cHCP65.Run(100000);
		}
		else
		{
			AssertFalse(pcPort->TakeEvent(&eEvent, &sProcess));
		}
		AssertTrue(cHCP65.IsStopped());
		AssertSize(1, pcPort->NumProcesses());
		AssertTrue(pcPort->GetProcess(0, &sProcess));
		AssertInt(0xFF, sProcess.uiProcess);
		AssertString("\\boot.exe", sProcess.szName);

		cHCP65.Kill();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A WDM that isn't for the debug port runs on as the no-op it is: another
// signature, or a message that isn't all in memory (its address is a
// device's), tells nothing.
//
//////////////////////////////////////////////////////////////////////////
static void TestHCP65DebugWDMIgnored(void)
{
	CHCP65				cHCP65;
	SHCP65DebugMessage	sMessage;
	static const uint8	auiSignature[] = { 0xC2, 0x30, 0xA9, 0x00, 0x10, 0xA2, 0x00, 0x00, 0x42, 0xD5, 0xDB };	//wdm #$D5
	static const uint8	auiDevice[] = { 0xC2, 0x30, 0xA9, 0x00, 0x00, 0xA2, 0x10, 0x00, 0x42, HCP65_DEBUG_WDM_SIGNATURE, 0xDB };	//ldx #$10: $100000.

	TestHCP65DebugSetMessage(&sMessage, HCP65_DEBUG_STARTED, 0xFF, 0, 0, 0, "\\boot.exe");

	TestHCP65DebugWDMMachine(&cHCP65, auiSignature, sizeof(auiSignature), &sMessage);
	cHCP65.GetDebugPort()->Watch(true);
	cHCP65.Run(100000);
	AssertTrue(cHCP65.IsStopped());
	AssertSize(0, cHCP65.GetDebugPort()->NumProcesses());
	cHCP65.Kill();

	TestHCP65DebugWDMMachine(&cHCP65, auiDevice, sizeof(auiDevice), &sMessage);
	cHCP65.GetDebugPort()->Watch(true);
	cHCP65.Run(100000);
	AssertTrue(cHCP65.IsStopped());
	AssertSize(0, cHCP65.GetDebugPort()->NumProcesses());
	cHCP65.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A process can't tell the debug port anything: in user mode WDM is a
// no-op, even with the signature and a message.  With a Memory Board
// (window 0, so process 0's bank 0 is its first bank), the same program
// runs from main memory once the interrupt depth says user mode.
//
//////////////////////////////////////////////////////////////////////////
static void TestHCP65DebugWDMUserMode(void)
{
	CHCP65				cHCP65;
	CHCP65MemoryBoard	cMemory;
	SHCP65DebugMessage	sMessage;
	static const uint8	auiTell[] = { 0xC2, 0x30, 0xA9, 0x00, 0x10, 0xA2, 0x00, 0x00, 0x42, HCP65_DEBUG_WDM_SIGNATURE, 0xDB };

	TestHCP65DebugSetMessage(&sMessage, HCP65_DEBUG_STARTED, 0xFF, 0, 0, 0, "\\boot.exe");
	TestHCP65DebugWDMMachine(&cHCP65, auiTell, sizeof(auiTell), &sMessage);
	cMemory.Init();
	AssertTrue(cHCP65.SetDevice(TEST_HCP65_DEBUG_MEMORY_SOCKET, &cMemory));
	memcpy(&cMemory.GetMemoryChip(0)[0x8002], auiTell, sizeof(auiTell));
	memcpy(&cMemory.GetMemoryChip(0)[TEST_HCP65_DEBUG_MESSAGE], &sMessage, sizeof(sMessage));

	cHCP65.GetDebugPort()->Watch(true);
	AssertTrue(cHCP65.GetMPU()->Step());	//clc
	AssertTrue(cHCP65.GetMPU()->Step());	//xce
	AssertTrue(cHCP65.IsNativeLatched());
	cHCP65.SetInterruptDepth(0);
	AssertFalse(cHCP65.IsKernelMode());
	cHCP65.Run(100000);
	AssertTrue(cHCP65.IsStopped());
	AssertSize(0, cHCP65.GetDebugPort()->NumProcesses());

	cHCP65.SetDevice(TEST_HCP65_DEBUG_MEMORY_SOCKET, NULL);
	cMemory.Kill();
	cHCP65.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The message's layout is the kernal's (TestHCP65Kernal checks they agree
// on the W65C816), and each of its commands does what it says: a process's
// name is kept for when it ends, a bank is filled in fill mode, a driver
// is kept until a reset, and a command it doesn't know does nothing.
//
//////////////////////////////////////////////////////////////////////////
static void TestHCP65DebugMessages(void)
{
	CHCP65				cHCP65;
	CHCP65MemoryBoard	cMemory;
	CHCP65DebugPort*	pcPort;
	SHCP65DebugMessage	sMessage;
	EDebugProcessEvent	eEvent;
	SDebugProcess		sProcess;

	AssertSize(0, offsetof(SHCP65DebugMessage, uiCommand));
	AssertSize(1, offsetof(SHCP65DebugMessage, uiValue));
	AssertSize(2, offsetof(SHCP65DebugMessage, uiExitCode));
	AssertSize(4, offsetof(SHCP65DebugMessage, uiAddress));
	AssertSize(8, offsetof(SHCP65DebugMessage, uiSize));
	AssertSize(12, offsetof(SHCP65DebugMessage, szName));
	AssertSize(12 + HCP65_DEBUG_NAME_SIZE, sizeof(SHCP65DebugMessage));

	cHCP65.Init(TEST_HCP65_DEBUG_CLOCK_RATE);
	cMemory.Init();
	AssertTrue(cHCP65.SetDevice(TEST_HCP65_DEBUG_MEMORY_SOCKET, &cMemory));
	cHCP65.Reset();
	pcPort = cHCP65.GetDebugPort();
	pcPort->Watch(true);

	//The name a process ended with is the one it started with; ended, it isn't running.
	TestHCP65DebugTell(pcPort, HCP65_DEBUG_STARTED, 5, 0, "\\five.exe");
	AssertSize(1, pcPort->NumProcesses());
	TestHCP65DebugTell(pcPort, HCP65_DEBUG_ENDED, 5, 0xFFFE, "");
	AssertSize(0, pcPort->NumProcesses());
	AssertTrue(pcPort->TakeEvent(&eEvent, &sProcess));
	AssertInt(DPE_Started, eEvent);
	AssertTrue(pcPort->TakeEvent(&eEvent, &sProcess));
	AssertInt(DPE_Ended, eEvent);
	AssertInt(5, sProcess.uiProcess);
	AssertInt(-2, sProcess.iExitCode);
	AssertString("\\five.exe", sProcess.szName);
	AssertFalse(pcPort->TakeEvent(&eEvent, &sProcess));

	//A bank given to a process is filled in fill mode, and only then.
	cMemory.GetMemoryChip(0)[0x10000] = 0x12;
	TestHCP65DebugSetMessage(&sMessage, HCP65_DEBUG_BANK, 1, 0, 0, 0, "");
	pcPort->Tell(&sMessage);
	AssertIntHex(0x12, cMemory.GetMemoryChip(0)[0x10000]);
	cHCP65.SetMemoryFill(true);
	pcPort->Tell(&sMessage);
	AssertIntHex(HCP65_DEBUG_FILL_BYTE, cMemory.GetMemoryChip(0)[0x10000]);

	//A driver is kept until a reset, which is an event as there was one.
	TestHCP65DebugTellDriver(pcPort, 4, 0x012345, 0x2E46, "\\OS\\Drivers\\UART.DRV");
	AssertSize(1, pcPort->NumDrivers());
	AssertInt(4, pcPort->GetDriver(0)->uiSlot);
	AssertIntHex(0x012345, pcPort->GetDriver(0)->uiAddress);
	AssertIntHex(0x2E46, pcPort->GetDriver(0)->uiSize);
	AssertString("\\OS\\Drivers\\UART.DRV", pcPort->GetDriver(0)->szName);
	AssertFalse(pcPort->TakeDriversCleared());
	cHCP65.Reset();
	AssertSize(0, pcPort->NumDrivers());
	AssertTrue(pcPort->TakeDriversCleared());
	AssertFalse(pcPort->TakeDriversCleared());

	//A name that fills the message is cut short.
	memset(&sMessage, 0, sizeof(sMessage));
	sMessage.uiCommand = HCP65_DEBUG_STARTED;
	sMessage.uiValue = 7;
	memset(sMessage.szName, 'x', HCP65_DEBUG_NAME_SIZE);
	pcPort->Tell(&sMessage);
	AssertTrue(pcPort->GetProcess(0, &sProcess));
	AssertSize(HCP65_DEBUG_NAME_SIZE - 1, strlen(sProcess.szName));

	//Nothing is done for a command it doesn't know.
	TestHCP65DebugSetMessage(&sMessage, 99, 9, 0, 0, 0, "\\nine.exe");
	pcPort->Tell(&sMessage);
	AssertSize(1, pcPort->NumProcesses());
	AssertSize(0, pcPort->NumDrivers());

	cHCP65.SetDevice(TEST_HCP65_DEBUG_MEMORY_SOCKET, NULL);
	cMemory.Kill();
	cHCP65.Kill();
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
	CHCP65DebugPort*		pcPort;
	uint32					uiProcess;
	SDebugModule*			psModule;
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
	TestHCP65DebugWDMStopRun();
	TestHCP65DebugWDMIgnored();
	TestHCP65DebugWDMUserMode();
	TestHCP65DebugMessages();

	cHCP65.Init(TEST_HCP65_DEBUG_CLOCK_RATE);
	pcPort = cHCP65.GetDebugPort();
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
	TestHCP65DebugTell(pcPort, HCP65_DEBUG_STARTED, 3, 0, "\\test.exe");
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
	TestHCP65DebugTell(pcPort, HCP65_DEBUG_ENDED, 3, 7, "\\test.exe");
	TestHCP65DebugWait(&cServer, &cClient, &CDebugClient::NumProcessEvents, 2);
	AssertSize(0, cClient.NumProcesses());
	AssertTrue(cClient.GetProcessEvent(0)->bStarted);
	AssertFalse(cClient.GetProcessEvent(1)->bStarted);
	AssertInt(3, cClient.GetProcessEvent(1)->sProcess.uiProcess);
	AssertInt(7, cClient.GetProcessEvent(1)->sProcess.iExitCode);
	AssertTrue(cClient.GetProcessEvent(2) == NULL);
	AssertSize(0, cHCP65.GetProcessBreakpoints(3)->NumBreakpoints());

	//Modules: a driver loaded while running stops it (DS_Module) before its code runs, and the debugger is told its
	//file, where it is and how big.  A reset drops it.
	AssertTrue(cClient.HasModules());
	AssertSize(0, cClient.NumModules());
	AssertTrue(cClient.Continue());
	for (i = 0; (i < TEST_HCP65_DEBUG_TIMEOUT) && !cClient.IsRunning(); i++)
	{
		cServer.Tick(TEST_HCP65_DEBUG_CYCLES);
		cClient.Poll(1);
	}
	AssertTrue(cClient.IsRunning());
	TestHCP65DebugTellDriver(pcPort, 4, 0x012345, 0x2E46, "\\OS\\Drivers\\UART.DRV");
	TestHCP65DebugWaitForAnyStop(&cServer, &cClient);
	AssertInt(DS_Module, cClient.GetStopReason());
	AssertSize(1, cClient.NumModules());
	AssertSize(1, cClient.NumModuleChanges());
	psModule = cClient.FindModule("\\os\\drivers\\uart.drv");
	AssertTrue(psModule != NULL);
	if (psModule)
	{
		AssertIntHex(0x012345, psModule->uiAddress);
		AssertIntHex(0x2E46, psModule->uiSize);
		AssertString("\\OS\\Drivers\\UART.DRV", psModule->szName);
	}
	AssertTrue(cClient.FindModule("\\OS\\Drivers\\SOUND.DRV") == NULL);
	pcPort->Reset();
	TestHCP65DebugWait(&cServer, &cClient, &CDebugClient::NumModuleChanges, 2);
	AssertSize(0, cClient.NumModules());

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
	cHCP65.Kill();

	TestStatistics();
}
