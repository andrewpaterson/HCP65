#include <stdio.h>
#include <string.h>
#include "BaseLib/Chars.h"
#include "BaseLib/DataMemory.h"
#include "BaseLib/FileUtil.h"
#include "BaseLib/TextFile.h"
#include "BaseLib/XMLFile.h"
#include "DeviceRomLib/DeviceROMSchematic.h"
#include "HCP65Lib/HCP65.h"
#include "HCP65Lib/HCP65Audio.h"
#include "HCP65Lib/HCP65Mainboard.h"
#include "HCP65Lib/HCP65SDCard.h"
#include "HCP65Lib/HCP65UART.h"
#include "HCP65Lib/HCP65VIA.h"
#include "HCP65Lib/HCP65PS2.h"
#include "HCP65Lib/HCP65PS2Keyboard.h"
#include "HCP65Lib/HCP65Terminal.h"
#include "HCP65Lib/HCP65Video.h"
#include "HCP65Lib/HCP65Blitter.h"
#include "HCP65Lib/HCP65Graphics.h"
#include "HCP65Lib/HCP65LCD.h"
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
	CHCP65VIABoard	cVIA;
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

	//The VIA Board's eight VIAs' 16 registers each are read and written, and the interrupts are only read.
	cVIA.Init();
	AssertTrue(TestHCP65LoadDeviceROM(TEST_HCP65_BOARDS, "VIABoard.xml", &cROM, &szError));
	AssertString("", szError.Text());
	AssertInt(8 * 16 * 2 + 1, cROM.NumRegisters());
	AssertString("VIA Board", (char*)&cROM.GetROM()[DEVICE_ROM_NAME_OFFSET]);
	AssertString("VIA.DRV", (char*)&cROM.GetROM()[DEVICE_ROM_DRIVER_OFFSET]);
	bSame = true;
	for (i = 0; i < DEVICE_ROM_SIZE; i++)
	{
		bSame &= cROM.Read(i) == cVIA.Read(i);
	}
	AssertTrue(bSame);
	cVIA.Kill();

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
	CHCP65PS2Board		cPS2;
	CHCP65VideoBoard	cVideo;
	CHCP65BlitterBoard	cBlitter;
	CHCP65GraphicsBoard	cGraphics;
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

	//The Video Board's emulated ROM is the schematic's.
	cVideo.Init();
	cSchematic.Init();
	TestHCP65DeviceROMSchematicMatches(TEST_HCP65_KICAD _FS_ "Video" _FS_ "Video Board - Signals" _FS_ "Video Board - Signals.kicad_sch", &cVideo, &cSchematic);
	cSchematic.Kill();
	cVideo.Kill();

	//The PS2 Board's emulated ROM is the schematic's.
	cPS2.Init();
	cSchematic.Init();
	TestHCP65DeviceROMSchematicMatches(TEST_HCP65_KICAD _FS_ "PS2" _FS_ "PS2 Board" _FS_ "PS2 Board.kicad_sch", &cPS2, &cSchematic);
	cSchematic.Kill();
	cPS2.Kill();

	//The Blitter Board's emulated ROM is the schematic's.
	cBlitter.Init();
	cSchematic.Init();
	TestHCP65DeviceROMSchematicMatches(TEST_HCP65_KICAD _FS_ "Blitter" _FS_ "Blitter Board" _FS_ "Blitter Board.kicad_sch", &cBlitter, &cSchematic);
	AssertString("Blitter Board", cSchematic.GetName());
	AssertString("BLITTER.DRV", cSchematic.GetDriver());
	AssertInt(DEVICE_BUS_32KB, cSchematic.GetDeviceBus());
	AssertInt(DEVICE_MAIN_BUS_MASTER, cSchematic.GetMainBus());
	AssertSize(18, cSchematic.NumRegisters());
	cSchematic.Kill();
	cBlitter.Kill();

	//The Graphics Board's emulated ROM is the schematic's.
	cGraphics.Init();
	cSchematic.Init();
	TestHCP65DeviceROMSchematicMatches(TEST_HCP65_KICAD _FS_ "Graphics" _FS_ "Graphics Board" _FS_ "Graphics Board.kicad_sch", &cGraphics, &cSchematic);
	AssertString("Graphics Board", cSchematic.GetName());
	AssertString("GRAPHICS.DRV", cSchematic.GetDriver());
	AssertInt(DEVICE_BUS_128KB, cSchematic.GetDeviceBus());
	AssertInt(DEVICE_MAIN_BUS_MASTER, cSchematic.GetMainBus());
	AssertSize(21, cSchematic.NumRegisters());
	cSchematic.Kill();
	cGraphics.Kill();

	//The LCD and SNES Board: its name from its device ROM chip's ROM_Name (the title block's is "HCP65 LCD and SNES
	//Board").  Its registers are decoded by the SNES Controller Clock, not a Device Decode Stage B, so the schematic
	//shows none of them.
	cSchematic.Init();
	szError.Init();
	AssertTrue(cSchematic.Read(TEST_HCP65_KICAD _FS_ "LCD" _FS_ "LCD Board" _FS_ "LCD Board.kicad_sch", &szError));
	AssertString("", szError.Text());
	AssertString(HCP65_LCD_BOARD_NAME, cSchematic.GetName());
	AssertString(HCP65_LCD_BOARD_DRIVER, cSchematic.GetDriver());
	AssertInt(DEVICE_BUS_32KB, cSchematic.GetDeviceBus());
	AssertInt(0, cSchematic.GetMainBus());
	AssertSize(0, cSchematic.NumRegisters());
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
// szFilename is written with uiSize bytes of nothing, or szText if it's
// given.
//
//////////////////////////////////////////////////////////////////////////
static void TestHCP65WriteFile(const char* szFilename, const char* szText, size uiSize)
{
	FILE*	pFile;
	size	i;

	pFile = fopen(szFilename, "wb");
	AssertNotNull(pFile);
	if (szText)
	{
		fputs(szText, pFile);
	}
	for (i = 0; i < uiSize; i++)
	{
		fputc(0, pFile);
	}
	fclose(pFile);
}


//////////////////////////////////////////////////////////////////////////
//
// Changing the boards in the sockets, and writing what's in them.
//
//////////////////////////////////////////////////////////////////////////
void TestHCP65MainboardChangeBoards(void)
{
	CHCP65				cHCP65;
	CHCP65Mainboard		cMainboard;
	CHCP65Mainboard		cWritten;
	CChars				szError;
	CFileUtil			cFileUtil;
	const char*			aszTypes[HCP65_NUM_DEVICE_SLOTS];
	CHCP65Device*		pcSDCard;
	CHCP65Device*		pcUART;
	CHCP65Device*		pcAudio;
	CHCP65Device*		pcMemory;
	CHCP65Device*		pcNewAudio;
	CTextFile			cText;

	DataMemoryInit();
	szError.Init();
	cFileUtil.TouchDir("Output");
	TestHCP65WriteFile("Output" _FS_ "MainboardCard.img", NULL, 1024);
	TestHCP65WriteFile("Output" _FS_ "Mainboard.xml",
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		"<!-- The test's Small Mainboard. -->\n"
		"<!-- Files are relative to this one. -->\n"
		"<Mainboard Type=\"Small\"><Processor Index=\"0\">"
		"<Board Socket=\"3\" Type=\"SD Card Board\"><Card Socket=\"1\" File=\"MainboardCard.img\"/></Board>"
		"<Board Socket=\"4\" Type=\"UART Board\"/>"
		"<Board Socket=\"16\" Type=\"Sound Board\"/>"
		"<Board Socket=\"18\" Type=\"Memory Board\"/>"
		"</Processor></Mainboard>", 0);

	//--card's image is in card socket 0: it isn't in the description, so it won't be written.
	cHCP65.Init(TEST_HCP65_CLOCK_RATE);
	cMainboard.Init();
	cMainboard.SetCard("Output" _FS_ "MainboardCard.img");
	AssertTrue(cMainboard.Read("Output" _FS_ "Mainboard.xml", &szError));
	AssertString("", szError.Text());
	AssertTrue(cMainboard.PlugIn(&cHCP65));
	pcSDCard = cMainboard.GetBoard(3);
	pcUART = cMainboard.GetBoard(4);
	pcAudio = cMainboard.GetBoard(16);
	pcMemory = cMainboard.GetBoard(18);
	AssertInt(2, ((CHCP65SDCard*)pcSDCard)->NumSectors(0));
	AssertInt(2, ((CHCP65SDCard*)pcSDCard)->NumSectors(1));

	//What fits where.  Sockets 3 and 4 are 32KB, 5, 6 and 7 aren't on the Small Mainboard.
	AssertSize(10, CHCP65Mainboard::NumBoardTypes());
	AssertFalse(cMainboard.CanHold(3, HCP65_GRAPHICS_NAME));
	AssertTrue(cMainboard.CanHold(3, HCP65_LCD_BOARD_NAME));
	AssertTrue(cMainboard.CanHold(16, HCP65_LCD_BOARD_NAME));
	AssertTrue(cMainboard.CanHold(16, HCP65_GRAPHICS_NAME));
	AssertTrue(cMainboard.CanHold(3, HCP65_SD_NAME));
	AssertTrue(cMainboard.CanHold(3, HCP65_MEMORY_BOARD_NAME));
	AssertFalse(cMainboard.CanHold(3, HCP65_AUDIO_NAME));
	AssertTrue(cMainboard.CanHold(16, HCP65_AUDIO_NAME));
	AssertFalse(cMainboard.CanHold(5, HCP65_SD_NAME));
	AssertFalse(cMainboard.CanHold(1, HCP65_SD_NAME));
	AssertFalse(cMainboard.CanHold(16, "No Such Board"));

	//One that won't fit stops all of them, before anything changes.
	memset(aszTypes, 0, sizeof(aszTypes));
	aszTypes[3] = HCP65_AUDIO_NAME;
	AssertFalse(cMainboard.ChangeBoards(&cHCP65, aszTypes, &szError));
	AssertFalse(szError.Empty());
	AssertTrue(pcSDCard == cMainboard.GetBoard(3));
	AssertTrue(pcUART == cMainboard.GetBoard(4));
	AssertTrue(pcAudio == cMainboard.GetBoard(16));
	AssertTrue(pcMemory == cHCP65.GetDevice(18));

	//The same boards again: nothing moves.
	memset(aszTypes, 0, sizeof(aszTypes));
	aszTypes[3] = HCP65_SD_NAME;
	aszTypes[4] = HCP65_UART_NAME;
	aszTypes[16] = HCP65_AUDIO_NAME;
	aszTypes[18] = HCP65_MEMORY_BOARD_NAME;
	AssertTrue(cMainboard.ChangeBoards(&cHCP65, aszTypes, &szError));
	AssertTrue(pcSDCard == cMainboard.GetBoard(3));
	AssertTrue(pcMemory == cMainboard.GetBoard(18));

	//Two swap and one moves: they are the same boards, and the SD Card Board has its cards.
	aszTypes[3] = HCP65_UART_NAME;
	aszTypes[4] = HCP65_SD_NAME;
	aszTypes[16] = NULL;
	aszTypes[17] = HCP65_AUDIO_NAME;
	AssertTrue(cMainboard.ChangeBoards(&cHCP65, aszTypes, &szError));
	AssertTrue(pcUART == cMainboard.GetBoard(3));
	AssertTrue(pcUART == cHCP65.GetDevice(3));
	AssertTrue(pcSDCard == cMainboard.GetBoard(4));
	AssertTrue(pcSDCard == cHCP65.GetDevice(4));
	AssertNull(cMainboard.GetBoard(16));
	AssertNull(cHCP65.GetDevice(16));
	AssertTrue(cMainboard.GetBoardType(16) == NULL);
	AssertTrue(pcAudio == cMainboard.GetBoard(17));
	AssertTrue(pcAudio == cHCP65.GetDevice(17));
	AssertTrue(pcAudio == cMainboard.FindBoard(HCP65_AUDIO_NAME));
	AssertTrue(pcMemory == cMainboard.GetBoard(18));
	AssertInt(2, ((CHCP65SDCard*)pcSDCard)->NumSectors(1));

	//The Memory Board taken out leaves no main memory, and comes back with what was on it.
	AssertNotNull(cHCP65.MainMemoryAt(0x123456));
	*cHCP65.MainMemoryAt(0x123456) = 0x5A;
	aszTypes[18] = NULL;
	AssertTrue(cMainboard.ChangeBoards(&cHCP65, aszTypes, &szError));
	AssertNull(cHCP65.MainMemoryAt(0x123456));
	cHCP65.Reset();
	aszTypes[18] = HCP65_MEMORY_BOARD_NAME;
	AssertTrue(cMainboard.ChangeBoards(&cHCP65, aszTypes, &szError));
	cHCP65.Reset();
	AssertTrue(pcMemory == cMainboard.GetBoard(18));
	AssertNotNull(cHCP65.MainMemoryAt(0x123456));
	AssertInt(0x5A, *cHCP65.MainMemoryAt(0x123456));

	//With none out, a new board is made; the one that was out goes in first.
	aszTypes[17] = NULL;
	AssertTrue(cMainboard.ChangeBoards(&cHCP65, aszTypes, &szError));
	aszTypes[17] = HCP65_AUDIO_NAME;
	aszTypes[19] = HCP65_AUDIO_NAME;
	AssertTrue(cMainboard.ChangeBoards(&cHCP65, aszTypes, &szError));
	AssertTrue(pcAudio == cMainboard.GetBoard(17));
	pcNewAudio = cMainboard.GetBoard(19);
	AssertNotNull(pcNewAudio);
	AssertTrue(pcNewAudio != pcAudio);
	AssertTrue(pcNewAudio == cHCP65.GetDevice(19));
	AssertString(HCP65_AUDIO_NAME, cMainboard.GetBoardType(19));

	//Written, the description reads back to the same boards with the card from its own <Card> and not --card's.  It
	//keeps the comments that were before <Mainboard>, and is indented with tabs, as descriptions are written by hand.
	szError.Set("");
	AssertTrue(cMainboard.Write("Output" _FS_ "MainboardWritten.xml", &szError));
	AssertString("", szError.Text());
	cText.Init();
	AssertTrue(cText.Read("Output" _FS_ "MainboardWritten.xml"));
	AssertString("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		"<!-- The test's Small Mainboard. -->\n"
		"<!-- Files are relative to this one. -->\n"
		"<Mainboard Type=\"Small\">\n"
		"\t<Processor Index=\"0\">\n"
		"\t\t<Board Socket=\"3\" Type=\"UART Board\"/>\n"
		"\t\t<Board Socket=\"4\" Type=\"SD Card Board\">\n"
		"\t\t\t<Card Socket=\"1\" File=\"MainboardCard.img\"/>\n"
		"\t\t</Board>\n"
		"\t\t<Board Socket=\"17\" Type=\"Sound Board\"/>\n"
		"\t\t<Board Socket=\"18\" Type=\"Memory Board\"/>\n"
		"\t\t<Board Socket=\"19\" Type=\"Sound Board\"/>\n"
		"\t</Processor>\n"
		"</Mainboard>\n", cText.Text());
	cText.Kill();
	cWritten.Init();
	AssertTrue(cWritten.Read("Output" _FS_ "MainboardWritten.xml", &szError));
	AssertString("", szError.Text());
	AssertInt(HMB_Small, cWritten.GetType());
	AssertString(HCP65_UART_NAME, cWritten.GetBoardType(3));
	AssertString(HCP65_SD_NAME, cWritten.GetBoardType(4));
	AssertTrue(cWritten.GetBoardType(16) == NULL);
	AssertString(HCP65_AUDIO_NAME, cWritten.GetBoardType(17));
	AssertString(HCP65_MEMORY_BOARD_NAME, cWritten.GetBoardType(18));
	AssertString(HCP65_AUDIO_NAME, cWritten.GetBoardType(19));
	AssertInt(0, ((CHCP65SDCard*)cWritten.GetBoard(4))->NumSectors(0));
	AssertInt(2, ((CHCP65SDCard*)cWritten.GetBoard(4))->NumSectors(1));
	cWritten.Kill();

	cHCP65.Kill();
	cMainboard.Kill();
	szError.Kill();
	DataMemoryKill();
}


//////////////////////////////////////////////////////////////////////////
//
// One W65C22S on its own, in its PHI2 ticks: the ports and their bus
// holders, the timers, the shift register, the control lines'
// handshakes and flags, and reset.
//
//////////////////////////////////////////////////////////////////////////
void TestHCP65VIA(void)
{
	CHCP65VIA	cVIA;
	uint64		uiTick;
	uint16		uiValue;
	int			i;

	cVIA.Init();

	//Reset: every pin an input and nothing enabled.  IER reads D7 set.
	AssertInt(0, cVIA.Read(HCP65_VIA_DDRA));
	AssertInt(0, cVIA.Read(HCP65_VIA_ACR));
	AssertInt(0, cVIA.Read(HCP65_VIA_IFR));
	AssertInt(0x80, cVIA.Read(HCP65_VIA_IER));
	AssertFalse(cVIA.IsInterrupting());
	AssertLong((int64)HCP65_NEVER, (int64)cVIA.NextEvent());

	//Port A reads its pins.  What the host drives on an output is ignored, and a pin nothing drives keeps its level.
	cVIA.Write(HCP65_VIA_DDRA, 0x0F);
	cVIA.Write(HCP65_VIA_ORA, 0x05);
	AssertInt(0xF5, cVIA.GetPortA());
	cVIA.HostDrivePortA(0x3A, 0xFF);
	AssertInt(0x35, cVIA.Read(HCP65_VIA_ORA_NH));
	cVIA.HostDrivePortA(0, 0);
	AssertInt(0x35, cVIA.Read(HCP65_VIA_ORA_NH));
	cVIA.Write(HCP65_VIA_DDRA, 0xFF);
	AssertInt(0x05, cVIA.GetPortA());
	cVIA.Write(HCP65_VIA_DDRA, 0x00);
	AssertInt(0x05, cVIA.GetPortA());

	//Port B reads ORB where it is an output.
	cVIA.Write(HCP65_VIA_DDRB, 0xF0);
	cVIA.Write(HCP65_VIA_ORB, 0xA5);
	cVIA.HostDrivePortB(0x0F, 0xFF);
	AssertInt(0xAF, cVIA.GetPortB());
	AssertInt(0xAF, cVIA.Read(HCP65_VIA_ORB));
	cVIA.Write(HCP65_VIA_DDRB, 0x00);
	cVIA.HostDrivePortB(0, 0);

	//One shot T1 of 10, written at tick 100: 10 on the next tick, 0 ten later, and the time out the tick after that.  It
	//sets the flag once.
	cVIA.CatchUp(100);
	cVIA.Write(HCP65_VIA_IER, HCP65_VIA_INT_ANY | HCP65_VIA_INT_T1);
	AssertInt(0xC0, cVIA.Read(HCP65_VIA_IER));
	cVIA.Write(HCP65_VIA_T1CL, 10);
	cVIA.Write(HCP65_VIA_T1CH, 0);
	AssertLong(112, (int64)cVIA.NextEvent());
	cVIA.CatchUp(101);
	AssertInt(10, cVIA.GetT1Counter());
	cVIA.CatchUp(111);
	AssertInt(0, cVIA.GetT1Counter());
	AssertFalse(cVIA.IsInterrupting());
	cVIA.CatchUp(112);
	AssertInt(0xFFFF, cVIA.GetT1Counter());
	AssertTrue(cVIA.IsInterrupting());
	AssertInt(0xC0, cVIA.Read(HCP65_VIA_IFR));
	AssertLong((int64)HCP65_NEVER, (int64)cVIA.NextEvent());
	cVIA.CatchUp(113);
	AssertInt(0xFE, cVIA.Read(HCP65_VIA_T1CL));
	AssertFalse(cVIA.IsInterrupting());
	AssertLong((int64)HCP65_NEVER, (int64)cVIA.NextEvent());
	cVIA.CatchUp(112 + 0x10000 + 5);
	AssertFalse(cVIA.IsInterrupting());
	AssertInt(10, cVIA.Read(HCP65_VIA_T1LL));

	//Continuous T1 of 4 times out every 6 ticks, inverting PB7, which went low when T1C-H was written.
	uiTick = 200000;
	cVIA.CatchUp(uiTick);
	cVIA.Write(HCP65_VIA_ACR, HCP65_VIA_ACR_T1_CONTINUOUS | HCP65_VIA_ACR_T1_PB7);
	cVIA.Write(HCP65_VIA_T1CL, 4);
	cVIA.Write(HCP65_VIA_T1CH, 0);
	AssertInt(0x00, cVIA.GetPortB() & 0x80);
	cVIA.CatchUp(uiTick + 6);
	AssertInt(0x80, cVIA.GetPortB() & 0x80);
	AssertInt(0x80, cVIA.Read(HCP65_VIA_ORB) & 0x80);
	AssertTrue(cVIA.IsInterrupting());
	cVIA.Read(HCP65_VIA_T1CL);
	AssertFalse(cVIA.IsInterrupting());
	AssertLong((int64)(uiTick + 12), (int64)cVIA.NextEvent());
	cVIA.CatchUp(uiTick + 7);
	AssertInt(4, cVIA.GetT1Counter());
	cVIA.CatchUp(uiTick + 12);
	AssertInt(0x00, cVIA.GetPortB() & 0x80);
	AssertTrue(cVIA.IsInterrupting());

	//1001 more time outs, worked out rather than counted.
	cVIA.CatchUp(uiTick + 12 + 6 * 1001);
	AssertInt(0x80, cVIA.GetPortB() & 0x80);
	AssertInt(0xFFFF, cVIA.GetT1Counter());

	//A new latch, written at a time out, is what reloads on the next tick.
	cVIA.Write(HCP65_VIA_T1LL, 10);
	cVIA.Read(HCP65_VIA_T1CL);
	AssertLong((int64)(uiTick + 12 + 6 * 1001 + 12), (int64)cVIA.NextEvent());
	cVIA.CatchUp(uiTick + 12 + 6 * 1001 + 1);
	AssertInt(10, cVIA.GetT1Counter());

	//Timed T2 of 5 times out 7 ticks after it is written, once.
	cVIA.Write(HCP65_VIA_ACR, 0);
	cVIA.Write(HCP65_VIA_IER, HCP65_VIA_INT_FLAGS);
	cVIA.Write(HCP65_VIA_IER, HCP65_VIA_INT_ANY | HCP65_VIA_INT_T2);
	uiTick = cVIA.GetTick();
	cVIA.Write(HCP65_VIA_T2CL, 5);
	cVIA.Write(HCP65_VIA_T2CH, 0);
	AssertLong((int64)(uiTick + 7), (int64)cVIA.NextEvent());
	cVIA.CatchUp(uiTick + 6);
	AssertInt(0, cVIA.GetT2Counter());
	AssertFalse(cVIA.IsInterrupting());
	cVIA.CatchUp(uiTick + 7);
	AssertTrue(cVIA.IsInterrupting());
	AssertInt(0xFF, cVIA.Read(HCP65_VIA_T2CL));
	AssertFalse(cVIA.IsInterrupting());
	cVIA.CatchUp(uiTick + 7 + 0x10000 + 1);
	AssertFalse(cVIA.IsInterrupting());

	//Counting PB6's falls, T2 sets its flag on reaching 0 and doesn't count PHI2.
	cVIA.Write(HCP65_VIA_ACR, HCP65_VIA_ACR_T2_PULSES);
	cVIA.HostDrivePortB(0x40, 0x40);
	cVIA.Write(HCP65_VIA_T2CL, 3);
	cVIA.Write(HCP65_VIA_T2CH, 0);
	for (i = 0; i < 3; i++)
	{
		AssertFalse(cVIA.IsInterrupting());
		cVIA.HostDrivePortB(0x00, 0x40);
		cVIA.HostDrivePortB(0x40, 0x40);
	}
	AssertTrue(cVIA.IsInterrupting());
	cVIA.CatchUp(cVIA.GetTick() + 1000);
	AssertInt(0, cVIA.GetT2Counter());
	cVIA.Read(HCP65_VIA_T2CL);
	AssertFalse(cVIA.IsInterrupting());
	cVIA.HostDrivePortB(0, 0);

	//Shifting out under PHI2: an edge of CB1 every tick, CB2 changing on the falls, D7 first.  Eight bits put $A5 back.
	cVIA.Write(HCP65_VIA_IER, HCP65_VIA_INT_FLAGS);
	cVIA.Write(HCP65_VIA_IER, HCP65_VIA_INT_ANY | HCP65_VIA_INT_SR);
	cVIA.Write(HCP65_VIA_ACR, HCP65_VIA_SR_OUT_PHI2 << HCP65_VIA_ACR_SR_SHIFT);
	uiTick = cVIA.GetTick();
	cVIA.Write(HCP65_VIA_SR, 0xA5);
	AssertLong((int64)(uiTick + 16), (int64)cVIA.NextEvent());
	AssertTrue(cVIA.GetCB1());
	cVIA.CatchUp(uiTick + 1);
	AssertFalse(cVIA.GetCB1());
	AssertTrue(cVIA.GetCB2());
	cVIA.CatchUp(uiTick + 2);
	AssertTrue(cVIA.GetCB1());
	cVIA.CatchUp(uiTick + 3);
	AssertFalse(cVIA.GetCB2());
	cVIA.CatchUp(uiTick + 15);
	AssertFalse(cVIA.IsInterrupting());
	cVIA.CatchUp(uiTick + 16);
	AssertTrue(cVIA.IsInterrupting());
	AssertInt(0x84, cVIA.Read(HCP65_VIA_IFR));
	AssertTrue(cVIA.GetCB1());
	AssertTrue(cVIA.GetCB2());
	AssertInt(0xA5, cVIA.Read(HCP65_VIA_SR));
	AssertFalse(cVIA.IsInterrupting());

	//Shifting in on the host's CB1: CB2 at each rise, into D0.
	cVIA.Write(HCP65_VIA_ACR, HCP65_VIA_SR_IN_CB1 << HCP65_VIA_ACR_SR_SHIFT);
	cVIA.Read(HCP65_VIA_SR);
	for (i = 7; i >= 0; i--)
	{
		AssertFalse(cVIA.IsInterrupting());
		cVIA.HostSetCB2(((0x5A >> i) & 1) != 0);
		cVIA.HostSetCB1(false);
		cVIA.HostSetCB1(true);
	}
	AssertTrue(cVIA.IsInterrupting());
	AssertInt(0x5A, cVIA.Read(HCP65_VIA_SR));
	AssertFalse(cVIA.IsInterrupting());

	//Shifting out free running at T2's rate (T2's low latch 0: an edge every 2 ticks) goes round and round without a
	//flag.  16001 edges are one.
	cVIA.Write(HCP65_VIA_T2CL, 0);
	cVIA.Write(HCP65_VIA_ACR, HCP65_VIA_SR_OUT_FREE << HCP65_VIA_ACR_SR_SHIFT);
	uiTick = cVIA.GetTick();
	cVIA.Write(HCP65_VIA_SR, 0x81);
	AssertLong((int64)HCP65_NEVER, (int64)cVIA.NextEvent());
	cVIA.CatchUp(uiTick + 2 * 16001);
	AssertFalse(cVIA.IsInterrupting());
	AssertFalse(cVIA.GetCB1());
	AssertTrue(cVIA.GetCB2());
	AssertInt(0x03, cVIA.Read(HCP65_VIA_SR));

	//CA2's handshake: low on reading ORA, high on CA1's active edge (here positive), which sets CA1's flag.  ORA without
	//handshake leaves the flag.
	cVIA.Write(HCP65_VIA_ACR, 0);
	cVIA.Write(HCP65_VIA_IER, HCP65_VIA_INT_FLAGS);
	cVIA.Write(HCP65_VIA_IFR, HCP65_VIA_INT_FLAGS);
	cVIA.Write(HCP65_VIA_PCR, HCP65_VIA_PCR_CA1_POSITIVE | (HCP65_VIA_C2_HANDSHAKE << 1));
	AssertTrue(cVIA.GetCA2());
	cVIA.Read(HCP65_VIA_ORA);
	AssertFalse(cVIA.GetCA2());
	cVIA.HostSetCA1(false);
	AssertInt(0, cVIA.Read(HCP65_VIA_IFR));
	cVIA.HostSetCA1(true);
	AssertInt(HCP65_VIA_INT_CA1, cVIA.Read(HCP65_VIA_IFR));
	AssertTrue(cVIA.GetCA2());
	cVIA.Read(HCP65_VIA_ORA_NH);
	AssertInt(HCP65_VIA_INT_CA1, cVIA.Read(HCP65_VIA_IFR));
	cVIA.Read(HCP65_VIA_ORA);
	AssertInt(0, cVIA.Read(HCP65_VIA_IFR));
	AssertFalse(cVIA.GetCA2());

	//Latching: port A as it was at CA1's active edge.
	cVIA.Write(HCP65_VIA_ACR, HCP65_VIA_ACR_PA_LATCH);
	cVIA.HostDrivePortA(0x11, 0xFF);
	cVIA.HostSetCA1(false);
	cVIA.HostSetCA1(true);
	cVIA.HostDrivePortA(0x22, 0xFF);
	AssertInt(0x11, cVIA.Read(HCP65_VIA_ORA_NH));
	AssertInt(0x22, cVIA.GetPortA());
	cVIA.Write(HCP65_VIA_ACR, 0);
	AssertInt(0x22, cVIA.Read(HCP65_VIA_ORA_NH));

	//CA2's pulse lasts a tick, and is counted.
	cVIA.Write(HCP65_VIA_PCR, HCP65_VIA_C2_PULSE << 1);
	AssertTrue(cVIA.GetCA2());
	cVIA.Write(HCP65_VIA_ORA, 0);
	AssertFalse(cVIA.GetCA2());
	AssertLong(1, (int64)cVIA.NumCA2Pulses());
	cVIA.CatchUp(cVIA.GetTick() + 1);
	AssertTrue(cVIA.GetCA2());

	//CB2 as an independent input: ORB leaves its flag, writing it to the IFR clears it.  Otherwise ORB clears it.
	cVIA.Write(HCP65_VIA_PCR, HCP65_VIA_C2_INDEPENDENT << 5);
	cVIA.HostSetCB2(true);
	cVIA.HostSetCB2(false);
	AssertInt(HCP65_VIA_INT_CB2, cVIA.Read(HCP65_VIA_IFR));
	cVIA.Read(HCP65_VIA_ORB);
	AssertInt(HCP65_VIA_INT_CB2, cVIA.Read(HCP65_VIA_IFR));
	cVIA.Write(HCP65_VIA_IFR, HCP65_VIA_INT_CB2);
	AssertInt(0, cVIA.Read(HCP65_VIA_IFR));
	cVIA.Write(HCP65_VIA_PCR, 0);
	cVIA.HostSetCB2(true);
	cVIA.HostSetCB2(false);
	AssertInt(HCP65_VIA_INT_CB2, cVIA.Read(HCP65_VIA_IFR));

	//IFR's D7 is any enabled flag; IER is set and cleared a bit at a time.
	cVIA.Write(HCP65_VIA_IER, HCP65_VIA_INT_ANY | HCP65_VIA_INT_CB2);
	AssertInt(HCP65_VIA_INT_ANY | HCP65_VIA_INT_CB2, cVIA.Read(HCP65_VIA_IFR));
	AssertTrue(cVIA.IsInterrupting());
	cVIA.Write(HCP65_VIA_IER, HCP65_VIA_INT_CB2);
	AssertInt(HCP65_VIA_INT_CB2, cVIA.Read(HCP65_VIA_IFR));
	AssertFalse(cVIA.IsInterrupting());
	cVIA.Write(HCP65_VIA_ORB, 0);
	AssertInt(0, cVIA.Read(HCP65_VIA_IFR));

	//Reset keeps the timers' latches.  Held in reset, the timers stop.
	cVIA.Write(HCP65_VIA_T1LL, 0x34);
	cVIA.Write(HCP65_VIA_T1LH, 0x12);
	cVIA.Write(HCP65_VIA_PCR, 0xFF);
	cVIA.Reset();
	AssertInt(0x34, cVIA.Read(HCP65_VIA_T1LL));
	AssertInt(0x12, cVIA.Read(HCP65_VIA_T1LH));
	AssertInt(0, cVIA.Read(HCP65_VIA_PCR));
	cVIA.SetFrozen(true);
	uiValue = cVIA.GetT1Counter();
	cVIA.CatchUp(cVIA.GetTick() + 5000);
	AssertInt(uiValue, cVIA.GetT1Counter());
	cVIA.SetFrozen(false);
	cVIA.CatchUp(cVIA.GetTick() + 1);
	AssertInt((uint16)(uiValue - 1), cVIA.GetT1Counter());

	cVIA.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The VIA Board decodes A7-A0: A6-A4 pick the VIA, A3-A0 its register,
// and $80 reads the interrupts.  A VIA whose breakout isn't fitted reads
// $FF and doesn't interrupt.  Writes are ignored while the board is held
// in reset.
//
//////////////////////////////////////////////////////////////////////////
void TestHCP65VIABoard(void)
{
	CHCP65VIABoard	cBoard;

	cBoard.Init();
	AssertInt(0x1A, cBoard.Read(0));		//The device ROM's magic, $C0DAFE1A.
	AssertInt(0xFF, cBoard.Read(0x800));
	AssertInt(8, cBoard.NumVIAs());
	AssertString("XJ", CHCP65VIABoard::GetVIAName(4));

	cBoard.Write(0x400 + HCP65_VIA_DDRA, 0xFF);
	AssertInt(0, cBoard.Read(0x400 + HCP65_VIA_DDRA));
	cBoard.SetHeldInReset(false);
	cBoard.Write(0x400 + HCP65_VIA_DDRA, 0x12);
	cBoard.Write(0x470 + HCP65_VIA_DDRA, 0x34);
	AssertInt(0x12, cBoard.Read(0x400 + HCP65_VIA_DDRA));
	AssertInt(0x34, cBoard.Read(0x470 + HCP65_VIA_DDRA));
	AssertInt(0x12, cBoard.Read(0x500 + HCP65_VIA_DDRA));
	AssertInt(0x34, cBoard.Read(0x770 + HCP65_VIA_DDRA));
	AssertInt(0x12, cBoard.GetVIA(0)->Read(HCP65_VIA_DDRA));
	AssertInt(0, cBoard.Read(0x410 + HCP65_VIA_DDRA));
	AssertInt(0xFF, cBoard.Read(0x481));
	AssertInt(0xFF, cBoard.Read(0x490));

	//CD's CA1 flag, enabled: it is D1 of the interrupt register.  Writing that register does nothing.
	AssertInt(0, cBoard.Read(0x480));
	cBoard.Write(0x410 + HCP65_VIA_IER, HCP65_VIA_INT_ANY | HCP65_VIA_INT_CA1);
	cBoard.GetVIA(1)->HostSetCA1(false);
	AssertTrue(cBoard.IsInterrupting());
	AssertInt(0x02, cBoard.Read(0x480));
	AssertInt(0x02, cBoard.Read(0x780));
	cBoard.Write(0x480, 0xFF);
	AssertInt(0x02, cBoard.Read(0x480));

	//Without breakout 0, AB and CD aren't there: they read $FF and don't interrupt.  Fitted again, they are as they were.
	cBoard.SetBreakout(0, false);
	AssertInt(6, cBoard.NumVIAs());
	AssertNull(cBoard.GetVIA(1));
	AssertNotNull(cBoard.GetVIA(2));
	AssertInt(0xFF, cBoard.Read(0x400 + HCP65_VIA_DDRA));
	AssertInt(0, cBoard.Read(0x480));
	AssertFalse(cBoard.IsInterrupting());
	cBoard.Write(0x400 + HCP65_VIA_DDRA, 0x56);
	cBoard.SetBreakout(0, true);
	AssertInt(0x12, cBoard.Read(0x400 + HCP65_VIA_DDRA));
	AssertInt(0x02, cBoard.Read(0x480));

	//Held in reset, the VIAs are reset.
	cBoard.SetHeldInReset(true);
	AssertFalse(cBoard.IsInterrupting());
	AssertInt(0, cBoard.Read(0x480));
	AssertInt(0, cBoard.Read(0x470 + HCP65_VIA_DDRA));

	cBoard.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The VIA Board in socket 3, its PHI2 half the CPU's clock: AB's one shot
// T1 interrupts the processor on its time out, N+2 ticks after T1C-H is
// written.
//
//////////////////////////////////////////////////////////////////////////
void TestHCP65VIAInterrupt(void)
{
	CHCP65				cHCP65;
	CHCP65VIABoard		cVIA;
	CHCP65MemoryBoard	cMemory;
	uint8*				puiRAM;
	uint64				uiWriteCycle;
	uint64				uiDue;

	static const uint8 auiIrq[] = {
		0xAF, 0x80, 0x84, 0x11,		//lda $118480 (interrupts)
		0x8F, 0x10, 0x00, 0x20,		//sta $200010
		0xAF, 0x0D, 0x84, 0x11,		//lda $11840D (AB's IFR)
		0x8F, 0x11, 0x00, 0x20,		//sta $200011
		0xAF, 0x04, 0x84, 0x11,		//lda $118404 (AB's T1C-L, clearing the flag)
		0xDB };						//stp

	cHCP65.Init(TEST_HCP65_CLOCK_RATE);
	TestHCP65AddMemory(&cHCP65, &cMemory);
	cVIA.Init();
	AssertTrue(cHCP65.SetDevice(3, &cVIA));
	puiRAM = TestHCP65WaitingKernal(&cHCP65, 0x8200, 0x8300);
	TestHCP65Put(puiRAM, 0x8200, auiIrq, sizeof(auiIrq));

	cHCP65.Reset();
	AssertTrue(cVIA.IsHeldInReset());
	cHCP65.Write(0x100003, 0x08);
	AssertFalse(cVIA.IsHeldInReset());
	AssertInt(0x1A, cHCP65.Read(0x118000));
	cHCP65.Write(0x11840E, HCP65_VIA_INT_ANY | HCP65_VIA_INT_T1);
	cHCP65.Write(0x118404, 100);
	uiWriteCycle = cHCP65.NumCycles();
	cHCP65.Write(0x118405, 0);

	cHCP65.Run(100000);
	AssertTrue(cHCP65.IsStopped());
	AssertInt(0x01, *TestHCP65Main(&cHCP65, 0x200010));
	AssertInt(0xC0, *TestHCP65Main(&cHCP65, 0x200011));
	uiDue = cHCP65.CycleOfDeviceTick(cHCP65.DeviceTick(uiWriteCycle, TEST_HCP65_CLOCK_RATE / 2) + 100 + 2, TEST_HCP65_CLOCK_RATE / 2);
	AssertTrue(cHCP65.NumCycles() >= uiDue);
	AssertTrue(cHCP65.NumCycles() < uiDue + 60);
	AssertFalse(cVIA.IsInterrupting());
	AssertInt(0x08, cHCP65.Read(0x100000) & 0x08);

	cHCP65.Kill();
	cVIA.Kill();
	cMemory.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A VIA Board's <Breakout>s say which breakouts are fitted; none is all
// four.  It is written the same way.
//
//////////////////////////////////////////////////////////////////////////
void TestHCP65VIABreakouts(void)
{
	CHCP65Mainboard		cMainboard;
	CHCP65VIABoard*		pcVIA;
	CChars				szError;
	CFileUtil			cFileUtil;
	CTextFile			cText;

	DataMemoryInit();
	szError.Init();
	cFileUtil.TouchDir("Output");
	TestHCP65WriteFile("Output" _FS_ "VIAMainboard.xml",
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		"<Mainboard Type=\"Small\"><Processor>"
		"<Board Socket=\"3\" Type=\"VIA Board\"/>"
		"<Board Socket=\"4\" Type=\"VIA Board\"><Breakout Socket=\"2\"/><Breakout Socket=\"0\"/></Board>"
		"</Processor></Mainboard>", 0);
	cMainboard.Init();
	AssertTrue(cMainboard.Read("Output" _FS_ "VIAMainboard.xml", &szError));
	AssertString("", szError.Text());
	pcVIA = (CHCP65VIABoard*)cMainboard.GetBoard(3);
	AssertInt(8, pcVIA->NumVIAs());
	pcVIA = (CHCP65VIABoard*)cMainboard.GetBoard(4);
	AssertInt(4, pcVIA->NumVIAs());
	AssertNotNull(pcVIA->GetVIA(1));
	AssertNull(pcVIA->GetVIA(2));
	AssertNotNull(pcVIA->GetVIA(5));
	AssertNull(pcVIA->GetVIA(7));

	AssertTrue(cMainboard.Write("Output" _FS_ "VIAMainboardWritten.xml", &szError));
	cText.Init();
	AssertTrue(cText.Read("Output" _FS_ "VIAMainboardWritten.xml"));
	AssertString("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		"<Mainboard Type=\"Small\">\n"
		"\t<Processor Index=\"0\">\n"
		"\t\t<Board Socket=\"3\" Type=\"VIA Board\"/>\n"
		"\t\t<Board Socket=\"4\" Type=\"VIA Board\">\n"
		"\t\t\t<Breakout Socket=\"0\"/>\n"
		"\t\t\t<Breakout Socket=\"2\"/>\n"
		"\t\t</Board>\n"
		"\t</Processor>\n"
		"</Mainboard>\n", cText.Text());
	cText.Kill();
	cMainboard.Kill();

	TestHCP65WriteFile("Output" _FS_ "VIAMainboard.xml",
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		"<Mainboard Type=\"Small\"><Processor>"
		"<Board Socket=\"3\" Type=\"VIA Board\"><Breakout Socket=\"4\"/></Board>"
		"</Processor></Mainboard>", 0);
	cMainboard.Init();
	AssertFalse(cMainboard.Read("Output" _FS_ "VIAMainboard.xml", &szError));
	AssertString("Output" _FS_ "VIAMainboard.xml: Bad number in Socket (<Breakout> line 2)", szError.Text());
	cMainboard.Kill();

	TestHCP65WriteFile("Output" _FS_ "VIAMainboard.xml",
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		"<Mainboard Type=\"Small\"><Processor>"
		"<Board Socket=\"3\" Type=\"VIA Board\"><Breakout Socket=\"1\"/><Breakout Socket=\"1\"/></Board>"
		"</Processor></Mainboard>", 0);
	cMainboard.Init();
	AssertFalse(cMainboard.Read("Output" _FS_ "VIAMainboard.xml", &szError));
	AssertString("Output" _FS_ "VIAMainboard.xml: A breakout is in this socket already (<Breakout> line 2)", szError.Text());
	cMainboard.Kill();

	szError.Kill();
	DataMemoryKill();
}


//////////////////////////////////////////////////////////////////////////
//
// The computer sends uiByte to pcPort's far end as a driver does through
// the GPIO: Clock low for 150us, Data low, Clock let go (the request to
// send), then each bit while the far end holds Clock low, and Data let go
// for the stop bit.  True if the far end acknowledged (Data low on its
// eleventh clock).
//
//////////////////////////////////////////////////////////////////////////
static bool TestHCP65PS2Send(CHCP65PS2Port* pcPort, uint8 uiByte)
{
	uint64	uiTick;
	uint64	uiEnd;
	uint16	uiFrame;
	int		iOnes;
	int		iFall;
	int		i;
	bool	bClock;
	bool	bAcknowledged;

	iOnes = 0;
	for (i = 0; i < 8; i++)
	{
		iOnes += (uiByte >> i) & 1;
	}
	uiFrame = (uint16)(uiByte | ((iOnes & 1) ? 0 : 0x100) | 0x200);

	uiTick = pcPort->GetTick();
	pcPort->SetBoardDrive(true, false);
	uiTick += 150;
	pcPort->CatchUp(uiTick);
	pcPort->SetBoardDrive(true, true);
	pcPort->SetBoardDrive(false, true);

	bAcknowledged = false;
	iFall = 0;
	bClock = pcPort->IsClockHigh();
	for (uiEnd = uiTick + 3000; uiTick < uiEnd; uiTick++)
	{
		pcPort->CatchUp(uiTick);
		if (bClock && !pcPort->IsClockHigh())
		{
			iFall++;
			if (iFall <= 10)
			{
				pcPort->SetBoardDrive(false, ((uiFrame >> (iFall - 1)) & 1) == 0);
			}
			else if (iFall == 11)
			{
				bAcknowledged = !pcPort->IsDataHigh();
			}
		}
		bClock = pcPort->IsClockHigh();
	}
	return bAcknowledged;
}


//////////////////////////////////////////////////////////////////////////
//
// One PS/2 port on its own, in microseconds: the far end's frames, the
// packet and code they make, the FIFO, the computer inhibiting the far
// end, and the computer sending it a byte.
//
//////////////////////////////////////////////////////////////////////////
void TestHCP65PS2Port(void)
{
	CHCP65PS2Port	cPort;
	uint8			auiBytes[4];
	uint64			uiStart;

	cPort.Init();
	cPort.SetHeld(false);
	AssertTrue(cPort.IsClockHigh());
	AssertTrue(cPort.IsDataHigh());
	AssertLong((int64)HCP65_NEVER, (int64)cPort.NextEvent());

	//$1C (A) goes once the gap after power on is over: bit n's Clock is low from 1020 + 80n to 1060 + 80n.  The packet
	//ends 630 after the last rise, and the code 6700 after that.
	cPort.HostSend((const uint8*)"\x1C", 1);
	AssertLong(1000, (int64)cPort.NextEvent());
	cPort.CatchUp(1859);
	AssertFalse(cPort.IsClockHigh());
	cPort.CatchUp(1860);
	AssertTrue(cPort.IsClockHigh());
	AssertSize(0, cPort.NumHostToSend());
	cPort.CatchUp(2489);
	AssertFalse(cPort.IsPacketWaiting());
	AssertFalse(cPort.IsOutputReady());
	cPort.CatchUp(2490);
	AssertTrue(cPort.IsPacketWaiting());
	AssertTrue(cPort.IsOutputReady());
	AssertFalse(cPort.IsCodeWaiting());
	AssertInt(0x1C, cPort.ReadFIFO());
	AssertFalse(cPort.IsOutputReady());
	AssertInt(0x1C, cPort.ReadFIFO());
	cPort.CatchUp(9189);
	AssertFalse(cPort.IsCodeWaiting());
	cPort.CatchUp(9190);
	AssertTrue(cPort.IsCodeWaiting());
	cPort.ClearWaiting();
	AssertFalse(cPort.IsPacketWaiting());
	AssertFalse(cPort.IsCodeWaiting());

	//E0 70 (Insert) is two packets, 1000 apart, and one code.
	cPort.HostSend((const uint8*)"\xE0\x70", 2);
	cPort.CatchUp(9190 + 860 + 630);
	AssertTrue(cPort.IsPacketWaiting());
	cPort.ClearWaiting();
	cPort.CatchUp(9190 + 860 + 1000 + 860 + 630 - 1);
	AssertFalse(cPort.IsPacketWaiting());
	cPort.CatchUp(9190 + 860 + 1000 + 860 + 630);
	AssertTrue(cPort.IsPacketWaiting());
	cPort.CatchUp(9190 + 860 + 1000 + 860 + 630 + 6700 - 1);
	AssertFalse(cPort.IsCodeWaiting());
	cPort.CatchUp(9190 + 860 + 1000 + 860 + 630 + 6700);
	AssertTrue(cPort.IsCodeWaiting());
	AssertInt(0xE0, cPort.ReadFIFO());
	AssertInt(0x70, cPort.ReadFIFO());
	AssertFalse(cPort.IsOutputReady());
	cPort.ClearWaiting();

	//The computer holds Clock low: the far end waits.  Holding it is a fall of Clock that shifts and starts a packet,
	//but the far end's frame follows within the packet's quiet, so its last eleven bits are the byte.
	uiStart = cPort.GetTick() + 10000;
	cPort.CatchUp(uiStart);
	cPort.SetBoardDrive(true, false);
	cPort.HostSend((const uint8*)"\x55", 1);
	cPort.CatchUp(uiStart + 5000);
	AssertSize(1, cPort.NumHostToSend());
	AssertFalse(cPort.IsPacketWaiting());
	cPort.SetBoardDrive(false, false);
	cPort.CatchUp(uiStart + 5050 + 860 + 630);
	AssertSize(0, cPort.NumHostToSend());
	AssertTrue(cPort.IsPacketWaiting());
	AssertInt(0x55, cPort.ReadFIFO());
	AssertFalse(cPort.IsOutputReady());
	cPort.ClearWaiting();

	//Clock held low in the middle of a frame gives it up (at the next bit's Clock low); it is sent again after the gap.
	//What the computer did makes a packet of its own first, which the driver must throw away.
	uiStart = cPort.GetTick() + 10000;
	cPort.CatchUp(uiStart);
	cPort.HostSend((const uint8*)"\xAA", 1);
	uiStart = cPort.NextEvent();
	cPort.CatchUp(uiStart + 310);
	cPort.SetBoardDrive(true, false);
	cPort.CatchUp(uiStart + 500);
	AssertLong(1, (int64)cPort.NumAborts());
	AssertSize(1, cPort.NumHostToSend());
	cPort.SetBoardDrive(false, false);
	cPort.CatchUp(uiStart + 340 + 1000 + 860 + 630);
	AssertSize(0, cPort.NumHostToSend());
	cPort.ReadFIFO();
	AssertInt(0xAA, cPort.ReadFIFO());
	AssertFalse(cPort.IsOutputReady());
	cPort.ClearWaiting();

	//The computer sends ED (set the LEDs): the far end clocks it in and acknowledges.
	cPort.CatchUp(cPort.GetTick() + 10000);
	AssertTrue(TestHCP65PS2Send(&cPort, 0xED));
	AssertSize(1, cPort.HostReceive(auiBytes, 4));
	AssertInt(0xED, auiBytes[0]);
	AssertLong(0, (int64)cPort.NumErrors());

	cPort.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The bytes in pcPort's FIFO, once the far end has nothing more to send
// and the last packet has ended, into puiDest.
//
//////////////////////////////////////////////////////////////////////////
static size TestHCP65PS2Drain(CHCP65PS2Port* pcPort, uint8* puiDest, size uiMax)
{
	size	uiCount;

	pcPort->CatchUp(pcPort->GetTick());
	while (pcPort->NextEvent() != HCP65_NEVER)
	{
		pcPort->CatchUp(pcPort->NextEvent());
	}
	uiCount = 0;
	while (pcPort->IsOutputReady() && (uiCount < uiMax))
	{
		puiDest[uiCount++] = pcPort->ReadFIFO();
	}
	pcPort->ClearWaiting();
	return uiCount;
}


//////////////////////////////////////////////////////////////////////////
//
// A keyboard in a port: AA when it is plugged in, its answers to the
// computer's commands, and its keys' scan codes, which it drops while
// disabled.
//
//////////////////////////////////////////////////////////////////////////
void TestHCP65PS2Keyboard(void)
{
	CHCP65PS2Port		cPort;
	CHCP65PS2Keyboard	cKeyboard;
	uint8				auiBytes[16];
	size				uiCount;

	cPort.Init();
	cPort.SetHeld(false);
	cKeyboard.Init();
	cKeyboard.Attach(&cPort);
	AssertInt(1, (int)TestHCP65PS2Drain(&cPort, auiBytes, 16));
	AssertInt(HCP65_PS2_KEYBOARD_BAT_OK, auiBytes[0]);

	//A byte the computer sends makes a packet of its own (here the last 11 clocks: the acknowledge and the bits before).
	AssertTrue(TestHCP65PS2Send(&cPort, HCP65_PS2_KEYBOARD_RESET));
	uiCount = TestHCP65PS2Drain(&cPort, auiBytes, 16);
	AssertInt(3, (int)uiCount);
	AssertInt(HCP65_PS2_KEYBOARD_ACK, auiBytes[1]);
	AssertInt(HCP65_PS2_KEYBOARD_BAT_OK, auiBytes[2]);

	AssertTrue(TestHCP65PS2Send(&cPort, HCP65_PS2_KEYBOARD_SET_LEDS));
	AssertTrue(TestHCP65PS2Send(&cPort, HCP65_PS2_KEYBOARD_LED_CAPS));
	AssertInt(HCP65_PS2_KEYBOARD_LED_CAPS, cKeyboard.GetLEDs());
	TestHCP65PS2Drain(&cPort, auiBytes, 16);

	AssertTrue(cKeyboard.Key("Cursor Up", true));
	AssertTrue(cKeyboard.Key("Cursor Up", false));
	AssertFalse(cKeyboard.Key("No Such Key", true));
	uiCount = TestHCP65PS2Drain(&cPort, auiBytes, 16);
	AssertInt(5, (int)uiCount);
	AssertInt(0xE0, auiBytes[0]);
	AssertInt(0x75, auiBytes[1]);
	AssertInt(0xE0, auiBytes[2]);
	AssertInt(0xF0, auiBytes[3]);
	AssertInt(0x75, auiBytes[4]);

	//Disabled, it drops its keys.
	AssertTrue(TestHCP65PS2Send(&cPort, HCP65_PS2_KEYBOARD_DISABLE));
	TestHCP65PS2Drain(&cPort, auiBytes, 16);
	AssertFalse(cKeyboard.IsScanning());
	cKeyboard.Key("A", true);
	AssertInt(0, (int)TestHCP65PS2Drain(&cPort, auiBytes, 16));

	//Print Screen and Pause have codes of their own; Pause has no break code.
	AssertInt(6, (int)CHCP65PS2Keyboard::MakeCode("Print Screen", false, auiBytes));
	AssertInt(0x12, auiBytes[5]);
	AssertInt(8, (int)CHCP65PS2Keyboard::MakeCode("Pause/Break", true, auiBytes));
	AssertInt(0, (int)CHCP65PS2Keyboard::MakeCode("Pause/Break", false, auiBytes));
	AssertInt(2, (int)CHCP65PS2Keyboard::MakeCode("A", false, auiBytes));

	cKeyboard.Kill();
	AssertTrue(cPort.GetDevice() == NULL);
	cPort.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The terminal at the far end of a UART: what it is sent, as a teletype
// shows it.
//
//////////////////////////////////////////////////////////////////////////
void TestHCP65Terminal(void)
{
	CHCP65UART		cUART;
	CHCP65Terminal	cTerminal;
	CChars			szText;
	const char*		sz;

	cUART.Init(HUM_RS232);
	cTerminal.Init();
	cTerminal.Attach(&cUART);
	AssertTrue(cUART.GetHost() == &cTerminal);
	AssertInt(HCP65_UART_MSR_CTS | HCP65_UART_MSR_DSR | HCP65_UART_MSR_DCD | HCP65_UART_MSR_RI, cUART.Read(HCP65_UART_MSR) & 0xF0);

	//What the UART sends is queued until Update.
	AssertFalse(cTerminal.Update());
	cTerminal.Received(&cUART, 'Q');
	AssertInt(1, (int)cTerminal.NumLines());
	AssertString("", cTerminal.GetLine(0));
	AssertTrue(cTerminal.Update());
	AssertString("Q", cTerminal.GetLine(0));

	//Form feed clears; CR goes back over the line; tab to the next eighth column; backspace back one.
	for (sz = "\fHello\rJ\r\nW\tX\b\bZ\a"; *sz; sz++)
	{
		cTerminal.Put((uint8)*sz);
	}
	AssertInt(2, (int)cTerminal.NumLines());
	AssertString("Jello", cTerminal.GetLine(0));
	AssertString("W      ZX", cTerminal.GetLine(1));
	AssertInt(8, (int)cTerminal.GetColumn());
	szText.Init();
	cTerminal.GetText(&szText);
	AssertString("Jello\nW      ZX", szText.Text());
	szText.Kill();

	//Only the last 500 lines are kept.
	for (sz = "\f"; *sz; sz++)
	{
		cTerminal.Put((uint8)*sz);
	}
	for (int i = 0; i < 600; i++)
	{
		cTerminal.Put('0' + (i % 10));
		cTerminal.Put('\n');
	}
	AssertInt(HCP65_TERMINAL_MAX_LINES, (int)cTerminal.NumLines());
	AssertString("1", cTerminal.GetLine(0));

	cTerminal.Kill();
	AssertTrue(cUART.GetHost() == NULL);
	cUART.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The PS2 Board decodes A0-A4 (A3 and A4 low), repeating every 32 bytes.
// Flags, State and Direct, its interrupt from a port's packet, and its
// GPIO pulling a line low while the board is enabled.
//
//////////////////////////////////////////////////////////////////////////
void TestHCP65PS2Board(void)
{
	CHCP65PS2Board	cBoard;
	CHCP65PS2Port*	pcPort;

	cBoard.Init();
	AssertInt(0x1A, cBoard.Read(0));		//The device ROM's magic, $C0DAFE1A.
	AssertInt(0xFF, cBoard.Read(0x800));
	AssertInt(0xFF, cBoard.Read(0x405));
	AssertInt(0xFF, cBoard.Read(0x408));
	AssertInt(0xFF, cBoard.Read(0x410));

	//Enable can't be set while the board is held in reset.  D7 reads whether it is interrupting.
	cBoard.Write(0x400, 0xC5);
	AssertInt(0x05, cBoard.Read(0x400));
	cBoard.SetHeldInReset(false);
	cBoard.Write(0x400, 0xC5);
	AssertInt(0x45, cBoard.Read(0x400));
	AssertInt(0x45, cBoard.Read(0x7E0));
	AssertInt(0x33, cBoard.Read(0x404));
	AssertInt(0x0F, cBoard.Read(0x403));

	//Port 1's far end sends $1C: a packet waiting interrupts, and State shows it and the byte.
	pcPort = cBoard.GetPort(0);
	pcPort->HostSend((const uint8*)"\x1C", 1);
	pcPort->CatchUp(2490);
	AssertTrue(cBoard.IsInterrupting());
	AssertInt(0xC5, cBoard.Read(0x400));
	AssertInt(0x36, cBoard.Read(0x404));
	AssertInt(0x1C, cBoard.Read(0x421));
	AssertInt(0x32, cBoard.Read(0x404));
	cBoard.Write(0x402, 0);
	AssertTrue(cBoard.IsInterrupting());
	cBoard.Write(0x401, 0);
	AssertFalse(cBoard.IsInterrupting());
	AssertInt(0x33, cBoard.Read(0x404));

	//Without the interrupt enable it doesn't interrupt, though the packet is waiting.
	pcPort->HostSend((const uint8*)"\x1C", 1);
	cBoard.Write(0x400, 0x45);
	pcPort->CatchUp(pcPort->GetTick() + 3000);
	AssertInt(0x36, cBoard.Read(0x404));
	AssertFalse(cBoard.IsInterrupting());
	AssertInt(0x1C, cBoard.Read(0x401));
	cBoard.Write(0x401, 0);

	//The GPIO pulls port 1's Clock low (its output enabled, its level low), but only while the board is enabled.
	cBoard.Write(0x403, HCP65_PS2_DIRECT_CLOCK_1 << HCP65_PS2_DIRECT_DRIVE);
	AssertInt(0x0D, cBoard.Read(0x403));
	AssertFalse(pcPort->IsClockHigh());
	cBoard.Write(0x400, 0x05);
	AssertInt(0x0F, cBoard.Read(0x403));
	cBoard.Write(0x400, 0x45);
	AssertInt(0x0D, cBoard.Read(0x403));
	cBoard.Write(0x403, 0);
	AssertInt(0x0F, cBoard.Read(0x403));

	//Reset turns Enable off and empties the FIFOs.
	cBoard.SetHeldInReset(true);
	AssertInt(0x05, cBoard.Read(0x400));
	AssertInt(0x33, cBoard.Read(0x404));

	cBoard.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The PS2 Board in socket 3: port 1's far end sends $5A and the processor
// is interrupted when its packet ends, 630us after the frame.
//
//////////////////////////////////////////////////////////////////////////
void TestHCP65PS2Interrupt(void)
{
	CHCP65				cHCP65;
	CHCP65PS2Board		cPS2;
	CHCP65MemoryBoard	cMemory;
	uint8*				puiRAM;
	uint64				uiDue;

	static const uint8 auiIrq[] = {
		0xAF, 0x01, 0x84, 0x11,		//lda $118401 (port 1's FIFO)
		0x8F, 0x10, 0x00, 0x20,		//sta $200010
		0xAF, 0x04, 0x84, 0x11,		//lda $118404 (State)
		0x8F, 0x11, 0x00, 0x20,		//sta $200011
		0x8F, 0x01, 0x84, 0x11,		//sta $118401 (clears port 1's waiting)
		0xDB };						//stp

	cHCP65.Init(TEST_HCP65_CLOCK_RATE);
	TestHCP65AddMemory(&cHCP65, &cMemory);
	cPS2.Init();
	AssertTrue(cHCP65.SetDevice(3, &cPS2));
	puiRAM = TestHCP65WaitingKernal(&cHCP65, 0x8200, 0x8300);
	TestHCP65Put(puiRAM, 0x8200, auiIrq, sizeof(auiIrq));

	cHCP65.Reset();
	cHCP65.Write(0x100003, 0x08);
	AssertFalse(cPS2.IsHeldInReset());
	cHCP65.Write(0x118400, HCP65_PS2_FLAG_ENABLE | HCP65_PS2_FLAG_INTERRUPT);
	cPS2.GetPort(0)->HostSend((const uint8*)"\x5A", 1);

	cHCP65.Run(100000);
	AssertTrue(cHCP65.IsStopped());
	AssertInt(0x5A, *TestHCP65Main(&cHCP65, 0x200010));
	AssertInt(0x32, *TestHCP65Main(&cHCP65, 0x200011));
	uiDue = cHCP65.CycleOfDeviceTick(1000 + 860 + 630, HCP65_PS2_TICK_RATE);
	AssertTrue(cHCP65.NumCycles() >= uiDue);
	AssertTrue(cHCP65.NumCycles() < uiDue + 60);
	AssertFalse(cPS2.IsInterrupting());

	cHCP65.Kill();
	cPS2.Kill();
	cMemory.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The Video Board in socket 16 of an HCP65 at 25.175MHz, its pixel clock
// 25.175MHz too, so a cycle is a pixel clock.  A small mode: a line of 40
// clocks, the first 16 shown, and a frame of 7 lines, the first 4 shown.
// The interrupt at the start of vertical blank; the frame drawn from the
// row table, the indices, the palette numbers and the palettes; the
// pattern; the counters; the main bus windows with Palette Write and
// Memory Frame; and reset.
//
//////////////////////////////////////////////////////////////////////////
void TestHCP65VideoBoard(void)
{
	CHCP65				cHCP65;
	CHCP65VideoBoard	cVideo;
	uint32				auiFrame[16 * 4];
	uint32				uiWidth;
	uint32				uiHeight;
	uint8*				puiIndices;
	uint8*				puiPalettes;
	int					x;
	int					y;
	int					i;
	bool				bSame;

	cHCP65.Init(25175000);
	cVideo.Init();
	AssertFalse(cHCP65.SetDevice(3, &cVideo));
	AssertTrue(cHCP65.SetDevice(16, &cVideo));
	cVideo.SetHeldInReset(false);
	AssertInt(0x1A, cVideo.Read(0));		//The device ROM's magic, $C0DAFE1A.
	AssertInt(0xFF, cVideo.Read(0x800));
	AssertInt(0xFF, cVideo.Read(0x414));
	AssertLong(25175000, (int64)CHCP65VideoBoard::GetClockRate(2));

	//The timing, in 8 pixels and lines; the pattern advances every clock.
	cVideo.Write(0x400 + HCP65_VIDEO_H_FRONT_PORCH, 2);
	cVideo.Write(0x400 + HCP65_VIDEO_H_SYNC, 3);
	cVideo.Write(0x400 + HCP65_VIDEO_H_BACK_PORCH, 4);
	cVideo.Write(0x400 + HCP65_VIDEO_H_END, 5);
	cVideo.Write(0x400 + HCP65_VIDEO_V_FRONT_PORCH, 4);
	cVideo.Write(0x400 + HCP65_VIDEO_V_FRONT_PORCH + 1, 0);
	cVideo.Write(0x400 + HCP65_VIDEO_V_SYNC, 5);
	cVideo.Write(0x400 + HCP65_VIDEO_V_BACK_PORCH, 6);
	cVideo.Write(0x400 + HCP65_VIDEO_V_END, 7);
	cVideo.Write(0x420 + HCP65_VIDEO_PATTERN_LOW, 0x0F);
	cVideo.Write(0x7E0 + HCP65_VIDEO_PATTERN_HIGH, 0xFF);
	AssertInt(40, (int)cVideo.GetLineLength());
	AssertInt(16, (int)cVideo.GetActiveWidth());
	AssertInt(7, (int)cVideo.GetFrameLines());
	AssertInt(4, (int)cVideo.GetActiveLines());

	//Row r starts at 16r.  Frame 1 (shown while Memory Frame is 0) has index a & $FF at a; palette 0 is red = index,
	//green $10 and blue $20.
	for (y = 0; y < 4; y++)
	{
		cVideo.Write(HCP65_VIDEO_ROW_TABLE + 4 * y, (uint8)(16 * y));
		cVideo.Write(HCP65_VIDEO_ROW_TABLE + 4 * y + 1, 0);
		cVideo.Write(HCP65_VIDEO_ROW_TABLE + 4 * y + 2, 0);
	}
	AssertInt(48, cVideo.Read(HCP65_VIDEO_ROW_TABLE + 12));
	puiIndices = cVideo.GetChip(1, 0);
	for (i = 0; i < 64; i++)
	{
		puiIndices[i] = (uint8)i;
	}
	for (i = 0; i < 256; i++)
	{
		cVideo.Write(HCP65_VIDEO_RED + i, (uint8)i);
		cVideo.Write(HCP65_VIDEO_GREEN + i, 0x10);
		cVideo.Write(HCP65_VIDEO_BLUE + i, 0x20);
	}

	//The clock starts at cycle 0.  Vertical blank, line 4, starts at 4 x 40: the interrupt is set then.
	cVideo.Write(0x400 + HCP65_VIDEO_SIGNAL_DELAY, 0);
	cVideo.Write(0x400 + HCP65_VIDEO_FLAGS, HCP65_VIDEO_FLAG_ENABLE | HCP65_VIDEO_FLAG_INTERRUPT);
	cVideo.Write(0x400 + HCP65_VIDEO_CLOCK_SELECT, HCP65_VIDEO_CLOCK_ENABLE | 2);
	AssertLong(160, (int64)cVideo.NextEvent());
	cVideo.CatchUp(159);
	AssertFalse(cVideo.IsInterrupting());
	AssertInt(3, cVideo.Read(0x400 + HCP65_VIDEO_V_FRONT_PORCH));
	cVideo.CatchUp(160);
	AssertTrue(cVideo.IsInterrupting());
	AssertInt(0xC0, cVideo.Read(0x400 + HCP65_VIDEO_FLAGS));
	AssertInt(4, cVideo.Read(0x400 + HCP65_VIDEO_V_FRONT_PORCH));
	AssertLong((int64)HCP65_NEVER, (int64)cVideo.NextEvent());
	AssertFalse(cVideo.IsSignal());

	//The frame ends at 7 x 40.  Clear Interrupt: the next is a frame on.
	cVideo.CatchUp(280);
	AssertLong(1, (int64)cVideo.NumFrames());
	AssertTrue(cVideo.IsSignal());
	cVideo.Write(0x400 + HCP65_VIDEO_CLEAR_INTERRUPT, 0);
	AssertFalse(cVideo.IsInterrupting());
	AssertLong(440, (int64)cVideo.NextEvent());
	AssertTrue(cVideo.GetFrame(auiFrame, 16, 4, &uiWidth, &uiHeight));
	AssertInt(16, (int)uiWidth);
	AssertInt(4, (int)uiHeight);
	bSame = true;
	for (y = 0; y < 4; y++)
	{
		for (x = 0; x < 16; x++)
		{
			bSame &= auiFrame[y * 16 + x] == (0xFF001020 | ((uint32)(16 * y + x) << 16));
		}
	}
	AssertTrue(bSame);

	//Five clocks into the line: H counts from 1, and the address has moved on 5.
	cVideo.CatchUp(285);
	AssertInt(6, cVideo.Read(0x400 + HCP65_VIDEO_H_FRONT_PORCH));
	AssertInt(0, cVideo.Read(0x400 + HCP65_VIDEO_H_SYNC));
	AssertInt(5, cVideo.Read(0x400 + HCP65_VIDEO_ADDRESS_LOW));
	AssertInt(0, cVideo.Read(0x400 + HCP65_VIDEO_ADDRESS_HIGH));
	AssertInt(0, cVideo.Read(0x400 + HCP65_VIDEO_ADDRESS_BANK));

	//A pattern of every other clock shows each pixel twice.  Pixel 1's palette number is 1 (D7 is ignored), whose red for
	//index 1 is $77 and green and blue 0.
	cVideo.Write(0x400 + HCP65_VIDEO_PATTERN_LOW, 0x0A);
	cVideo.Write(0x400 + HCP65_VIDEO_PATTERN_HIGH, 0xAA);
	cVideo.GetChip(1, 1)[1] = 0x81;
	cVideo.Write(HCP65_VIDEO_RED + 0x101, 0x77);
	cVideo.CatchUp(560);
	AssertTrue(cVideo.GetFrame(auiFrame, 16, 4, &uiWidth, &uiHeight));
	AssertInt((int)0xFF001020, (int)auiFrame[0]);
	AssertInt((int)0xFF001020, (int)auiFrame[1]);
	AssertInt((int)0xFF770000, (int)auiFrame[2]);
	AssertInt((int)0xFF770000, (int)auiFrame[3]);
	AssertInt((int)0xFF021020, (int)auiFrame[4]);
	AssertInt((int)0xFF171020, (int)auiFrame[16 + 15]);

	//The main bus: socket Z (16) in window 1, so window A is $400000-$5FFFFF and window B $600000-$7FFFFF.  With Palette
	//Write set, a write to window A writes the Palette Byte into window B too.
	cHCP65.Write(0x108001, 0x40);
	cVideo.Write(0x400 + HCP65_VIDEO_PALETTE_BYTE, 5);
	cVideo.Write(0x400 + HCP65_VIDEO_FLAGS, HCP65_VIDEO_FLAG_ENABLE | HCP65_VIDEO_FLAG_PALETTE_WRITE);
	cHCP65.Write(0x400010, 0x33);
	AssertInt(0x33, cVideo.GetChip(0, 0)[0x10]);
	AssertInt(5, cVideo.GetChip(0, 1)[0x10]);
	cVideo.Write(0x400 + HCP65_VIDEO_FLAGS, HCP65_VIDEO_FLAG_ENABLE);
	cHCP65.Write(0x400011, 0x44);
	AssertInt(0x44, cVideo.GetChip(0, 0)[0x11]);
	AssertInt(0, cVideo.GetChip(0, 1)[0x11]);
	cHCP65.Write(0x600012, 9);
	AssertInt(9, cVideo.GetChip(0, 1)[0x12]);
	AssertInt(0x33, cHCP65.Read(0x400010));

	//Memory Frame: the processor has frame 1 and frame 0 is shown.
	cVideo.Write(0x400 + HCP65_VIDEO_FLAGS, HCP65_VIDEO_FLAG_ENABLE | HCP65_VIDEO_FLAG_MEMORY_FRAME);
	AssertInt(0x10, cHCP65.Read(0x400010));
	AssertInt(0x81, cHCP65.Read(0x600001));
	puiPalettes = cVideo.GetChip(1, 1);
	AssertInt(0x81, puiPalettes[1]);

	//Reset stops the clock and turns the output off.
	cVideo.Reset();
	AssertFalse(cVideo.IsSignal());
	AssertFalse(cVideo.GetFrame(auiFrame, 16, 4, &uiWidth, &uiHeight));
	AssertInt(0, (int)uiWidth);
	AssertLong((int64)HCP65_NEVER, (int64)cVideo.NextEvent());

	cHCP65.Kill();
	cVideo.Kill();
}

//////////////////////////////////////////////////////////////////////////
//
// uiValue written to the Blitter Board's 24 bit address whose low byte's
// register is uiRegister, or its 16 bit count's.
//
//////////////////////////////////////////////////////////////////////////
static void TestHCP65BlitterAddress(CHCP65BlitterBoard* pcBlitter, uint32 uiRegister, uint32 uiValue)
{
	pcBlitter->Write(HCP65_BLITTER_REGISTERS + uiRegister, (uint8)uiValue);
	pcBlitter->Write(HCP65_BLITTER_REGISTERS + uiRegister + 1, (uint8)(uiValue >> 8));
	pcBlitter->Write(HCP65_BLITTER_REGISTERS + uiRegister + 2, (uint8)(uiValue >> 16));
}


static void TestHCP65BlitterCount(CHCP65BlitterBoard* pcBlitter, uint16 uiStart, uint16 uiStop)
{
	pcBlitter->Write(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_COUNT_START_LOW, (uint8)uiStart);
	pcBlitter->Write(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_COUNT_START_HIGH, (uint8)(uiStart >> 8));
	pcBlitter->Write(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_COUNT_STOP_LOW, (uint8)uiStop);
	pcBlitter->Write(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_COUNT_STOP_HIGH, (uint8)(uiStop >> 8));
}


static uint16 TestHCP65BlitterReadCount(CHCP65BlitterBoard* pcBlitter)
{
	uint16	uiCount;

	uiCount = pcBlitter->Read(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_COUNT_START_LOW);
	return (uint16)(uiCount | (pcBlitter->Read(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_COUNT_START_HIGH) << 8));
}


//////////////////////////////////////////////////////////////////////////
//
// The Blitter Board on its own: a copy of stop - start bytes from the
// source address to the destination, two cycles a byte, which leaves the
// addresses past the last byte; the transparent colour; the interrupt when
// a copy is done; the board having main memory while it copies; and what a
// reset clears.  The processor sits in a loop in MPU RAM throughout.
//
//////////////////////////////////////////////////////////////////////////
void TestHCP65BlitterBoard(void)
{
	CHCP65				cHCP65;
	CHCP65MemoryBoard	cMemory;
	CHCP65BlitterBoard	cBlitter;
	uint8*				puiSource;
	uint8*				puiDest;
	uint16				uiCount;
	int					i;
	bool				bSame;

	static const uint8 auiKernal[] = {
		0x18, 0xFB,					//8000 clc  xce
		0x80, 0xFE };				//8002 bra $8002

	cHCP65.Init(TEST_HCP65_CLOCK_RATE);
	TestHCP65AddMemory(&cHCP65, &cMemory);
	cBlitter.Init();
	AssertTrue(cHCP65.SetDevice(3, &cBlitter));
	TestHCP65Program(&cHCP65, auiKernal, sizeof(auiKernal));
	cHCP65.Reset();
	cHCP65.Run(100);

	AssertInt(0x1A, cBlitter.Read(0));		//The device ROM's magic, $C0DAFE1A.
	AssertInt(0xFF, cBlitter.Read(0x800));
	AssertInt(0xFF, cBlitter.Read(HCP65_BLITTER_REGISTERS + 0x10));
	AssertInt(0xFF, cBlitter.Read(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_START));

	//Held in reset the board can't be enabled, and Start does nothing.
	cBlitter.Write(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_FLAGS, HCP65_BLITTER_FLAG_ENABLE | 0x22);
	AssertInt(0x22, cBlitter.Read(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_FLAGS));
	TestHCP65BlitterCount(&cBlitter, 0, 4);
	cBlitter.Write(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_START, 0);
	AssertFalse(cBlitter.IsCopying());
	cBlitter.SetHeldInReset(false);
	AssertInt(0x22, cBlitter.Read(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_FLAGS));

	//The registers repeat every 32 bytes.
	cBlitter.Write(HCP65_BLITTER_REGISTERS + 0x20 + HCP65_BLITTER_FLAGS, HCP65_BLITTER_FLAG_ENABLE);
	AssertInt(HCP65_BLITTER_FLAG_ENABLE, cBlitter.Read(0x7E0 + HCP65_BLITTER_FLAGS));
	cBlitter.Write(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_TRANSPARENT_COLOUR, 0x77);
	AssertInt(0x77, cBlitter.Read(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_TRANSPARENT_COLOUR));

	//Sixteen bytes from $201000 to $21FFF8, across a bank's end.  A byte is two cycles.
	puiSource = TestHCP65Main(&cHCP65, 0x201000);
	puiDest = TestHCP65Main(&cHCP65, 0x21FFF8);
	for (i = 0; i < 32; i++)
	{
		puiSource[i] = (uint8)(0x40 + i);
	}
	memset(puiDest, 0xEE, 8);
	memset(TestHCP65Main(&cHCP65, 0x220000), 0xEE, 24);
	TestHCP65BlitterAddress(&cBlitter, HCP65_BLITTER_SOURCE_LOW, 0x201000);
	TestHCP65BlitterAddress(&cBlitter, HCP65_BLITTER_DEST_LOW, 0x21FFF8);
	AssertInt(0x201000, cBlitter.GetSource());
	AssertInt(0x21FFF8, cBlitter.GetDest());
	TestHCP65BlitterCount(&cBlitter, 0, 16);
	cBlitter.Write(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_START, 0);
	AssertTrue(cBlitter.IsCopying());
	AssertLong((int64)(cHCP65.NumCycles() + 32), (int64)cBlitter.NextEvent());
	AssertInt(0xEE, puiDest[0]);
	cHCP65.Run(100);
	AssertFalse(cBlitter.IsCopying());
	AssertFalse(cHCP65.IsContention());
	AssertInt(0, memcmp(puiDest, puiSource, 8));
	AssertInt(0, memcmp(TestHCP65Main(&cHCP65, 0x220000), &puiSource[8], 8));
	AssertInt(0xEE, *TestHCP65Main(&cHCP65, 0x220008));
	AssertInt(0x201010, cBlitter.GetSource());
	AssertInt(0x220008, cBlitter.GetDest());
	AssertInt(16, TestHCP65BlitterReadCount(&cBlitter));
	AssertLong(16, (int64)cBlitter.NumBytesCopied());
	AssertFalse(cBlitter.IsInterrupting());
	AssertLong((int64)HCP65_NEVER, (int64)cBlitter.NextEvent());

	//Start again without the addresses written: the next sixteen bytes follow.  The count starts where it is told to
	//(here 65530, stopping at 10 through 0).
	TestHCP65BlitterCount(&cBlitter, 65530, 10);
	cBlitter.Write(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_START, 0);
	cHCP65.Run(100);
	AssertInt(0, memcmp(TestHCP65Main(&cHCP65, 0x220008), &puiSource[16], 16));
	AssertInt(0x220018, cBlitter.GetDest());
	AssertInt(10, cBlitter.GetCount());
	AssertLong(32, (int64)cBlitter.NumBytesCopied());

	//A start that is the stop copies nothing, and is done at once.
	TestHCP65BlitterCount(&cBlitter, 7, 7);
	cBlitter.Write(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_START, 0);
	AssertFalse(cBlitter.IsCopying());
	AssertInt(7, cBlitter.GetCount());
	AssertLong(32, (int64)cBlitter.NumBytesCopied());

	//Part way through, the count read is how far it has got (its high byte kept as the low one is read), the addresses
	//can't be written, and disabling the board stops the copy there.
	puiDest = TestHCP65Main(&cHCP65, 0x230000);
	memset(puiDest, 0xEE, 1000);
	TestHCP65BlitterAddress(&cBlitter, HCP65_BLITTER_SOURCE_LOW, 0x201000);
	TestHCP65BlitterAddress(&cBlitter, HCP65_BLITTER_DEST_LOW, 0x230000);
	TestHCP65BlitterCount(&cBlitter, 0x100, 0x100 + 1000);
	cBlitter.Write(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_START, 0);
	cHCP65.Run(700);
	AssertTrue(cBlitter.IsCopying());
	uiCount = TestHCP65BlitterReadCount(&cBlitter);
	AssertTrue((uiCount >= 0x100 + 340) && (uiCount <= 0x100 + 360));
	AssertInt(0x230000 + (uiCount - 0x100), cBlitter.GetDest());
	AssertInt(TestHCP65Main(&cHCP65, 0x201000)[uiCount - 0x101], puiDest[uiCount - 0x101]);
	AssertInt(0xEE, puiDest[uiCount - 0x100]);
	TestHCP65BlitterAddress(&cBlitter, HCP65_BLITTER_DEST_LOW, 0x123456);
	AssertInt(0x230000 + (uiCount - 0x100), cBlitter.GetDest());
	cBlitter.Write(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_FLAGS, 0);
	AssertFalse(cBlitter.IsCopying());
	cHCP65.Run(2000);
	AssertInt(uiCount, cBlitter.GetCount());
	AssertInt(0xEE, puiDest[uiCount - 0x100]);

	//Transparency: with Flags D0, the bytes that are the transparent colour aren't written.  Without it they are.
	puiSource = TestHCP65Main(&cHCP65, 0x240000);
	puiDest = TestHCP65Main(&cHCP65, 0x250000);
	for (i = 0; i < 8; i++)
	{
		puiSource[i] = (uint8)((i & 1) ? 0x77 : (0x10 + i));
	}
	memset(puiDest, 0xEE, 16);
	cBlitter.Write(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_FLAGS, HCP65_BLITTER_FLAG_ENABLE | HCP65_BLITTER_FLAG_TRANSPARENCY);
	TestHCP65BlitterAddress(&cBlitter, HCP65_BLITTER_SOURCE_LOW, 0x240000);
	TestHCP65BlitterAddress(&cBlitter, HCP65_BLITTER_DEST_LOW, 0x250000);
	TestHCP65BlitterCount(&cBlitter, 0, 8);
	cBlitter.Write(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_START, 0);
	cHCP65.Run(100);
	bSame = true;
	for (i = 0; i < 8; i++)
	{
		bSame &= puiDest[i] == ((i & 1) ? 0xEE : (0x10 + i));
	}
	AssertTrue(bSame);
	AssertInt(0x250008, cBlitter.GetDest());
	cBlitter.Write(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_FLAGS, HCP65_BLITTER_FLAG_ENABLE);
	TestHCP65BlitterAddress(&cBlitter, HCP65_BLITTER_SOURCE_LOW, 0x240000);
	cBlitter.Write(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_START, 0);
	cHCP65.Run(100);
	AssertInt(0, memcmp(&puiDest[8], puiSource, 8));

	//The bytes go in order, so a destination one past the source repeats its first byte.
	TestHCP65BlitterAddress(&cBlitter, HCP65_BLITTER_SOURCE_LOW, 0x250008);
	TestHCP65BlitterAddress(&cBlitter, HCP65_BLITTER_DEST_LOW, 0x250009);
	TestHCP65BlitterCount(&cBlitter, 0, 6);
	cBlitter.Write(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_START, 0);
	cHCP65.Run(100);
	bSame = true;
	for (i = 8; i < 15; i++)
	{
		bSame &= puiDest[i] == 0x10;
	}
	AssertTrue(bSame);
	AssertInt(0x77, puiDest[15]);

	//The interrupt, when it is enabled: latched when a copy is done, until Clear Interrupt, the next Start or the
	//interrupt being disabled.
	cBlitter.Write(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_FLAGS, HCP65_BLITTER_FLAG_ENABLE | HCP65_BLITTER_FLAG_INTERRUPT);
	AssertInt(HCP65_BLITTER_FLAG_ENABLE, cBlitter.Read(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_FLAGS));
	TestHCP65BlitterCount(&cBlitter, 0, 4);
	cBlitter.Write(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_START, 0);
	AssertFalse(cBlitter.IsInterrupting());
	cHCP65.Run(100);
	AssertTrue(cBlitter.IsInterrupting());
	AssertInt(HCP65_BLITTER_FLAG_ENABLE | HCP65_BLITTER_FLAG_INTERRUPT, cBlitter.Read(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_FLAGS));
	cBlitter.Write(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_CLEAR_INTERRUPT, 0);
	AssertFalse(cBlitter.IsInterrupting());
	cBlitter.Write(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_START, 0);
	cHCP65.Run(100);
	AssertTrue(cBlitter.IsInterrupting());
	cBlitter.Write(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_START, 0);
	AssertFalse(cBlitter.IsInterrupting());
	cHCP65.Run(100);
	AssertTrue(cBlitter.IsInterrupting());
	cBlitter.Write(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_FLAGS, HCP65_BLITTER_FLAG_ENABLE);
	AssertFalse(cBlitter.IsInterrupting());

	//A reset clears the flags, the addresses and the count, not the count's start and stop or the transparent colour.
	cBlitter.Reset();
	AssertInt(0, cBlitter.Read(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_FLAGS));
	AssertInt(0, cBlitter.GetSource());
	AssertInt(0, cBlitter.GetDest());
	AssertInt(0, TestHCP65BlitterReadCount(&cBlitter));
	AssertInt(0x77, cBlitter.Read(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_TRANSPARENT_COLOUR));
	puiDest = TestHCP65Main(&cHCP65, 0x260000);
	memset(puiDest, 0xEE, 8);
	cBlitter.Write(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_FLAGS, HCP65_BLITTER_FLAG_ENABLE);
	TestHCP65BlitterAddress(&cBlitter, HCP65_BLITTER_SOURCE_LOW, 0x240000);
	TestHCP65BlitterAddress(&cBlitter, HCP65_BLITTER_DEST_LOW, 0x260000);
	cBlitter.Write(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_START, 0);
	cHCP65.Run(100);
	AssertInt(0, memcmp(puiDest, puiSource, 4));
	AssertInt(0xEE, puiDest[4]);

	//While it copies the board has main memory: the processor reading it is contention, which shuts main memory off, so
	//the rest of the copy is lost.
	memset(puiDest, 0xEE, 8);
	TestHCP65BlitterAddress(&cBlitter, HCP65_BLITTER_SOURCE_LOW, 0x240000);
	TestHCP65BlitterAddress(&cBlitter, HCP65_BLITTER_DEST_LOW, 0x260000);
	cBlitter.Write(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_START, 0);
	AssertFalse(cHCP65.IsContention());
	AssertInt(0xFF, cHCP65.Read(0x260000));
	AssertTrue(cHCP65.IsContention());
	cHCP65.Run(100);
	AssertInt(0xEE, puiDest[0]);

	cHCP65.Kill();
	cBlitter.Kill();
	cMemory.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The Graphics Board on its own: a Blitter Board whose bytes are read from
// the source address + the offset its table has for the count.  The
// offsets are device RAM, the processor's while no copy is under way; the
// source address isn't changed by a copy or a reset and can be read; the
// count is 15 bits.
//
//////////////////////////////////////////////////////////////////////////
void TestHCP65GraphicsBoard(void)
{
	CHCP65				cHCP65;
	CHCP65MemoryBoard	cMemory;
	CHCP65GraphicsBoard	cGraphics;
	uint8*				puiSource;
	uint8*				puiDest;
	int					i;

	static const uint8 auiKernal[] = {
		0x18, 0xFB,					//8000 clc  xce
		0x80, 0xFE };				//8002 bra $8002

	cHCP65.Init(TEST_HCP65_CLOCK_RATE);
	TestHCP65AddMemory(&cHCP65, &cMemory);
	cGraphics.Init();
	AssertTrue(cHCP65.SetDevice(16, &cGraphics));
	TestHCP65Program(&cHCP65, auiKernal, sizeof(auiKernal));
	cHCP65.Reset();
	cHCP65.Run(100);
	cGraphics.SetHeldInReset(false);

	AssertInt(0x1A, cGraphics.Read(0));		//The device ROM's magic, $C0DAFE1A.
	AssertInt(0xFF, cGraphics.Read(0x800));
	AssertInt(0xFF, cGraphics.Read(HCP65_GRAPHICS_OFFSETS_LOW - 1));
	AssertInt(0xFF, cGraphics.Read(HCP65_GRAPHICS_OFFSETS_HIGH + HCP65_GRAPHICS_OFFSETS));

	//The offsets: low bytes at $08000, high bytes at $10000.
	AssertInt(0, cGraphics.Read(HCP65_GRAPHICS_OFFSETS_LOW + 5));
	cGraphics.Write(HCP65_GRAPHICS_OFFSETS_LOW + 5, 0x34);
	cGraphics.Write(HCP65_GRAPHICS_OFFSETS_HIGH + 5, 0x12);
	AssertInt(0x34, cGraphics.Read(HCP65_GRAPHICS_OFFSETS_LOW + 5));
	AssertInt(0x12, cGraphics.Read(HCP65_GRAPHICS_OFFSETS_HIGH + 5));
	AssertInt(0x1234, cGraphics.GetOffset(5));
	cGraphics.Write(HCP65_GRAPHICS_OFFSETS_LOW + 0x7FFF, 0xCD);
	cGraphics.Write(HCP65_GRAPHICS_OFFSETS_HIGH + 0x7FFF, 0xAB);
	AssertInt(0xABCD, cGraphics.GetOffset(0x7FFF));
	AssertInt(0, cGraphics.GetOffset(0));

	//The source address can be read.
	TestHCP65BlitterAddress(&cGraphics, HCP65_BLITTER_SOURCE_LOW, 0x201000);
	AssertInt(0x00, cGraphics.Read(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_SOURCE_LOW));
	AssertInt(0x10, cGraphics.Read(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_SOURCE_HIGH));
	AssertInt(0x20, cGraphics.Read(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_SOURCE_BANK));

	//Eight bytes backwards: offsets 7 down to 0 at counts 16 to 23.  The source address stays; the destination steps.
	puiSource = TestHCP65Main(&cHCP65, 0x201000);
	puiDest = TestHCP65Main(&cHCP65, 0x230000);
	for (i = 0; i < 16; i++)
	{
		puiSource[i] = (uint8)(0x40 + i);
		cGraphics.SetOffset((uint16)(16 + i), (uint16)(7 - i));
	}
	memset(puiDest, 0xEE, 16);
	cGraphics.Write(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_FLAGS, HCP65_BLITTER_FLAG_ENABLE);
	TestHCP65BlitterAddress(&cGraphics, HCP65_BLITTER_DEST_LOW, 0x230000);
	TestHCP65BlitterCount(&cGraphics, 16, 24);
	cGraphics.Write(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_START, 0);
	AssertTrue(cGraphics.IsCopying());
	AssertLong((int64)(cHCP65.NumCycles() + 16), (int64)cGraphics.NextEvent());

	//While it copies the offsets are the board's, and the source address still takes a write.
	AssertInt(0xFF, cGraphics.Read(HCP65_GRAPHICS_OFFSETS_LOW + 5));
	cGraphics.Write(HCP65_GRAPHICS_OFFSETS_LOW + 5, 0x99);
	cHCP65.Run(100);
	AssertFalse(cGraphics.IsCopying());
	AssertFalse(cHCP65.IsContention());
	AssertInt(0x34, cGraphics.Read(HCP65_GRAPHICS_OFFSETS_LOW + 5));
	for (i = 0; i < 8; i++)
	{
		AssertInt(0x47 - i, puiDest[i]);
	}
	AssertInt(0xEE, puiDest[8]);
	AssertInt(0x201000, cGraphics.GetSource());
	AssertInt(0x230008, cGraphics.GetDest());
	AssertInt(24, TestHCP65BlitterReadCount(&cGraphics));
	AssertLong(8, (int64)cGraphics.NumBytesCopied());

	//Stretched to twice as wide, without the transparent colour: offsets 0, 0, 1, 1...  The offset carries into the
	//bank: the source address is 4 below a bank's end.
	puiSource = TestHCP65Main(&cHCP65, 0x24FFFC);
	puiDest = TestHCP65Main(&cHCP65, 0x260000);
	for (i = 0; i < 8; i++)
	{
		puiSource[i] = (uint8)((i == 2) ? 0x77 : (0x10 + i));
		cGraphics.SetOffset((uint16)(2 * i), (uint16)i);
		cGraphics.SetOffset((uint16)(2 * i + 1), (uint16)i);
	}
	memset(puiDest, 0xEE, 32);
	cGraphics.Write(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_TRANSPARENT_COLOUR, 0x77);
	cGraphics.Write(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_FLAGS, HCP65_BLITTER_FLAG_ENABLE | HCP65_BLITTER_FLAG_TRANSPARENCY);
	TestHCP65BlitterAddress(&cGraphics, HCP65_BLITTER_SOURCE_LOW, 0x24FFFC);
	TestHCP65BlitterAddress(&cGraphics, HCP65_BLITTER_DEST_LOW, 0x260000);
	TestHCP65BlitterCount(&cGraphics, 0, 16);
	cGraphics.Write(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_START, 0);
	cHCP65.Run(100);
	for (i = 0; i < 16; i++)
	{
		AssertInt(((i / 2) == 2) ? 0xEE : (0x10 + i / 2), puiDest[i]);
	}
	AssertInt(0xEE, puiDest[16]);

	//An offset is 16 bits and never negative: $FFFF is 65535 past the source address.
	*TestHCP65Main(&cHCP65, 0x24FFFC + 0xFFFF) = 0x5A;
	cGraphics.SetOffset(100, 0xFFFF);
	cGraphics.Write(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_FLAGS, HCP65_BLITTER_FLAG_ENABLE);
	TestHCP65BlitterAddress(&cGraphics, HCP65_BLITTER_DEST_LOW, 0x260000);
	TestHCP65BlitterCount(&cGraphics, 100, 101);
	cGraphics.Write(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_START, 0);
	cHCP65.Run(100);
	AssertInt(0x5A, puiDest[0]);

	//The count is 15 bits: bit 15 of its start and stop is ignored, and it wraps from 32767 to 0.
	puiSource = TestHCP65Main(&cHCP65, 0x201000);
	cGraphics.SetOffset(32766, 3);
	cGraphics.SetOffset(32767, 2);
	cGraphics.SetOffset(0, 1);
	cGraphics.SetOffset(1, 0);
	memset(puiDest, 0xEE, 8);
	TestHCP65BlitterAddress(&cGraphics, HCP65_BLITTER_SOURCE_LOW, 0x201000);
	TestHCP65BlitterAddress(&cGraphics, HCP65_BLITTER_DEST_LOW, 0x260000);
	TestHCP65BlitterCount(&cGraphics, 0xFFFE, 0x8002);
	cGraphics.Write(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_START, 0);
	AssertLong((int64)(cHCP65.NumCycles() + 8), (int64)cGraphics.NextEvent());
	cHCP65.Run(100);
	AssertFalse(cGraphics.IsCopying());
	AssertInt(0x43, puiDest[0]);
	AssertInt(0x42, puiDest[1]);
	AssertInt(0x41, puiDest[2]);
	AssertInt(0x40, puiDest[3]);
	AssertInt(0xEE, puiDest[4]);
	AssertInt(2, TestHCP65BlitterReadCount(&cGraphics));

	//The interrupt when a copy is done, as the Blitter Board's.
	cGraphics.Write(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_FLAGS, HCP65_BLITTER_FLAG_ENABLE | HCP65_BLITTER_FLAG_INTERRUPT);
	TestHCP65BlitterCount(&cGraphics, 0, 4);
	cGraphics.Write(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_START, 0);
	AssertFalse(cGraphics.IsInterrupting());
	cHCP65.Run(100);
	AssertTrue(cGraphics.IsInterrupting());
	cGraphics.Write(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_CLEAR_INTERRUPT, 0);
	AssertFalse(cGraphics.IsInterrupting());

	//A reset clears the flags, the destination address and the count, not the source address or the offsets.
	cGraphics.Reset();
	AssertInt(0, cGraphics.Read(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_FLAGS));
	AssertInt(0x201000, cGraphics.GetSource());
	AssertInt(0, cGraphics.GetDest());
	AssertInt(0, TestHCP65BlitterReadCount(&cGraphics));
	AssertInt(0xFFFF, cGraphics.GetOffset(100));

	cHCP65.Kill();
	cGraphics.Kill();
	cMemory.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The LCD's busy flag, read as a driver would: R/~W high with E low, then
// E high.  E is left low.
//
//////////////////////////////////////////////////////////////////////////
static uint8 TestHCP65LCDStatus(CHCP65LCDBoard* pcLCD)
{
	uint8	uiStatus;

	pcLCD->Read(HCP65_LCD_REGISTERS + HCP65_LCD_STATUS);
	uiStatus = pcLCD->Read(HCP65_LCD_REGISTERS + HCP65_LCD_STATUS_E);
	pcLCD->Read(HCP65_LCD_REGISTERS + HCP65_LCD_STATUS);
	return uiStatus;
}


//////////////////////////////////////////////////////////////////////////
//
// Runs until the LCD isn't busy.
//
//////////////////////////////////////////////////////////////////////////
static void TestHCP65LCDWait(CHCP65* pcHCP65, CHCP65LCDBoard* pcLCD)
{
	int		i;

	for (i = 0; i < 10000; i++)
	{
		if (!(TestHCP65LCDStatus(pcLCD) & 0x80))
		{
			return;
		}
		pcHCP65->Run(50);
	}
	AssertTrue(false);
}


//////////////////////////////////////////////////////////////////////////
//
// A byte to the LCD's instruction register or as data: written with E
// high, and E dropped with a read so the latched byte doesn't change as
// it falls.  Then waits until the LCD isn't busy.
//
//////////////////////////////////////////////////////////////////////////
static void TestHCP65LCDSend(CHCP65* pcHCP65, CHCP65LCDBoard* pcLCD, bool bData, uint8 uiValue)
{
	pcLCD->Write(HCP65_LCD_REGISTERS + (bData ? HCP65_LCD_DATA_E : HCP65_LCD_COMMAND_E), uiValue);
	pcLCD->Read(HCP65_LCD_REGISTERS + (bData ? HCP65_LCD_DATA : HCP65_LCD_COMMAND));
	TestHCP65LCDWait(pcHCP65, pcLCD);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
static void TestHCP65LCDSendText(CHCP65* pcHCP65, CHCP65LCDBoard* pcLCD, const char* szText)
{
	while (*szText)
	{
		TestHCP65LCDSend(pcHCP65, pcLCD, true, (uint8)*szText);
		szText++;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The LCD and SNES Board as a driver would use it: the display's pins
// set by the registers' addresses, its busy flag polled, its
// instructions, text on both lines, a character of its own, the cursor,
// a shift, reading back (through the data register) and four bit mode; the
// SNES controllers' latches.
//
//////////////////////////////////////////////////////////////////////////
void TestHCP65LCDBoard(void)
{
	CHCP65				cHCP65;
	CHCP65MemoryBoard	cMemory;
	CHCP65LCDBoard		cLCD;
	SHCP65LCDPicture	sPicture;
	uint64				uiCycle;
	int					i;

	static const uint8 auiKernal[] = {
		0x18, 0xFB,					//8000 clc  xce
		0x80, 0xFE };				//8002 bra $8002

	static const uint8 auiH[HCP65_LCD_CHARACTER_HEIGHT] = { 0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11, 0x00 };
	static const uint8 auiW[HCP65_LCD_CHARACTER_HEIGHT] = { 0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A, 0x00 };
	static const uint8 auiOwn[HCP65_LCD_CHARACTER_HEIGHT] = { 0x1F, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x1F, 0x15 };

	cHCP65.Init(TEST_HCP65_CLOCK_RATE);
	TestHCP65AddMemory(&cHCP65, &cMemory);
	cLCD.Init();
	AssertTrue(cHCP65.SetDevice(3, &cLCD));
	TestHCP65Program(&cHCP65, auiKernal, sizeof(auiKernal));
	cHCP65.Reset();
	cHCP65.Run(100);
	cLCD.SetHeldInReset(false);

	AssertInt(0x1A, cLCD.Read(0));		//The device ROM's magic, $C0DAFE1A.
	AssertInt(0xFF, cLCD.Read(0x800));
	AssertInt(0xFF, cLCD.Read(HCP65_LCD_REGISTERS + 0x0C));
	AssertInt(0xFF, cLCD.Read(HCP65_LCD_REGISTERS + 0x0F));

	//Busy with its own reset for 10ms after power on, then off, one line and blank.
	AssertTrue(TestHCP65LCDStatus(&cLCD) & 0x80);
	cHCP65.Run(TEST_HCP65_CLOCK_RATE / 50);
	AssertInt(0x00, TestHCP65LCDStatus(&cLCD));
	cLCD.GetPicture(&sPicture, cHCP65.NumCycles());
	AssertFalse(sPicture.bOn);

	//Eight bit, two lines; on; cleared, which is busy for 1.52ms; incrementing.
	TestHCP65LCDSend(&cHCP65, &cLCD, false, 0x38);
	TestHCP65LCDSend(&cHCP65, &cLCD, false, 0x0C);
	uiCycle = cHCP65.NumCycles();
	cLCD.Write(HCP65_LCD_REGISTERS + HCP65_LCD_COMMAND_E, 0x01);
	cLCD.Read(HCP65_LCD_REGISTERS + HCP65_LCD_COMMAND);
	TestHCP65LCDWait(&cHCP65, &cLCD);
	AssertTrue(cHCP65.NumCycles() - uiCycle >= (uint64)TEST_HCP65_CLOCK_RATE * 153 / 100000);
	TestHCP65LCDSend(&cHCP65, &cLCD, false, 0x06);

	//Text on both lines.
	TestHCP65LCDSendText(&cHCP65, &cLCD, "HELLO");
	AssertInt(0x05, TestHCP65LCDStatus(&cLCD));
	TestHCP65LCDSend(&cHCP65, &cLCD, false, 0xC0);
	TestHCP65LCDSendText(&cHCP65, &cLCD, "WORLD");
	AssertInt(0x45, TestHCP65LCDStatus(&cLCD));
	AssertInt('H', cLCD.GetDDRAM(0x00));
	AssertInt('O', cLCD.GetDDRAM(0x04));
	AssertInt('W', cLCD.GetDDRAM(0x40));
	AssertInt(' ', cLCD.GetDDRAM(0x05));
	cLCD.GetPicture(&sPicture, cHCP65.NumCycles());
	AssertTrue(sPicture.bOn);
	AssertMemory(auiH, sPicture.aauiDots[0][0], HCP65_LCD_CHARACTER_HEIGHT);
	AssertMemory(auiW, sPicture.aauiDots[1][0], HCP65_LCD_CHARACTER_HEIGHT);
	for (i = 0; i < HCP65_LCD_CHARACTER_HEIGHT; i++)
	{
		AssertInt(0, sPicture.aauiDots[0][5][i]);
	}

	//Given something while busy it drops it.
	AssertLong(0, (int64)cLCD.NumIgnored());
	cLCD.Write(HCP65_LCD_REGISTERS + HCP65_LCD_DATA_E, '!');
	cLCD.Read(HCP65_LCD_REGISTERS + HCP65_LCD_DATA);
	cLCD.Write(HCP65_LCD_REGISTERS + HCP65_LCD_DATA_E, '?');
	cLCD.Read(HCP65_LCD_REGISTERS + HCP65_LCD_DATA);
	AssertLong(1, (int64)cLCD.NumIgnored());
	TestHCP65LCDWait(&cHCP65, &cLCD);
	AssertInt('!', cLCD.GetDDRAM(0x45));
	AssertInt(' ', cLCD.GetDDRAM(0x46));

	//Dropping E with a write: the display takes the byte latched before it, not the one written as E falls.
	cLCD.Write(HCP65_LCD_REGISTERS + HCP65_LCD_DATA_E, 'A');
	cLCD.Write(HCP65_LCD_REGISTERS + HCP65_LCD_DATA, 'B');
	TestHCP65LCDWait(&cHCP65, &cLCD);
	AssertInt('A', cLCD.GetDDRAM(0x46));

	//With R/~W low a read is the latched byte; with it high and E low nothing drives the bus.
	AssertInt('B', cLCD.Read(HCP65_LCD_REGISTERS + HCP65_LCD_COMMAND));
	AssertInt(0xFF, cLCD.Read(HCP65_LCD_REGISTERS + HCP65_LCD_READ_DATA));

	//Reading DDRAM back: E high reads the byte, E falling moves the address counter.
	TestHCP65LCDSend(&cHCP65, &cLCD, false, 0x81);
	cLCD.Read(HCP65_LCD_REGISTERS + HCP65_LCD_READ_DATA);
	AssertInt('E', cLCD.Read(HCP65_LCD_REGISTERS + HCP65_LCD_READ_DATA_E));
	AssertInt('E', cLCD.Read(HCP65_LCD_REGISTERS + HCP65_LCD_READ_DATA_E));
	cLCD.Read(HCP65_LCD_REGISTERS + HCP65_LCD_READ_DATA);
	TestHCP65LCDWait(&cHCP65, &cLCD);
	AssertInt(0x02, TestHCP65LCDStatus(&cLCD));

	//The registers repeat every 16 bytes.
	AssertInt(0x02, cLCD.Read(HCP65_LCD_REGISTERS + 0x10 + HCP65_LCD_STATUS_E));
	cLCD.Read(HCP65_LCD_REGISTERS + 0x3F0 + HCP65_LCD_STATUS);

	//A read gives the data register, which setting the address and each read load but a write doesn't: straight after
	//a write it gives what was there before.
	TestHCP65LCDSend(&cHCP65, &cLCD, false, 0x80);
	TestHCP65LCDSend(&cHCP65, &cLCD, true, 'Q');
	AssertInt('Q', cLCD.GetDDRAM(0x00));
	cLCD.Read(HCP65_LCD_REGISTERS + HCP65_LCD_READ_DATA);
	AssertInt('H', cLCD.Read(HCP65_LCD_REGISTERS + HCP65_LCD_READ_DATA_E));
	cLCD.Read(HCP65_LCD_REGISTERS + HCP65_LCD_READ_DATA);
	TestHCP65LCDWait(&cHCP65, &cLCD);
	cLCD.Read(HCP65_LCD_REGISTERS + HCP65_LCD_READ_DATA);
	AssertInt('L', cLCD.Read(HCP65_LCD_REGISTERS + HCP65_LCD_READ_DATA_E));
	cLCD.Read(HCP65_LCD_REGISTERS + HCP65_LCD_READ_DATA);
	TestHCP65LCDWait(&cHCP65, &cLCD);
	AssertInt(0x03, TestHCP65LCDStatus(&cLCD));
	TestHCP65LCDSend(&cHCP65, &cLCD, false, 0x80);
	TestHCP65LCDSend(&cHCP65, &cLCD, true, 'H');

	//A character of its own: CGRAM character 0, shown by codes $00 and $08.
	TestHCP65LCDSend(&cHCP65, &cLCD, false, 0x40);
	for (i = 0; i < HCP65_LCD_CHARACTER_HEIGHT; i++)
	{
		TestHCP65LCDSend(&cHCP65, &cLCD, true, auiOwn[i]);
	}
	TestHCP65LCDSend(&cHCP65, &cLCD, false, 0x8E);
	TestHCP65LCDSend(&cHCP65, &cLCD, true, 0x00);
	TestHCP65LCDSend(&cHCP65, &cLCD, true, 0x08);
	cLCD.GetPicture(&sPicture, cHCP65.NumCycles());
	AssertMemory(auiOwn, sPicture.aauiDots[0][14], HCP65_LCD_CHARACTER_HEIGHT);
	AssertMemory(auiOwn, sPicture.aauiDots[0][15], HCP65_LCD_CHARACTER_HEIGHT);

	//The cursor is the last row of the character at the address counter; blinking makes it all dots half the time.
	TestHCP65LCDSend(&cHCP65, &cLCD, false, 0x85);
	TestHCP65LCDSend(&cHCP65, &cLCD, false, 0x0E);
	cLCD.GetPicture(&sPicture, cHCP65.NumCycles());
	AssertInt(0x1F, sPicture.aauiDots[0][5][7]);
	AssertInt(0x00, sPicture.aauiDots[0][5][6]);
	AssertInt(0x00, sPicture.aauiDots[0][4][7]);
	TestHCP65LCDSend(&cHCP65, &cLCD, false, 0x0F);
	cLCD.GetPicture(&sPicture, 0);
	AssertInt(0x1F, sPicture.aauiDots[0][5][0]);
	cLCD.GetPicture(&sPicture, (uint64)TEST_HCP65_CLOCK_RATE * 4096 / 10000);
	AssertInt(0x00, sPicture.aauiDots[0][5][0]);
	TestHCP65LCDSend(&cHCP65, &cLCD, false, 0x0C);

	//Shifting the display left shows each line from its second character.
	TestHCP65LCDSend(&cHCP65, &cLCD, false, 0x18);
	cLCD.GetPicture(&sPicture, cHCP65.NumCycles());
	AssertInt(0x1F, sPicture.aauiDots[0][0][0]);	//E's top row.
	AssertInt(0x0E, sPicture.aauiDots[1][0][0]);	//O's.
	TestHCP65LCDSend(&cHCP65, &cLCD, false, 0x02);
	cLCD.GetPicture(&sPicture, cHCP65.NumCycles());
	AssertMemory(auiH, sPicture.aauiDots[0][0], HCP65_LCD_CHARACTER_HEIGHT);

	//Four bit: the function set is one transfer, then each byte is two, the high four bits first, reads too (so the
	//busy flag isn't polled between halves; this waits instead).  0x38 in two halves goes back to eight bit.
	cLCD.Write(HCP65_LCD_REGISTERS + HCP65_LCD_COMMAND_E, 0x28);
	cLCD.Read(HCP65_LCD_REGISTERS + HCP65_LCD_COMMAND);
	cHCP65.Run(TEST_HCP65_CLOCK_RATE / 10000);
	cLCD.Write(HCP65_LCD_REGISTERS + HCP65_LCD_COMMAND_E, 0x80);
	cLCD.Read(HCP65_LCD_REGISTERS + HCP65_LCD_COMMAND);
	cLCD.Write(HCP65_LCD_REGISTERS + HCP65_LCD_COMMAND_E, 0x00);
	cLCD.Read(HCP65_LCD_REGISTERS + HCP65_LCD_COMMAND);
	cHCP65.Run(TEST_HCP65_CLOCK_RATE / 10000);
	cLCD.Write(HCP65_LCD_REGISTERS + HCP65_LCD_DATA_E, 0x50);
	cLCD.Read(HCP65_LCD_REGISTERS + HCP65_LCD_DATA);
	cLCD.Write(HCP65_LCD_REGISTERS + HCP65_LCD_DATA_E, 0xA0);
	cLCD.Read(HCP65_LCD_REGISTERS + HCP65_LCD_DATA);
	cHCP65.Run(TEST_HCP65_CLOCK_RATE / 10000);
	AssertInt('Z', cLCD.GetDDRAM(0x00));
	AssertLong(1, (int64)cLCD.NumIgnored());
	cLCD.Read(HCP65_LCD_REGISTERS + HCP65_LCD_STATUS);
	AssertInt(0x0F, cLCD.Read(HCP65_LCD_REGISTERS + HCP65_LCD_STATUS_E));
	cLCD.Read(HCP65_LCD_REGISTERS + HCP65_LCD_STATUS);
	AssertInt(0x1F, cLCD.Read(HCP65_LCD_REGISTERS + HCP65_LCD_STATUS_E));
	cLCD.Read(HCP65_LCD_REGISTERS + HCP65_LCD_STATUS);
	cLCD.Write(HCP65_LCD_REGISTERS + HCP65_LCD_COMMAND_E, 0x30);
	cLCD.Read(HCP65_LCD_REGISTERS + HCP65_LCD_COMMAND);
	cLCD.Write(HCP65_LCD_REGISTERS + HCP65_LCD_COMMAND_E, 0x80);
	cLCD.Read(HCP65_LCD_REGISTERS + HCP65_LCD_COMMAND);
	TestHCP65LCDWait(&cHCP65, &cLCD);
	AssertInt(0x01, TestHCP65LCDStatus(&cLCD));
	cLCD.GetPicture(&sPicture, cHCP65.NumCycles());
	AssertMemory(auiW, sPicture.aauiDots[1][0], HCP65_LCD_CHARACTER_HEIGHT);

	//A reset doesn't reach the display.
	cHCP65.Reset();
	cHCP65.Run(100);
	cLCD.SetHeldInReset(false);
	AssertInt('Z', cLCD.GetDDRAM(0x00));
	cLCD.GetPicture(&sPicture, cHCP65.NumCycles());
	AssertTrue(sPicture.bOn);

	//The SNES controllers: 0 is pressed, the high register the first eight buttons, nothing plugged in reads $FF.
	AssertInt(0xFF, cLCD.Read(HCP65_LCD_REGISTERS + HCP65_LCD_SNES_1_LOW));
	AssertInt(0xFF, cLCD.Read(HCP65_LCD_REGISTERS + HCP65_LCD_SNES_1_HIGH));
	cLCD.SetButtons(0, HCP65_SNES_B | HCP65_SNES_START | HCP65_SNES_A | HCP65_SNES_R);
	cLCD.SetButtons(1, HCP65_SNES_RIGHT | HCP65_SNES_L);
	AssertInt(0xF6, cLCD.Read(HCP65_LCD_REGISTERS + HCP65_LCD_SNES_1_HIGH));
	AssertInt(0xF6, cLCD.Read(HCP65_LCD_REGISTERS + HCP65_LCD_SNES_1_LOW));
	AssertInt(0x7F, cLCD.Read(HCP65_LCD_REGISTERS + HCP65_LCD_SNES_2_HIGH));
	AssertInt(0xFB, cLCD.Read(HCP65_LCD_REGISTERS + HCP65_LCD_SNES_2_LOW));

	//Held in reset the SNES clock stops and the latches keep what they had; writes do nothing.
	cLCD.SetHeldInReset(true);
	cLCD.SetButtons(0, 0);
	AssertInt(0xF6, cLCD.Read(HCP65_LCD_REGISTERS + HCP65_LCD_SNES_1_HIGH));
	cLCD.SetHeldInReset(false);
	AssertInt(0xFF, cLCD.Read(HCP65_LCD_REGISTERS + HCP65_LCD_SNES_1_HIGH));
	cLCD.Write(HCP65_LCD_REGISTERS + HCP65_LCD_SNES_1_HIGH, 0);
	AssertInt(0xFF, cLCD.Read(HCP65_LCD_REGISTERS + HCP65_LCD_SNES_1_HIGH));

	cHCP65.Kill();
	cLCD.Kill();
	cMemory.Kill();
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
	TestHCP65VIA();
	TestHCP65VIABoard();
	TestHCP65VIAInterrupt();
	TestHCP65VIABreakouts();
	TestHCP65PS2Port();
	TestHCP65PS2Keyboard();
	TestHCP65Terminal();
	TestHCP65PS2Board();
	TestHCP65PS2Interrupt();
	TestHCP65VideoBoard();
	TestHCP65BlitterBoard();
	TestHCP65GraphicsBoard();
	TestHCP65LCDBoard();
	TestHCP65Timer();
	TestHCP65Contention();
	TestHCP65MemoryFill();
	TestHCP65MemoryBoard();
	TestHCP65MainboardChangeBoards();

	TestStatistics();
}
