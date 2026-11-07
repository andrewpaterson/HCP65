#include <stdio.h>
#include <string.h>
#include "BaseLib/Chars.h"
#include "BaseLib/DataMemory.h"
#include "BaseLib/FileUtil.h"
#include "BaseLib/XMLFile.h"
#include "DeviceRomLib/DeviceROMSchematic.h"
#include "HCP65Lib/HCP65.h"
#include "HCP65Lib/HCP65Audio.h"
#include "HCP65Lib/HCP65SDCard.h"
#include "HCP65Lib/HCP65UART.h"
#include "TestLib/Assert.h"


#define TEST_HCP65_CLOCK_RATE	10000000
#define TEST_HCP65_BOARDS		".." _FS_ ".." _FS_ ".." _FS_ "DeviceROM" _FS_ "Boards"
#define TEST_HCP65_KICAD		".." _FS_ ".." _FS_ ".." _FS_ "Kicad" _FS_ "Computer"
#define TEST_HCP65_MEMORY_SOCKET	18		//Socket V: the channel's register 0, D7-D6.


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
static void TestHCP65Put(uint8* puiMemory, uint32 uiAddress, const uint8* puiBytes, size uiLength)
{
	memcpy(&puiMemory[uiAddress], puiBytes, uiLength);
}


//////////////////////////////////////////////////////////////////////////
//
// A memory board in the test's socket (V, device 18): in window 0 until
// the channel says otherwise, so main memory is $000000 to $3FFFFF, the
// kernel mode part from $200000.
//
//////////////////////////////////////////////////////////////////////////
static void TestHCP65AddMemory(CHCP65* pcHCP65, CHCP65MemoryBoard* pcMemory)
{
	pcMemory->Init();
	AssertTrue(pcHCP65->SetDevice(TEST_HCP65_MEMORY_SOCKET, pcMemory));
}


//////////////////////////////////////////////////////////////////////////
//
// The byte of main memory at the physical address uiAddress (there must
// be memory there).
//
//////////////////////////////////////////////////////////////////////////
static uint8* TestHCP65Main(CHCP65* pcHCP65, uint32 uiAddress)
{
	uint8*	puiMain;

	puiMain = pcHCP65->MainMemoryAt(uiAddress);
	AssertNotNull(puiMain);
	return puiMain;
}


//////////////////////////////////////////////////////////////////////////
//
// A program in ROM and the same in RAM at its address (as the start up's
// copy would leave it), started from $8000 by the reset vector.
//
//////////////////////////////////////////////////////////////////////////
static void TestHCP65Program(CHCP65* pcHCP65, const uint8* puiProgram, size uiLength)
{
	uint8*	puiROM;
	uint8*	puiRAM;

	puiROM = pcHCP65->GetMPUROM();
	puiRAM = pcHCP65->GetMPURAM();
	TestHCP65Put(puiROM, 0x8000, puiProgram, uiLength);
	TestHCP65Put(puiRAM, 0x8000, puiProgram, uiLength);
	puiROM[0xFFFC] = 0x00;
	puiROM[0xFFFD] = 0x80;
}


//////////////////////////////////////////////////////////////////////////
//
// The reset vector and the code before the first xce come from ROM (ROM
// is at 0); xce swaps RAM to 0.  RTI from kernel mode enters user mode (the
// interrupt depth goes to 0): the pulls come from the user's stack, through
// the process's remap table.  BRK goes back to kernel mode, pushing on the
// user's stack.
//
//////////////////////////////////////////////////////////////////////////
void TestHCP65BootAndUserMode(void)
{
	CHCP65				cHCP65;
	CHCP65MemoryBoard	cMemory;
	uint8*				puiROM;
	uint8*				puiRAM;
	uint8*				puiMain;

	static const uint8 auiKernal[] = {
		0x18, 0xFB,					//8000 clc  xce
		0xA9, 0x22,					//8002 lda #$22 (ROM's copy has #$11)
		0x8F, 0x00, 0x00, 0x20,		//8004 sta $200000
		0xA9, 0xFE,					//8008 lda #$FE
		0x8F, 0x08, 0x00, 0x10,		//800A sta $100008 (process ID)
		0xA2, 0xF0,					//800E ldx #$F0
		0x9A,						//8010 txs
		0x40 };						//8011 rti
	static const uint8 auiUser[] = {
		0xA9, 0x33,					//lda #$33
		0x8D, 0x34, 0x12,			//sta $1234
		0x00, 0x00 };				//brk

	cHCP65.Init(TEST_HCP65_CLOCK_RATE);
	puiROM = cHCP65.GetMPUROM();
	puiRAM = cHCP65.GetMPURAM();
	TestHCP65AddMemory(&cHCP65, &cMemory);
	puiMain = TestHCP65Main(&cHCP65, 0x220000);
	TestHCP65Program(&cHCP65, auiKernal, sizeof(auiKernal));
	puiROM[0x8003] = 0x11;
	puiRAM[0x8100] = 0xDB;									//stp
	puiRAM[0xFFE6] = 0x00;									//Native BRK vector.
	puiRAM[0xFFE7] = 0x81;
	puiRAM[HCP65_REMAP_TABLE + 0xFE00 + 0] = 0x22;			//Process $FE's bank 0 is bank $22.

	TestHCP65Put(puiMain, 0x9000, auiUser, sizeof(auiUser));
	puiMain[0x00F1] = 0x34;	//P
	puiMain[0x00F2] = 0x00;	//PCL
	puiMain[0x00F3] = 0x90;	//PCH
	puiMain[0x00F4] = 0x00;	//PBR

	cHCP65.Reset();
	AssertTrue(cHCP65.IsKernelMode());
	AssertInt(1, cHCP65.GetInterruptDepth());
	AssertFalse(cHCP65.IsNativeLatched());

	cHCP65.Run(1000);
	AssertTrue(cHCP65.IsStopped());
	AssertTrue(cHCP65.IsNativeLatched());
	AssertInt(0x22, *TestHCP65Main(&cHCP65, 0x200000));
	AssertInt(0xFE, cHCP65.GetProcessID());
	AssertInt(0x33, puiMain[0x1234]);
	AssertInt(1, cHCP65.GetInterruptDepth());
	AssertTrue(cHCP65.IsKernelMode());
	AssertInt(0x00, puiMain[0x00F4]);
	AssertInt(0x90, puiMain[0x00F3]);
	AssertInt(0x07, puiMain[0x00F2]);
	AssertInt(0x8101, cHCP65.GetMPU()->GetProgramCounter());
	cHCP65.Kill();
	cMemory.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Native mode, interrupts on, then waits; uiIrqHandler and uiNmiHandler
// are the native vectors.
//
//////////////////////////////////////////////////////////////////////////
static uint8* TestHCP65WaitingKernal(CHCP65* pcHCP65, uint16 uiIrqHandler, uint16 uiNmiHandler)
{
	uint8*	puiRAM;

	static const uint8 auiProgram[] = {
		0x18, 0xFB,					//8000 clc  xce
		0x58,						//8002 cli
		0xCB,						//8003 wai
		0x80, 0xFD };				//8004 bra 8003

	TestHCP65Program(pcHCP65, auiProgram, sizeof(auiProgram));
	puiRAM = pcHCP65->GetMPURAM();
	puiRAM[0xFFEE] = (uint8)uiIrqHandler;
	puiRAM[0xFFEF] = (uint8)(uiIrqHandler >> 8);
	puiRAM[0xFFEA] = (uint8)uiNmiHandler;
	puiRAM[0xFFEB] = (uint8)(uiNmiHandler >> 8);
	return puiRAM;
}


//////////////////////////////////////////////////////////////////////////
//
// The sound board in a 128KB socket: its ROM, registers and sample RAMs,
// the samples it plays, and its interrupt on the sample tick that reaches
// the interrupt address.
//
//////////////////////////////////////////////////////////////////////////
void TestHCP65AudioBoard(void)
{
	CHCP65				cHCP65;
	CHCP65Audio			cAudio;
	CHCP65MemoryBoard	cMemory;
	uint8*				puiRAM;
	uint8				auiFrames[64];
	uint32		uiFrames;
	uint64		uiEnableCycle;
	uint64		uiDue;
	int			i;

	static const uint8 auiIrq[] = {
		0xAF, 0x00, 0x04, 0x18,		//lda $180400 (flags)
		0x8F, 0x10, 0x00, 0x20,		//sta $200010
		0xAF, 0x02, 0x04, 0x18,		//lda $180402 (counter low)
		0x8F, 0x11, 0x00, 0x20,		//sta $200011
		0xDB };						//stp

	cHCP65.Init(TEST_HCP65_CLOCK_RATE);
	TestHCP65AddMemory(&cHCP65, &cMemory);
	cAudio.Init();
	AssertFalse(cHCP65.SetDevice(3, &cAudio));
	AssertTrue(cHCP65.SetDevice(16, &cAudio));
	puiRAM = TestHCP65WaitingKernal(&cHCP65, 0x8200, 0x8300);
	TestHCP65Put(puiRAM, 0x8200, auiIrq, sizeof(auiIrq));
	for (i = 0; i < HCP65_AUDIO_RAM_SIZE; i++)
	{
		cAudio.GetRAMA()[i] = (uint8)i;
		cAudio.GetRAMB()[i] = (uint8)(255 - i);
	}

	cHCP65.Reset();
	AssertTrue(cAudio.IsHeldInReset());
	cHCP65.Write(0x100005, 0x01);
	AssertFalse(cAudio.IsHeldInReset());
	AssertInt(0x01, cHCP65.Read(0x100005));
	AssertInt(0x1A, cHCP65.Read(0x180000));		//The device ROM's magic, $C0DAFE1A.
	cHCP65.Write(0x180402, 0x10);
	cHCP65.Write(0x180403, 0x00);
	cHCP65.Write(0x180404, 0x13);
	cHCP65.Write(0x180405, 0x00);
	cHCP65.Write(0x180406, 0x12);
	cHCP65.Write(0x180407, 0x00);
	uiEnableCycle = cHCP65.NumCycles();
	cHCP65.Write(0x180400, 0xC0);
	AssertInt(0x40, cHCP65.Read(0x180420));
	AssertInt(32768, cAudio.GetSampleRate());

	cHCP65.Run(100000);
	AssertTrue(cHCP65.IsStopped());
	AssertInt(0xC0, *TestHCP65Main(&cHCP65, 0x200010));
	AssertInt(0x12, *TestHCP65Main(&cHCP65, 0x200011));
	uiDue = cHCP65.CycleOfCrystalTick(cHCP65.CrystalTick(uiEnableCycle) + 2);
	AssertTrue(cHCP65.NumCycles() >= uiDue);
	AssertTrue(cHCP65.NumCycles() < uiDue + 60);
	AssertInt(0, cHCP65.Read(0x100002) & 0x01);

	uiFrames = cAudio.ReadFrames(auiFrames, 32);
	AssertInt(2, uiFrames);
	AssertInt(0x10, auiFrames[0]);
	AssertInt(0xEF, auiFrames[1]);
	AssertInt(0x11, auiFrames[2]);
	AssertInt(0xEE, auiFrames[3]);
	cHCP65.Write(0x180401, 0);
	AssertFalse(cAudio.IsInterrupting());
	AssertInt(1, cHCP65.Read(0x100002) & 0x01);

	//The loop 10, 11, 12, 13, 10 ... at 8192Hz.
	cHCP65.Write(0x180400, 0x42);
	cAudio.CatchUp(cHCP65.NumCycles() + cHCP65.CycleOfCrystalTick(4 * 8) - 1);
	uiFrames = cAudio.ReadFrames(auiFrames, 32);
	AssertTrue((uiFrames >= 7) && (uiFrames <= 8));
	AssertInt(0x12, auiFrames[0]);
	AssertInt(0x13, auiFrames[2]);
	AssertInt(0x10, auiFrames[4]);
	AssertInt(0x11, auiFrames[6]);

	cHCP65.Kill();
	cAudio.Kill();
	cMemory.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The program itself enables the sound board and its interrupt, inside one
// long Run: the interrupt comes on the 100th sample.
//
//////////////////////////////////////////////////////////////////////////
void TestHCP65AudioFromProgram(void)
{
	CHCP65		cHCP65;
	CHCP65Audio	cAudio;
	uint8*		puiRAM;

	static const uint8 auiProgram[] = {
		0x18, 0xFB,								//clc  xce
		0xA9, 0x02, 0x8F, 0x05, 0x00, 0x10,		//lda #2  sta $100005 (enable device 17)
		0xA9, 0x00, 0x8F, 0x02, 0x04, 0x1A,		//lda #0  sta $1A0402 (start)
		0xA9, 0x64, 0x8F, 0x06, 0x04, 0x1A,		//lda #100  sta $1A0406 (interrupt)
		0xA9, 0xC3, 0x8F, 0x00, 0x04, 0x1A,		//lda #$C3  sta $1A0400 (enable, interrupt, 4096Hz)
		0x58,									//cli
		0xCB, 0x80, 0xFD };						//wai  bra -3

	cHCP65.Init(TEST_HCP65_CLOCK_RATE);
	cAudio.Init();
	cHCP65.SetDevice(17, &cAudio);
	TestHCP65Program(&cHCP65, auiProgram, sizeof(auiProgram));
	puiRAM = cHCP65.GetMPURAM();
	puiRAM[0xFFEE] = 0x00;
	puiRAM[0xFFEF] = 0x82;
	puiRAM[0x8200] = 0xDB;
	cHCP65.Reset();
	cHCP65.Run(10000000);
	AssertTrue(cHCP65.IsStopped());
	AssertInt(100, cAudio.GetCounter());
	AssertInt(100, cAudio.NumFrames());
	AssertTrue(cHCP65.NumCycles() > cHCP65.CycleOfCrystalTick(796));
	AssertTrue(cHCP65.NumCycles() < cHCP65.CycleOfCrystalTick(804));
	cHCP65.Kill();
	cAudio.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The NMI timer starts when the processor first goes native and latches
// its NMI source every 2^(21 - 2 * speed) crystal ticks.
//
//////////////////////////////////////////////////////////////////////////
void TestHCP65Timer(void)
{
	CHCP65	cHCP65;
	uint8*	puiRAM;
	uint64	uiDue;

	cHCP65.Init(TEST_HCP65_CLOCK_RATE);
	puiRAM = TestHCP65WaitingKernal(&cHCP65, 0x8200, 0x8300);
	puiRAM[0x8300] = 0xDB;
	cHCP65.Reset();
	cHCP65.Write(0x100006, 7);
	cHCP65.Run(2000000);
	AssertTrue(cHCP65.IsStopped());
	AssertInt(0x11, cHCP65.Read(0x100007) & 0x11);
	uiDue = cHCP65.CycleOfCrystalTick(128);
	AssertTrue(cHCP65.NumCycles() >= uiDue);
	AssertTrue(cHCP65.NumCycles() < uiDue + 60);
	cHCP65.Write(0x100007, 0x01);
	AssertInt(0, cHCP65.Read(0x100007) & 0x01);
	cHCP65.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A processor access to main memory while a device is using it (the SD
// Card Board's DMA) latches contention: main memory is shut off (the store
// is lost, reads float to $FF) and NMI fires after the instruction.
//
//////////////////////////////////////////////////////////////////////////
void TestHCP65Contention(void)
{
	CHCP65				cHCP65;
	CHCP65MemoryBoard	cMemory;
	uint8*				puiRAM;

	static const uint8 auiKernal[] = {
		0x18, 0xFB,					//8000 clc  xce
		0xA9, 0x55,					//8002 lda #$55
		0x8F, 0x00, 0x00, 0x20,		//8004 sta $200000
		0xAF, 0x00, 0x00, 0x20,		//8008 lda $200000
		0x8D, 0x00, 0x83,			//800C sta $8300
		0x80, 0xFE };				//800F bra $800F

	cHCP65.Init(TEST_HCP65_CLOCK_RATE);
	TestHCP65AddMemory(&cHCP65, &cMemory);
	TestHCP65Program(&cHCP65, auiKernal, sizeof(auiKernal));
	puiRAM = cHCP65.GetMPURAM();
	puiRAM[0x8100] = 0xDB;
	puiRAM[0xFFEA] = 0x00;
	puiRAM[0xFFEB] = 0x81;
	cHCP65.Reset();
	AssertFalse(cHCP65.IsContention());

	cHCP65.SetMainMemoryBusy(1000);
	cHCP65.Run(2000);
	AssertTrue(cHCP65.IsContention());
	AssertTrue(cHCP65.IsStopped());
	AssertInt(0x8101, cHCP65.GetMPU()->GetProgramAddress() & 0xFFFF);
	AssertInt(0, *TestHCP65Main(&cHCP65, 0x200000));
	AssertInt(0xFF, cHCP65.Read(0x200000));

	//Only a reset clears it.
	cHCP65.Reset();
	AssertFalse(cHCP65.IsContention());
	cHCP65.Kill();
	cMemory.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Fill mode: memory is filled at reset, and a main memory bank when the
// kernal's debug port says it was given to a process.  Off, nothing is.
//
//////////////////////////////////////////////////////////////////////////
void TestHCP65MemoryFill(void)
{
	CHCP65				cHCP65;
	CHCP65MemoryBoard	cMemory;
	uint8*				puiBank;

	cHCP65.Init(TEST_HCP65_CLOCK_RATE);
	TestHCP65AddMemory(&cHCP65, &cMemory);
	puiBank = TestHCP65Main(&cHCP65, 0x300000);
	cHCP65.Reset();
	AssertInt(0, puiBank[0]);
	cHCP65.FillMainMemoryBank(0x30);
	AssertInt(0, puiBank[0]);

	cHCP65.SetMemoryFill(true);
	cHCP65.Reset();
	AssertInt(HCP65_DEBUG_FILL_BYTE, puiBank[0]);
	AssertInt(HCP65_DEBUG_FILL_BYTE, cHCP65.GetMPURAM()[0x1234]);
	puiBank[0xFFFF] = 0;
	puiBank[0x10000] = 0;
	cHCP65.FillMainMemoryBank(0x30);
	AssertInt(HCP65_DEBUG_FILL_BYTE, puiBank[0xFFFF]);
	AssertInt(0, puiBank[0x10000]);
	cHCP65.FillMainMemoryBank(0x80);  //No memory there.
	cHCP65.Kill();
	cMemory.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The memory boards and the main memory channel: each socket's 4MB in the
// window its registers give it (chip A the lower 2MB), nothing where no
// chip answers, the first socket's where two share a window.  The
// registers repeat every 8 bytes and read nothing; the device enables
// don't reach them, and nor does the system reset: they are latches.
//
//////////////////////////////////////////////////////////////////////////
void TestHCP65MemoryBoard(void)
{
	CHCP65				cHCP65;
	CHCP65MemoryBoard	cV;
	CHCP65MemoryBoard	cX;

	cHCP65.Init(TEST_HCP65_CLOCK_RATE);
	cV.Init();
	cX.Init();
	AssertFalse(cHCP65.SetDevice(HCP65_MEMORY_CHANNEL_SLOT, &cV));
	AssertTrue(cHCP65.SetDevice(18, &cV));
	AssertTrue(cHCP65.SetDevice(3, &cX));
	AssertInt(0x1A, cHCP65.Read(0x118000));		//X's device ROM: its magic, $C0DAFE1A.
	AssertInt(DEVICE_MAIN_BUS_WINDOW_A | DEVICE_MAIN_BUS_WINDOW_B, cX.Read(DEVICE_ROM_MAIN_BUS_OFFSET));

	//Both in window 0: V answers, before X in the channel's order.
	AssertTrue(cHCP65.MainMemoryAt(0x000000) == cV.GetMemoryChip(0));
	AssertTrue(cHCP65.MainMemoryAt(0x3FFFFF) == cV.GetMemoryChip(1) + 0x1FFFFF);
	AssertTrue(cHCP65.MainMemoryAt(0x400000) == NULL);
	AssertInt(0xFF, cHCP65.Read(0x400000));

	//V in window 1 (register 0, D7-D6), X in window 2 (register 1, D3-D2).
	cHCP65.Write(0x108000, 1 << 6);
	cHCP65.Write(0x108001, 2 << 2);
	AssertInt(0xFF, cHCP65.Read(0x108000));
	AssertTrue(cHCP65.MainMemoryAt(0x200000) == NULL);
	AssertTrue(cHCP65.MainMemoryAt(0x400000) == cV.GetMemoryChip(0));
	AssertTrue(cHCP65.MainMemoryAt(0x600000) == cV.GetMemoryChip(1));
	AssertTrue(cHCP65.MainMemoryAt(0x800000) == cX.GetMemoryChip(0));
	AssertTrue(cHCP65.MainMemoryAt(0xBFFFFF) == cX.GetMemoryChip(1) + 0x1FFFFF);
	AssertTrue(cHCP65.MainMemoryAt(0xC00000) == NULL);
	cHCP65.Write(0x612345, 0x5A);
	AssertInt(0x5A, cV.GetMemoryChip(1)[0x12345]);
	AssertInt(0x5A, cHCP65.Read(0x612345));
	cHCP65.Write(0xC00000, 0x5A);
	AssertInt(0xFF, cHCP65.Read(0xC00000));

	//The device enables don't clear the registers; a write 8 bytes on is register 0 again.
	cHCP65.SetDeviceEnables(0);
	AssertTrue(cHCP65.MainMemoryAt(0x400000) == cV.GetMemoryChip(0));
	cHCP65.Write(0x108008, 3 << 6);
	AssertTrue(cHCP65.MainMemoryAt(0xC00000) == cV.GetMemoryChip(0));
	AssertTrue(cHCP65.MainMemoryAt(0x400000) == NULL);

	//The reset keeps the windows and the memory.
	cHCP65.Reset();
	AssertTrue(cHCP65.MainMemoryAt(0xC00000) == cV.GetMemoryChip(0));
	AssertTrue(cHCP65.MainMemoryAt(0x800000) == cX.GetMemoryChip(0));
	AssertTrue(cHCP65.MainMemoryAt(0x400000) == NULL);
	AssertInt(0x5A, cV.GetMemoryChip(1)[0x12345]);

	//Both back in window 0.
	cHCP65.Write(0x108000, 0);
	cHCP65.Write(0x108001, 0);
	AssertTrue(cHCP65.MainMemoryAt(0x200000) == cV.GetMemoryChip(1));

	//Taken out, its memory is gone.
	AssertTrue(cHCP65.SetDevice(18, NULL));
	AssertTrue(cHCP65.MainMemoryAt(0x000000) == cX.GetMemoryChip(0));
	cHCP65.Kill();
	cX.Kill();
	cV.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A device ROM: the magic, the board's description, the register list in
// order whatever order it was given in, erased flash after it and bytes
// that sum to 0.  The sound board's, as the bus reads it.
//
//////////////////////////////////////////////////////////////////////////
void TestHCP65DeviceROM(void)
{
	CDeviceROM		cROM;
	CHCP65Audio		cAudio;
	uint8*			puiROM;
	uint8			uiSum;
	uint32			i;
	char			szName[DEVICE_ROM_NAME_LENGTH];

	cROM.Init("Test Board", "Tester", 3, DEVICE_BUS_128KB, DEVICE_MAIN_BUS_MASTER | DEVICE_MAIN_BUS_WINDOW_B, "TEST.DRV");
	AssertTrue(cROM.AddRegister(0x405, DEVICE_REGISTER_WRITE));
	AssertTrue(cROM.AddRegister(0x400, DEVICE_REGISTER_WRITE));
	AssertTrue(cROM.AddRegister(0x405, DEVICE_REGISTER_READ));
	AssertFalse(cROM.AddRegister(0x400, DEVICE_REGISTER_WRITE));
	AssertInt(3, cROM.NumRegisters());

	puiROM = cROM.GetROM();
	AssertInt(0x1A, puiROM[0]);
	AssertInt(0xFE, puiROM[1]);
	AssertInt(0xDA, puiROM[2]);
	AssertInt(0xC0, puiROM[3]);
	AssertInt(DEVICE_ROM_FORMAT, puiROM[DEVICE_ROM_FORMAT_OFFSET]);
	AssertInt(3, puiROM[DEVICE_ROM_REVISION_OFFSET]);
	AssertInt(DEVICE_BUS_128KB, puiROM[DEVICE_ROM_DEVICE_BUS_OFFSET]);
	AssertInt(DEVICE_MAIN_BUS_MASTER | DEVICE_MAIN_BUS_WINDOW_B, puiROM[DEVICE_ROM_MAIN_BUS_OFFSET]);
	AssertInt(0xFF, puiROM[0x008]);
	AssertString("Test Board", (char*)&puiROM[DEVICE_ROM_NAME_OFFSET]);
	AssertInt(0, puiROM[DEVICE_ROM_NAME_OFFSET + DEVICE_ROM_NAME_LENGTH - 1]);
	AssertString("Tester", (char*)&puiROM[DEVICE_ROM_MAKER_OFFSET]);
	AssertString("TEST.DRV", (char*)&puiROM[DEVICE_ROM_DRIVER_OFFSET]);
	AssertInt(3, puiROM[DEVICE_ROM_COUNT_OFFSET]);
	AssertInt(0, puiROM[DEVICE_ROM_COUNT_OFFSET + 1]);

	static const uint8 auiRegisters[] = { 0x00, 0x04, 1,  0x05, 0x04, 0,  0x05, 0x04, 1 };
	AssertTrue(memcmp(auiRegisters, &puiROM[DEVICE_ROM_REGISTERS_OFFSET], sizeof(auiRegisters)) == 0);
	AssertInt(0xFF, puiROM[DEVICE_ROM_REGISTERS_OFFSET + sizeof(auiRegisters)]);

	uiSum = 0;
	for (i = 0; i < DEVICE_ROM_SIZE; i++)
	{
		uiSum = (uint8)(uiSum + puiROM[i]);
	}
	AssertInt(0, uiSum);

	cAudio.Init();
	AssertInt(0x1A, cAudio.Read(0));
	for (i = 0; i < DEVICE_ROM_NAME_LENGTH; i++)
	{
		szName[i] = (char)cAudio.Read(DEVICE_ROM_NAME_OFFSET + i);
	}
	AssertString("Sound Board", szName);
	AssertInt(DEVICE_BUS_128KB, cAudio.Read(DEVICE_ROM_DEVICE_BUS_OFFSET));
	AssertInt(11, cAudio.Read(DEVICE_ROM_COUNT_OFFSET));
	uiSum = 0;
	for (i = 0; i < DEVICE_ROM_SIZE; i++)
	{
		uiSum = (uint8)(uiSum + cAudio.Read(i));
	}
	AssertInt(0, uiSum);
	cAudio.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Reads szFile's <DeviceROM> into pcROM.  pszError says why it couldn't.
//
//////////////////////////////////////////////////////////////////////////
static bool TestHCP65LoadDeviceROM(const char* szDirectory, const char* szFile, CDeviceROM* pcROM, CChars* pszError)
{
	CXMLFile	cFile;
	bool		bResult;

	cFile.Init();
	bResult = cFile.Read(szFile, szDirectory) && cFile.mcMarkup.mpcDoc && cFile.mcMarkup.mpcDoc->GetRootTag();
	if (bResult)
	{
		bResult = pcROM->LoadXML(cFile.mcMarkup.mpcDoc->GetRootTag(), pszError);
	}
	else
	{
		pszError->Set("Can't read the file");
	}
	cFile.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The device ROM tool's descriptions of the HCP65's boards (DeviceROM\
// Boards) make exactly the ROMs the emulator's boards have.  A wrong
// description says what is wrong with it.
//
//////////////////////////////////////////////////////////////////////////
void TestHCP65DeviceROMFromXML(void)
{
	CDeviceROM		cROM;
	CHCP65Audio		cAudio;
	CHCP65SDCard	cSDCard;
	CHCP65UARTBoard	cUART;
	CHCP65MemoryBoard	cMemory;
	CChars			szError;
	CChars			szMainBus;
	FILE*			pFile;
	uint32			i;
	bool			bSame;
	CFileUtil		cFileUtil;

	DataMemoryInit();
	szError.Init();

	cAudio.Init();
	AssertTrue(TestHCP65LoadDeviceROM(TEST_HCP65_BOARDS, "SoundBoard.xml", &cROM, &szError));
	bSame = true;
	for (i = 0; i < DEVICE_ROM_SIZE; i++)
	{
		bSame &= cROM.Read(i) == cAudio.Read(i);
	}
	AssertTrue(bSame);
	cAudio.Kill();

	cSDCard.Init();
	AssertTrue(TestHCP65LoadDeviceROM(TEST_HCP65_BOARDS, "SDCardBoard.xml", &cROM, &szError));
	bSame = true;
	for (i = 0; i < DEVICE_ROM_SIZE; i++)
	{
		bSame &= cROM.Read(i) == cSDCard.Read(i);
	}
	AssertTrue(bSame);
	cSDCard.Kill();

	cUART.Init();
	AssertTrue(TestHCP65LoadDeviceROM(TEST_HCP65_BOARDS, "UARTBoard.xml", &cROM, &szError));
	AssertString("", szError.Text());
	bSame = true;
	for (i = 0; i < DEVICE_ROM_SIZE; i++)
	{
		bSame &= cROM.Read(i) == cUART.Read(i);
	}
	AssertTrue(bSame);
	cUART.Kill();

	cMemory.Init();
	AssertTrue(TestHCP65LoadDeviceROM(TEST_HCP65_BOARDS, "MemoryBoard.xml", &cROM, &szError));
	AssertString("", szError.Text());
	bSame = true;
	for (i = 0; i < DEVICE_ROM_SIZE; i++)
	{
		bSame &= cROM.Read(i) == cMemory.Read(i);
	}
	AssertTrue(bSame);
	cMemory.Kill();

	cFileUtil.TouchDir("Output");  //False when Output is already there.
	AssertTrue(cFileUtil.IsDirectory("Output"));
	pFile = fopen("Output" _FS_ "BadDeviceROM.xml", "wb");
	AssertNotNull(pFile);
	fputs("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<DeviceROM Name=\"Board\" Manufacturer=\"Maker\" DeviceBus=\"32KB\">\n\t<Register Offset=\"0x400\" Access=\"Read\"/>\n\t<Register Offset=\"0x400\" Access=\"ReadWrite\"/>\n</DeviceROM>\n", pFile);
	fclose(pFile);
	AssertFalse(TestHCP65LoadDeviceROM("Output", "BadDeviceROM.xml", &cROM, &szError));
	AssertString("Register listed twice (<Register> line 4)", szError.Text());

	//A private board: its windows are its own memory.  MainBus's words are written in one order.
	pFile = fopen("Output" _FS_ "PrivateDeviceROM.xml", "wb");
	AssertNotNull(pFile);
	fputs("<?xml version=\"1.0\"?>\n<DeviceROM Name=\"Board\" Manufacturer=\"Maker\" DeviceBus=\"32KB\" MainBus=\"Private  WindowB WindowA\"/>\n", pFile);
	fclose(pFile);
	szError.Clear();
	AssertTrue(TestHCP65LoadDeviceROM("Output", "PrivateDeviceROM.xml", &cROM, &szError));
	AssertString("", szError.Text());
	AssertInt(DEVICE_MAIN_BUS_WINDOW_A | DEVICE_MAIN_BUS_WINDOW_B | DEVICE_MAIN_BUS_PRIVATE, cROM.Read(DEVICE_ROM_MAIN_BUS_OFFSET));
	szMainBus.Init();
	DeviceROMMainBusText(DEVICE_MAIN_BUS_MASTER | DEVICE_MAIN_BUS_WINDOW_A | DEVICE_MAIN_BUS_WINDOW_B | DEVICE_MAIN_BUS_PRIVATE, &szMainBus);
	AssertString("Master WindowA WindowB Private", szMainBus.Text());
	DeviceROMMainBusText(0, &szMainBus);
	AssertString("", szMainBus.Text());
	szMainBus.Kill();

	//A word that isn't a flag's is named.
	pFile = fopen("Output" _FS_ "BadDeviceROM.xml", "wb");
	AssertNotNull(pFile);
	fputs("<?xml version=\"1.0\"?>\n<DeviceROM Name=\"Board\" Manufacturer=\"Maker\" DeviceBus=\"32KB\" MainBus=\"WindowA Privat\"/>\n", pFile);
	fclose(pFile);
	AssertFalse(TestHCP65LoadDeviceROM("Output", "BadDeviceROM.xml", &cROM, &szError));
	AssertString("Unknown MainBus: Privat (<DeviceROM> line 2)", szError.Text());

	szError.Kill();
	DataMemoryKill();
}


//////////////////////////////////////////////////////////////////////////
//
// The ROM made from szSchematic's description, through the XML file the
// device ROM tool writes, is pcDevice's ROM.
//
//////////////////////////////////////////////////////////////////////////
static void TestHCP65DeviceROMSchematicMatches(const char* szSchematic, CHCP65Device* pcDevice, CDeviceROMSchematic* pcSchematic)
{
	CDeviceROM		cROM;
	CChars			szError;
	uint32			i;
	bool			bSame;

	szError.Init();
	AssertTrue(pcSchematic->Read(szSchematic, &szError));
	AssertString("", szError.Text());
	AssertTrue(pcSchematic->WriteXML("Output" _FS_ "Schematic.xml"));
	AssertTrue(TestHCP65LoadDeviceROM("Output", "Schematic.xml", &cROM, &szError));
	bSame = true;
	for (i = 0; i < DEVICE_ROM_SIZE; i++)
	{
		bSame &= cROM.Read(i) == pcDevice->Read(i);
	}
	AssertTrue(bSame);
	szError.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The Sound Board's and SD Card Board's KiCad schematics describe exactly
// the ROMs the emulator's boards have: the registers from their Device
// Decode Stage Bs, named by their strobes' nets, the buses from what A16,
// ~{Main Access} and ~{Memory A} and B reach, the name from the title block
// and the maker and driver from their device ROM chips' properties.
//
//////////////////////////////////////////////////////////////////////////
void TestHCP65DeviceROMFromSchematic(void)
{
	CDeviceROMSchematic	cSchematic;
	CHCP65Audio			cAudio;
	CHCP65SDCard		cSDCard;
	CChars				szError;

	DataMemoryInit();

	cAudio.Init();
	cSchematic.Init();
	TestHCP65DeviceROMSchematicMatches(TEST_HCP65_KICAD _FS_ "Audio" _FS_ "Audio Board" _FS_ "Audio Board.kicad_sch", &cAudio, &cSchematic);
	AssertString("Sound Board", cSchematic.GetName());
	AssertInt(DEVICE_BUS_128KB, cSchematic.GetDeviceBus());
	AssertInt(0, cSchematic.GetMainBus());
	AssertSize(11, cSchematic.NumRegisters());
	AssertString("Flags", cSchematic.GetRegister(0)->szName);
	AssertString("Counter Address Low", cSchematic.GetRegister(3)->szName);
	AssertInt(DEVICE_REGISTER_READ, cSchematic.GetRegister(3)->uiDirection);
	AssertString("Start Address Low", cSchematic.GetRegister(4)->szName);
	cSchematic.Kill();
	cAudio.Kill();

	cSDCard.Init();
	cSchematic.Init();
	TestHCP65DeviceROMSchematicMatches(TEST_HCP65_KICAD _FS_ "SD Card" _FS_ "SD Card Board" _FS_ "SD Card Board.kicad_sch", &cSDCard, &cSchematic);
	AssertString("SD Card Board", cSchematic.GetName());
	AssertInt(DEVICE_BUS_32KB, cSchematic.GetDeviceBus());
	AssertInt(DEVICE_MAIN_BUS_MASTER, cSchematic.GetMainBus());
	AssertSize(15, cSchematic.NumRegisters());
	cSchematic.Kill();
	cSDCard.Kill();

	//A memory board has no Device Decode Stage A to read the rest by.
	szError.Init();
	cSchematic.Init();
	AssertFalse(cSchematic.Read(TEST_HCP65_KICAD _FS_ "Memory" _FS_ "Memory Board" _FS_ "Memory Board.kicad_sch", &szError));
	AssertString("The board must have one Device_Decode_Stage_A", szError.Text());
	cSchematic.Kill();
	szError.Kill();

	//The PS2 Board: its name from its device ROM chip's ROM_Name (the title block's is "PS2 Board PS2"), and its
	//registers' names without KiCad's markup (the nets are "PS2 FIFO_{ 1}" and "Clear Interrupt _{1}").
	cSchematic.Init();
	szError.Init();
	AssertTrue(cSchematic.Read(TEST_HCP65_KICAD _FS_ "PS2" _FS_ "PS2 Board" _FS_ "PS2 Board.kicad_sch", &szError));
	AssertString("", szError.Text());
	AssertString("PS2 Board", cSchematic.GetName());
	AssertString("PS2.DRV", cSchematic.GetDriver());
	AssertSize(9, cSchematic.NumRegisters());
	AssertString("PS2 FIFO 1", cSchematic.GetRegister(2)->szName);
	AssertString("Clear Interrupt 1", cSchematic.GetRegister(3)->szName);
	cSchematic.Kill();
	szError.Kill();

	//The Coprocessor Board: the coprocessors' memory is windows A and B, swapped onto the main bus to load and collect
	//it, so its device ROM chip's ROM_MainBus says it is private.
	cSchematic.Init();
	szError.Init();
	AssertTrue(cSchematic.Read(TEST_HCP65_KICAD _FS_ "Coprocessor" _FS_ "Coprocessor Board - Main" _FS_ "Coprocessor Board - Main.kicad_sch", &szError));
	AssertString("", szError.Text());
	AssertString("Coprocessor Board", cSchematic.GetName());
	AssertString("COPROC.DRV", cSchematic.GetDriver());
	AssertInt(DEVICE_MAIN_BUS_WINDOW_A | DEVICE_MAIN_BUS_WINDOW_B | DEVICE_MAIN_BUS_PRIVATE, cSchematic.GetMainBus());
	cSchematic.Kill();
	szError.Kill();

	//The Video Board: its video memory is windows A and B too, but on the Video Board - Memory board, past a connector the
	//wiring isn't followed through, so ROM_MainBus gives all three words.
	cSchematic.Init();
	szError.Init();
	AssertTrue(cSchematic.Read(TEST_HCP65_KICAD _FS_ "Video" _FS_ "Video Board - Signals" _FS_ "Video Board - Signals.kicad_sch", &szError));
	AssertString("", szError.Text());
	AssertString("Video Board", cSchematic.GetName());
	AssertInt(DEVICE_BUS_128KB, cSchematic.GetDeviceBus());
	AssertInt(DEVICE_MAIN_BUS_WINDOW_A | DEVICE_MAIN_BUS_WINDOW_B | DEVICE_MAIN_BUS_PRIVATE, cSchematic.GetMainBus());
	AssertSize(27, cSchematic.NumRegisters());
	cSchematic.Kill();
	szError.Kill();

	DataMemoryKill();
}


//////////////////////////////////////////////////////////////////////////
//
// One emulated TL16C550D on its own, in its oscillator's ticks: the
// divisor, a character's time, the interrupts and their priorities, the
// FIFO trigger and timeout, auto-RTS and auto-CTS, loopback, an overrun,
// the modem status deltas and 7 bit characters.
//
//////////////////////////////////////////////////////////////////////////
void TestHCP65UART(void)
{
	CHCP65UART	cUART;
	uint8		auiData[16];
	uint64		uiTick;
	uint64		uiChar;		//9600 8N1: 16 ticks a bit, divisor 12, 10 bits.
	uint64		uiHoldOffs;
	int			i;

	uiChar = 16 * 12 * 10;
	cUART.Init(HUM_RS232);

	//Master reset: nothing pending, the transmitter empty, RI tied on.
	AssertInt(HCP65_UART_IIR_NONE, cUART.Read(HCP65_UART_IIR));
	AssertInt(0x60, cUART.Read(HCP65_UART_LSR));
	AssertInt(HCP65_UART_MSR_RI, cUART.Read(HCP65_UART_MSR));

	//The divisor latch behind DLAB.
	cUART.Write(HCP65_UART_LCR, 0x83);
	cUART.Write(HCP65_UART_RBR, 12);
	cUART.Write(HCP65_UART_IER, 0);
	AssertInt(12, cUART.Read(HCP65_UART_RBR));
	cUART.Write(HCP65_UART_LCR, 0x03);
	AssertInt(9600, cUART.GetBaudRate());
	cUART.Write(HCP65_UART_IIR, HCP65_UART_FCR_ENABLE | HCP65_UART_FCR_RX_RESET | HCP65_UART_FCR_TX_RESET);
	AssertInt(HCP65_UART_IIR_NONE | HCP65_UART_IIR_FIFOS, cUART.Read(HCP65_UART_IIR));

	//A byte goes straight to the shift register (THR empty again) and reaches the far end a character later.
	uiTick = 0;
	cUART.Write(HCP65_UART_RBR, 'A');
	AssertInt(HCP65_UART_LSR_THR_EMPTY, cUART.Read(HCP65_UART_LSR));
	cUART.CatchUp(uiTick + uiChar - 1);
	AssertSize(0, cUART.NumHostReceived());
	uiTick += uiChar;
	cUART.CatchUp(uiTick);
	AssertSize(1, cUART.HostReceive(auiData, 16));
	AssertInt('A', auiData[0]);
	AssertInt(0x60, cUART.Read(HCP65_UART_LSR));

	//Turning the transmit interrupt on with THR empty interrupts; reading IIR clears it.
	cUART.Write(HCP65_UART_IER, HCP65_UART_IER_TRANSMIT);
	AssertTrue(cUART.IsInterrupting());
	AssertInt(HCP65_UART_IIR_TRANSMIT | HCP65_UART_IIR_FIFOS, cUART.Read(HCP65_UART_IIR));
	AssertFalse(cUART.IsInterrupting());
	cUART.Write(HCP65_UART_IER, 0);

	//The far end only sends while RTS is on, one character at a time.  Three bytes are under the trigger level (4), so
	//they interrupt by the timeout, four characters after the last.
	cUART.Write(HCP65_UART_IIR, HCP65_UART_FCR_ENABLE | 0x40);
	cUART.Write(HCP65_UART_IER, HCP65_UART_IER_RECEIVED);
	cUART.HostSend((uint8*)"xyz", 3);
	uiTick += 10 * uiChar;
	cUART.CatchUp(uiTick);
	AssertSize(3, cUART.NumHostToSend());
	cUART.Write(HCP65_UART_MCR, HCP65_UART_MCR_RTS);
	AssertTrue(cUART.IsRTSOn());
	cUART.CatchUp(uiTick + uiChar - 1);
	AssertInt(0, cUART.Read(HCP65_UART_LSR) & HCP65_UART_LSR_DATA_READY);
	cUART.CatchUp(uiTick + 3 * uiChar);
	AssertInt(HCP65_UART_LSR_DATA_READY, cUART.Read(HCP65_UART_LSR) & HCP65_UART_LSR_DATA_READY);
	AssertSize(0, cUART.NumHostToSend());
	AssertFalse(cUART.IsInterrupting());
	AssertInt((int)(uiTick + 7 * uiChar), (int)cUART.NextEvent());
	uiTick += 7 * uiChar;
	cUART.CatchUp(uiTick);
	AssertInt(HCP65_UART_IIR_TIMEOUT | HCP65_UART_IIR_FIFOS, cUART.Read(HCP65_UART_IIR));
	AssertInt('x', cUART.Read(HCP65_UART_RBR));
	AssertInt('y', cUART.Read(HCP65_UART_RBR));
	AssertInt('z', cUART.Read(HCP65_UART_RBR));
	AssertInt(HCP65_UART_IIR_NONE | HCP65_UART_IIR_FIFOS, cUART.Read(HCP65_UART_IIR));

	//Auto-RTS: at the trigger level (4) RTS goes off and the far end waits; reading a byte lets it go on.  At the trigger
	//level the received interrupt is pending.  The far end is held off three times.
	uiHoldOffs = cUART.NumHoldOffs();
	cUART.Write(HCP65_UART_MCR, HCP65_UART_MCR_RTS | HCP65_UART_MCR_AUTO_FLOW);
	for (i = 0; i < 10; i++)
	{
		auiData[i] = (uint8)('0' + i);
	}
	cUART.HostSend(auiData, 10);
	uiTick += 20 * uiChar;
	cUART.CatchUp(uiTick);
	AssertFalse(cUART.IsRTSOn());
	AssertSize(6, cUART.NumHostToSend());
	AssertInt(HCP65_UART_IIR_RECEIVED | HCP65_UART_IIR_FIFOS, cUART.Read(HCP65_UART_IIR));
	AssertInt('0', cUART.Read(HCP65_UART_RBR));
	AssertTrue(cUART.IsRTSOn());
	uiTick += uiChar;
	cUART.CatchUp(uiTick);
	AssertSize(5, cUART.NumHostToSend());
	for (i = 1; i <= 4; i++)
	{
		AssertInt('0' + i, cUART.Read(HCP65_UART_RBR));
	}
	uiTick += 20 * uiChar;
	cUART.CatchUp(uiTick);
	for (i = 5; i <= 8; i++)
	{
		AssertInt('0' + i, cUART.Read(HCP65_UART_RBR));
	}
	uiTick += 20 * uiChar;
	cUART.CatchUp(uiTick);
	AssertInt('9', cUART.Read(HCP65_UART_RBR));
	AssertSize(0, cUART.NumHostToSend());
	AssertInt(0, cUART.Read(HCP65_UART_LSR) & HCP65_UART_LSR_DATA_READY);
	AssertLong(0, (int64)cUART.NumOverruns());
	AssertLong(3, (int64)(cUART.NumHoldOffs() - uiHoldOffs));

	//Auto-CTS: with the far end's RTS off nothing goes; on, it does.
	cUART.Write(HCP65_UART_IER, 0);
	cUART.Write(HCP65_UART_RBR, 'C');
	uiTick += 10 * uiChar;
	cUART.CatchUp(uiTick);
	AssertSize(0, cUART.NumHostReceived());
	AssertInt(0, cUART.Read(HCP65_UART_LSR) & HCP65_UART_LSR_THR_EMPTY);
	cUART.HostSetCTS(true);
	AssertInt(0x01 | HCP65_UART_MSR_CTS | HCP65_UART_MSR_RI, cUART.Read(HCP65_UART_MSR));
	AssertInt(HCP65_UART_MSR_CTS | HCP65_UART_MSR_RI, cUART.Read(HCP65_UART_MSR));
	uiTick += uiChar;
	cUART.CatchUp(uiTick);
	AssertSize(1, cUART.HostReceive(auiData, 16));
	AssertInt('C', auiData[0]);

	//A modem status change interrupts while it is enabled, until MSR is read.
	cUART.Write(HCP65_UART_IER, HCP65_UART_IER_MODEM_STATUS);
	cUART.HostSetDSR(true);
	AssertInt(HCP65_UART_IIR_MODEM_STATUS | HCP65_UART_IIR_FIFOS, cUART.Read(HCP65_UART_IIR));
	AssertInt(0x02 | HCP65_UART_MSR_CTS | HCP65_UART_MSR_DSR | HCP65_UART_MSR_RI, cUART.Read(HCP65_UART_MSR));
	AssertInt(HCP65_UART_IIR_NONE | HCP65_UART_IIR_FIFOS, cUART.Read(HCP65_UART_IIR));
	cUART.Write(HCP65_UART_IER, 0);

	//Loopback: what it sends comes back and the far end gets nothing; its inputs are its outputs, and RTS and DTR are
	//off to the far end.
	cUART.Write(HCP65_UART_MCR, HCP65_UART_MCR_LOOP | HCP65_UART_MCR_RTS);
	cUART.Read(HCP65_UART_MSR);
	AssertInt(HCP65_UART_MSR_CTS, cUART.Read(HCP65_UART_MSR));
	AssertFalse(cUART.IsRTSOn());
	cUART.Write(HCP65_UART_RBR, 'L');
	uiTick += uiChar;
	cUART.CatchUp(uiTick);
	AssertSize(0, cUART.NumHostReceived());
	AssertInt('L', cUART.Read(HCP65_UART_RBR));

	//With the FIFOs off there is one holding register: the far end's next two bytes overrun it, which the line status
	//interrupt says until LSR is read.
	cUART.Write(HCP65_UART_MCR, HCP65_UART_MCR_RTS);
	cUART.Write(HCP65_UART_IIR, 0);
	cUART.Write(HCP65_UART_IER, HCP65_UART_IER_LINE_STATUS);
	cUART.HostSend((uint8*)"abc", 3);
	uiTick += 3 * uiChar;
	cUART.CatchUp(uiTick);
	AssertLong(2, (int64)cUART.NumOverruns());
	AssertInt(HCP65_UART_IIR_LINE_STATUS, cUART.Read(HCP65_UART_IIR));
	AssertInt(HCP65_UART_LSR_DATA_READY | HCP65_UART_LSR_OVERRUN | 0x60, cUART.Read(HCP65_UART_LSR));
	AssertInt(HCP65_UART_IIR_NONE, cUART.Read(HCP65_UART_IIR));
	AssertInt('a', cUART.Read(HCP65_UART_RBR));
	cUART.Write(HCP65_UART_IER, 0);

	//7 bit characters lose bit 7 on the line.
	cUART.Write(HCP65_UART_LCR, 0x02);
	auiData[0] = 0xC1;
	cUART.HostSend(auiData, 1);
	uiTick += uiChar;
	cUART.CatchUp(uiTick);
	AssertInt(0x41, cUART.Read(HCP65_UART_RBR));

	cUART.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The UART Board decodes A3 to pick its UART and repeats every 16 bytes;
// below $400 is its device ROM.  The TTL module has no DSR or DCD.
// Writes are ignored while it is held in reset.
//
//////////////////////////////////////////////////////////////////////////
void TestHCP65UARTBoard(void)
{
	CHCP65UARTBoard	cBoard;

	cBoard.Init();
	AssertInt(0x1A, cBoard.Read(0));		//The device ROM's magic, $C0DAFE1A.
	AssertInt(0xFF, cBoard.Read(0x800));

	cBoard.Write(0x400 + HCP65_UART_SCR, 0x55);
	AssertInt(0, cBoard.Read(0x400 + HCP65_UART_SCR));
	cBoard.SetHeldInReset(false);
	cBoard.Write(0x400 + HCP65_UART_SCR, 0x55);
	cBoard.Write(0x408 + HCP65_UART_SCR, 0xAA);
	AssertInt(0x55, cBoard.Read(0x400 + HCP65_UART_SCR));
	AssertInt(0xAA, cBoard.Read(0x408 + HCP65_UART_SCR));
	AssertInt(0x55, cBoard.Read(0x7F0 + HCP65_UART_SCR));
	AssertInt(0xAA, cBoard.Read(0x418 + HCP65_UART_SCR));

	cBoard.GetTTL()->HostSetDSR(true);
	cBoard.GetTTL()->HostSetDCD(true);
	cBoard.GetRS232()->HostSetDSR(true);
	AssertInt(HCP65_UART_MSR_RI, cBoard.Read(0x408 + HCP65_UART_MSR));
	AssertInt(0x02 | HCP65_UART_MSR_DSR | HCP65_UART_MSR_RI, cBoard.Read(0x400 + HCP65_UART_MSR));
	AssertFalse(cBoard.IsInterrupting());

	cBoard.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A device clock's ticks and CPU cycles at 25MHz, the UART's 1.8432MHz:
// a tick takes 13.56 cycles, so the first has happened by cycle 14.  A
// thousand hours' cycles times the rate would overflow 64 bits.
//
//////////////////////////////////////////////////////////////////////////
void TestHCP65DeviceTick(void)
{
	CHCP65	cHCP65;
	uint64	uiHours;

	cHCP65.Init(25000000);
	AssertLong(0, (int64)cHCP65.DeviceTick(13, 1843200));
	AssertLong(1, (int64)cHCP65.DeviceTick(14, 1843200));
	AssertLong(14, (int64)cHCP65.CycleOfDeviceTick(1, 1843200));
	AssertLong(1843200, (int64)cHCP65.DeviceTick(25000000, 1843200));
	AssertLong(25000000, (int64)cHCP65.CycleOfDeviceTick(1843200, 1843200));

	uiHours = 1000ULL * 3600;
	AssertLong((int64)(uiHours * 1843200), (int64)cHCP65.DeviceTick(uiHours * 25000000, 1843200));
	AssertLong((int64)(uiHours * 25000000), (int64)cHCP65.CycleOfDeviceTick(uiHours * 1843200, 1843200));
	AssertLong((int64)(uiHours * 25000000 + 14), (int64)cHCP65.CycleOfDeviceTick(uiHours * 1843200 + 1, 1843200));
	cHCP65.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void TestHCP65Machine(void)
{
	BeginTests();

	TestHCP65DeviceROM();
	TestHCP65DeviceROMFromXML();
	TestHCP65DeviceROMFromSchematic();
	TestHCP65BootAndUserMode();
	TestHCP65AudioBoard();
	TestHCP65AudioFromProgram();
	TestHCP65DeviceTick();
	TestHCP65UART();
	TestHCP65UARTBoard();
	TestHCP65Timer();
	TestHCP65Contention();
	TestHCP65MemoryFill();
	TestHCP65MemoryBoard();

	TestStatistics();
}
