#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "BaseLib/FileUtil.h"
#include "BaseLib/FatImageFile.h"
#include "BaseLib/ArrayUint8.h"
#include "BaseLib/GlobalDataTypesIO.h"
#include "BaseLib/IntelHex.h"
#include "StandardLib/Objects.h"
#include "SupportLib/MODReader.h"
#include "SupportLib/ModPlayer.h"
#include "HCP65Lib/HCP65.h"
#include "HCP65Lib/HCP65DebugPort.h"
#include "HCP65Lib/HCP65Audio.h"
#include "HCP65Lib/HCP65SDCard.h"
#include "TestLib/Assert.h"
#include "HCP65KernalBuilder.h"
#include "HCP65ExeBuilder.h"
#include "../../../ModPlay/ModPlaySamples.h"


//Relative to the test's directory (TestHCP65Lib/TestHCP65Lib), as Visual Studio runs it.
#define TEST_HCP65_COMMON			".." _FS_ ".." _FS_ ".." _FS_ ".." _FS_ "commonlibraries" _FS_ "Libraries"
#define TEST_HCP65_LIBRARIES		".." _FS_ ".." _FS_ ".." _FS_ "Libraries"
#define TEST_HCP65_KERNAL			".." _FS_ ".." _FS_ ".." _FS_ "Kernal"
#define TEST_HCP65_INPUT			".." _FS_ ".." _FS_ "HCP65Emulator" _FS_ "Input"
#define TEST_HCP65_BLANK_SD_CARD	TEST_HCP65_INPUT _FS_ "SDCard.img"	//Freshly formatted; never written.
#define TEST_HCP65_OUTPUT			"Output"
#define TEST_HCP65_SD_CARD_IMAGE	TEST_HCP65_OUTPUT _FS_ "SDCard.img"	//The emulator boots these two (HCP65Emulator.h).
#define TEST_HCP65_KERNAL_ROM		TEST_HCP65_OUTPUT _FS_ "Kernal.rom"
#define TEST_HCP65_USER				".." _FS_ ".." _FS_ ".." _FS_ "Libraries" _FS_ "UserLib"
#define TEST_HCP65_BOOT_PROGRAM		".." _FS_ ".." _FS_ ".." _FS_ "OperatingSystem" _FS_ "HCP65OperatingSystem.vcxproj"
#define TEST_HCP65_MODPLAY_PROGRAM	".." _FS_ ".." _FS_ ".." _FS_ "ModPlay" _FS_ "HCP65ModPlay.vcxproj"
#define TEST_HCP65_SONG				TEST_HCP65_INPUT _FS_ "SpaceDebris.mod"
#define TEST_HCP65_MATH_TABLES		TEST_HCP65_LIBRARIES _FS_ "Libc65" _FS_ "MathTbls"

//The test's machine is fast enough that modplay.exe never runs out of samples, so what it plays is exactly what the host
//renders.
#define TEST_HCP65_CLOCK_RATE		4000000000ULL
#define TEST_HCP65_RUN_CYCLES		20000000
#define TEST_HCP65_MAX_RUNS			2000
#define TEST_MODPLAY_SECONDS		1			//boot.exe's argument, patched into the test's copy of the ROM.
#define TEST_HCP65_SD_SOCKET		2			//Socket 1 is the main memory channel.
#define TEST_HCP65_CARD_SOCKET		2			//The SD Card Board's socket with the volume; socket 0 has a card without one.
#define TEST_HCP65_BLANK_CARD_SIZE	0x100000
#define TEST_HCP65_STRAY_SOCKET		3			//A device with no driver that interrupts.
#define TEST_HCP65_AUDIO_SOCKET		16
#define TEST_HCP65_DEBUG_SOCKET		7			//As the emulator has it.

//The kernal's states (Kernal/Kernal.h, which is W65C816 code).
#define TEST_KERNAL_STATE_DEVICES_SCANNED	2
#define TEST_KERNAL_STATE_MOUNTED			6
#define TEST_KERNAL_STATE_BOOT_FAILED		7
#define TEST_KERNAL_STATE_BOOT_EXITED		8
#define TEST_KERNAL_PROCESS_EXITED			5
#define TEST_KERNAL_FAT32					3


//The kernal's SKernalStatus as it is laid out in MPU RAM (16 bit words, 32 bit longs, little endian).
struct STestKernalStatus
{
	uint16	uiState;
	uint16	uiSDError;
	uint16	uiFatCode;
	uint16	uiFileSystem;
	uint32	uiNumSectors;
	uint32	uiNumClusters;
	uint32	uiFreeClusters;
	uint16	uiBootResult;
	uint16	uiExitCode;
	uint16	uiUserValue;
	uint16	uiBootArgument;
	uint16	uiSDCards;
	uint16	uiSDSocket;
	uint32	uiStrayInterrupts;
};


//A board the kernal knows nothing of that starts interrupting once armed (and enabled): the kernal has no handler for it,
//so it resets it, which quietens it for good.
class CTestStrayDevice : public CHCP65Device
{
protected:
	bool	mbArmed;
	bool	mbQuiet;

public:
	void	Init(void)						{ CHCP65Device::Init(HCP65_SMALL_SLOT_SIZE); mbArmed = false; mbQuiet = false; }
	uint8	Read(uint32 uiOffset) override	{ return 0xFF; }
	void	Write(uint32 uiOffset, uint8 uiValue) override {}
	void	Reset(void) override			{ mbQuiet = mbArmed; }
	bool	IsInterrupting(void) override	{ return mbArmed && !mbQuiet && !mbHeldInReset; }
	void	Arm(void)						{ mbArmed = true; }
	bool	IsQuiet(void)					{ return mbQuiet; }
};


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
static void TestKernalReadStatus(uint8* puiRAM, uint32 uiAddress, STestKernalStatus* psStatus)
{
	uint8*	p;

	p = &puiRAM[uiAddress];
	psStatus->uiState = (uint16)(p[0] | (p[1] << 8));
	psStatus->uiSDError = (uint16)(p[2] | (p[3] << 8));
	psStatus->uiFatCode = (uint16)(p[4] | (p[5] << 8));
	psStatus->uiFileSystem = (uint16)(p[6] | (p[7] << 8));
	psStatus->uiNumSectors = (uint32)p[8] | ((uint32)p[9] << 8) | ((uint32)p[10] << 16) | ((uint32)p[11] << 24);
	psStatus->uiNumClusters = (uint32)p[12] | ((uint32)p[13] << 8) | ((uint32)p[14] << 16) | ((uint32)p[15] << 24);
	psStatus->uiFreeClusters = (uint32)p[16] | ((uint32)p[17] << 8) | ((uint32)p[18] << 16) | ((uint32)p[19] << 24);
	psStatus->uiBootResult = (uint16)(p[20] | (p[21] << 8));
	psStatus->uiExitCode = (uint16)(p[22] | (p[23] << 8));
	psStatus->uiUserValue = (uint16)(p[24] | (p[25] << 8));
	psStatus->uiBootArgument = (uint16)(p[26] | (p[27] << 8));
	psStatus->uiSDCards = (uint16)(p[28] | (p[29] << 8));
	psStatus->uiSDSocket = (uint16)(p[30] | (p[31] << 8));
	psStatus->uiStrayInterrupts = (uint32)p[32] | ((uint32)p[33] << 8) | ((uint32)p[34] << 16) | ((uint32)p[35] << 24);
}


//////////////////////////////////////////////////////////////////////////
//
// Builds an executable from its project, szProjectFile (as HCP65-IDE
// builds it; bMathTables as its MathTables), and puts it on the SD card
// image as szName.
//
//////////////////////////////////////////////////////////////////////////
static bool TestKernalPutExe(const char* szName, const char* szProjectFile, const char** aszLibraries, uint32 uiHeapSize, bool bMathTables)
{
	CHCP65ExeBuilder	cExe;
	bool				bBuilt;

	cExe.Init();
	cExe.SetHeapSize(uiHeapSize);
	cExe.SetMathTables(bMathTables);
	bBuilt = cExe.Build(TEST_HCP65_COMMON, TEST_HCP65_LIBRARIES, TEST_HCP65_USER, szProjectFile, aszLibraries);
	AssertString("", cExe.GetErrors());
	AssertTrue(bBuilt);
	if (bBuilt)
	{
		AssertTrue(cExe.GetExecutableSize() > 0);
		if (getenv("TEST_HCP65_VERBOSE"))
		{
			printf("%s: %d bytes\n", szName, (int)cExe.GetExecutableSize());
		}
		bBuilt = FatPutFileOnImage(TEST_HCP65_SD_CARD_IMAGE, szName, cExe.GetExecutable(), cExe.GetExecutableSize());
		AssertTrue(bBuilt);
	}
	cExe.Kill();
	return bBuilt;
}


//////////////////////////////////////////////////////////////////////////
//
// uiLength bytes of the Intel HEX file szHexFile (in MathTbls) from its
// lowest address, in puiDest.
//
//////////////////////////////////////////////////////////////////////////
static bool TestKernalReadTable(const char* szHexFile, uint8* puiDest, uint32 uiLength)
{
	CIntelHex	cHex;
	CChars		szPath;
	bool		bResult;

	szPath.Init(TEST_HCP65_MATH_TABLES _FS_);
	szPath.Append(szHexFile);
	cHex.Init();
	bResult = cHex.Read(szPath.Text());
	AssertString("", cHex.GetError());
	AssertTrue(bResult);
	if (bResult)
	{
		AssertInt(uiLength, (uint32)(cHex.GetEndAddress() - cHex.GetLowAddress()));
		cHex.Fill(cHex.GetLowAddress(), uiLength, 0xFF, puiDest);
	}
	cHex.Kill();
	szPath.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The maths tables' files as one block, MULT then all of INVERT, into
// puiDest (HCP65_MATH_MULT_SIZE + HCP65_MATH_INVERT_SIZE bytes).
//
//////////////////////////////////////////////////////////////////////////
static bool TestKernalReadTables(uint8* puiDest)
{
	bool	bResult;

	bResult = TestKernalReadTable("MULT.HEX", puiDest, HCP65_MATH_MULT_SIZE);
	bResult &= TestKernalReadTable("INVERT.HEX", &puiDest[HCP65_MATH_MULT_SIZE], HCP65_MATH_INVERT_SIZE);
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Copies the blank SD card image to Output and puts boot.exe (HCP65/OperatingSystem),
// modplay.exe (HCP65/ModPlay), the song it plays and the maths tables
// (\OS\Math\MULT.BIN and INVERT.BIN, from MathTbls' HEX files) on the copy.
//
//////////////////////////////////////////////////////////////////////////
static bool TestKernalPutFiles(void)
{
	static const char*	aszModPlayLibraries[] = { "StandardLib", "SupportLib", NULL };
	CFileUtil			cFileUtil;
	uint8*				puiTables;
	bool				bResult;

	//TouchDir returns false when Output is already there (every run after the first).
	cFileUtil.TouchDir(TEST_HCP65_OUTPUT);
	AssertTrue(cFileUtil.IsDirectory(TEST_HCP65_OUTPUT));
	bResult = cFileUtil.Copy(TEST_HCP65_BLANK_SD_CARD, TEST_HCP65_SD_CARD_IMAGE);
	AssertTrue(bResult);
	if (!bResult)
	{
		return false;
	}

	//boot.exe loads the maths tables, so it is built without them; modplay.exe uses them.
	if (!TestKernalPutExe("\\boot.exe", TEST_HCP65_BOOT_PROGRAM, NULL, HCP65_EXE_DEFAULT_HEAP, false))
	{
		return false;
	}
	if (!TestKernalPutExe("\\modplay.exe", TEST_HCP65_MODPLAY_PROGRAM, aszModPlayLibraries, MODPLAY_HEAP_SIZE, true))
	{
		return false;
	}

	puiTables = (uint8*)malloc(HCP65_MATH_MULT_SIZE + HCP65_MATH_INVERT_SIZE);
	bResult = TestKernalReadTables(puiTables);
	bResult = bResult && FatPutFileOnImage(TEST_HCP65_SD_CARD_IMAGE, HCP65_MATH_MULT_PATH, puiTables, HCP65_MATH_MULT_SIZE);
	bResult = bResult && FatPutFileOnImage(TEST_HCP65_SD_CARD_IMAGE, HCP65_MATH_INVERT_PATH, &puiTables[HCP65_MATH_MULT_SIZE], HCP65_MATH_INVERT_SIZE);
	free(puiTables);
	AssertTrue(bResult);
	if (!bResult)
	{
		return false;
	}

	bResult = FatPutDiskFileOnImage(TEST_HCP65_SD_CARD_IMAGE, MODPLAY_FILE, TEST_HCP65_SONG);
	AssertTrue(bResult);
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
static void TestKernalAppend(CArrayUint8* pacDest, uint8* puiSource, uint32 uiLength)
{
	size	uiOld;

	uiOld = pacDest->NumElements();
	pacDest->Resize(uiOld + (size)uiLength);
	memcpy(pacDest->GetData() + uiOld, puiSource, uiLength);
}


//////////////////////////////////////////////////////////////////////////
//
// What modplay.exe should play in uiFrames frames: CModPlayer on the host
// with its settings, a block at a time, as left/right pairs.
//
//////////////////////////////////////////////////////////////////////////
static void TestKernalRenderSong(lsize uiFrames, CArrayUint8* pacFrames)
{
	CModPlayer		cPlayer;
	SModUnsigned8	sBlock;
	uint8			auiSamples[MODPLAY_BLOCK_FRAMES * 2];
	lsize			uiDone;
	lsize			uiBlock;
	lsize			i;

	DataIOInit();
	ObjectsInit();
	Ptr<CModSong> pcSong = LoadMOD((char*)TEST_HCP65_SONG);
	AssertTrue(pcSong.IsNotNull());
	if (pcSong.IsNotNull())
	{
		cPlayer.Init(pcSong, MODPLAY_SAMPLE_RATE, MI_Nearest, 1.0);
		for (uiDone = 0; uiDone < uiFrames; uiDone += uiBlock)
		{
			uiBlock = ((uiFrames - uiDone) < MODPLAY_BLOCK_FRAMES) ? (uiFrames - uiDone) : MODPLAY_BLOCK_FRAMES;
			uiBlock = cPlayer.Render8(&sBlock, uiBlock);
			if (uiBlock == 0)
			{
				break;
			}
			for (i = 0; i < uiBlock; i++)
			{
				auiSamples[i * 2] = sBlock.auiLeft[i];
				auiSamples[i * 2 + 1] = sBlock.auiRight[i];
			}
			TestKernalAppend(pacFrames, auiSamples, (uint32)(uiBlock * 2));
		}
		cPlayer.Kill();
	}
	pcSong = NULL;
	ObjectsKill(false);
	DataIOKill();
}


//////////////////////////////////////////////////////////////////////////
//
// Builds the kernal ROM, boots it with the sound board and the SD Card
// Board (holding Output/SDCard.img, a copy of the emulator's blank
// Input/SDCard.img with the test's boot.exe, modplay.exe, Space Debris and
// the maths tables on it, not written back) and checks that it found both,
// mounted the FAT32 volume and ran boot.exe in user mode, which loaded the
// maths tables to main memory and ran modplay.exe as a second process: it
// played the first TEST_MODPLAY_SECONDS of the song through the sound
// board as CModPlayer does on the host.  The ROM image is written to
// Output/Kernal.rom; the emulator boots it and Output/SDCard.img (where
// boot.exe's argument is zero: modplay.exe plays the whole song).
//
//////////////////////////////////////////////////////////////////////////
void TestHCP65KernalBoot(void)
{
	CHCP65KernalBuilder	cBuilder;
	CHCP65				cHCP65;
	CHCP65Audio			cAudio;
	CHCP65SDCard		cSDCard;
	uint8*				puiBlankCard;
	CTestStrayDevice	cStray;
	CHCP65DebugPort		cPort;
	EDebugProcessEvent	eEvent;
	SDebugProcess		sProcess;
	STestKernalStatus	sStatus;
	CArrayUint8			acPlayed;
	CArrayUint8			acExpected;
	uint8				auiFrames[0x1000 * 2];
	uint32				uiStatus;
	uint32				uiArgument;
	uint32				uiSeconds;
	uint64				uiRunCycles;
	uint32				uiFrames;
	uint32				uiFirstDifference;
	uint32				uiExpectedFrames;
	uint8*				puiRAM;
	uint8*				puiMain;
	uint8*				puiROM;
	uint8*				puiPlayed;
	uint8*				puiTables;
	CChars				szMissing;
	int					i;
	bool				bBuilt;
	bool				bSilence;
	CAsm65LineTable*	pcLines;
	SAsm65LineEntry*	psLine;

	if (!TestKernalPutFiles())
	{
		return;
	}

	cBuilder.Init();
	bBuilt = cBuilder.Build(TEST_HCP65_COMMON, TEST_HCP65_LIBRARIES, TEST_HCP65_KERNAL);
	AssertString("", cBuilder.GetErrors());
	AssertTrue(bBuilt);
	if (!bBuilt)
	{
		cBuilder.Kill();
		return;
	}
	AssertTrue(cBuilder.GetCodeEnd() > 0x10000);
	AssertTrue(cBuilder.GetCodeEnd() <= HCP65_ROM_CODE_END);

	//Nothing is stubbed: the kernal is built without the maths tables (they aren't in memory until boot.exe loads
	//them), so nothing it links may read MathMult or MathInvert.
	szMissing.Init();
	cBuilder.GetBuilder()->GetMissingNames(&szMissing);
	AssertString("", szMissing.Text());
	szMissing.Kill();
	AssertTrue(cBuilder.GetSymbol("_gsKernalStatus", &uiStatus));
	AssertTrue(cBuilder.Write(TEST_HCP65_KERNAL_ROM));

	//Start.asm's lines are in the line table, as the C++ units' are, so a debugger can stop at them: the native
	//vectors at $FFE4 are its data.
	pcLines = cBuilder.GetBuilder()->GetLineTable();
	psLine = pcLines->FindLine(0xFFE4);
	AssertTrue(psLine != NULL);
	if (psLine)
	{
		AssertTrue(strstr(pcLines->GetFile(psLine->uiFile), "Start.asm") != NULL);
	}

	//guiKernalBootArgument is image data: in the ROM at its MPU RAM address (the kernal's status is set by the start
	//up).  boot.exe gets the boot argument.
	puiROM = cBuilder.GetROM();
	AssertTrue(cBuilder.GetSymbol("_guiKernalBootArgument", &uiArgument));
	AssertInt(0xFFFF, (int32)(puiROM[uiArgument] | (puiROM[uiArgument + 1] << 8)));
	//TEST_MODPLAY_SECONDS, TEST_HCP65_RUN_CYCLES and TEST_HCP65_CLOCK (environment) override them, to time modplay.exe
	//(at the board's 25000000 it must keep the sound board fed, or what it plays won't match).
	uiSeconds = getenv("TEST_MODPLAY_SECONDS") ? (uint32)atoi(getenv("TEST_MODPLAY_SECONDS")) : TEST_MODPLAY_SECONDS;
	uiRunCycles = getenv("TEST_HCP65_RUN_CYCLES") ? (uint64)atoll(getenv("TEST_HCP65_RUN_CYCLES")) : TEST_HCP65_RUN_CYCLES;
	puiROM[uiArgument] = (uint8)uiSeconds;
	puiROM[uiArgument + 1] = (uint8)(uiSeconds >> 8);

	if (getenv("TEST_HCP65_VERBOSE"))
	{
		printf("status at $%06X, boot argument at $%06X\n", uiStatus, uiArgument);
	}
	cHCP65.Init(getenv("TEST_HCP65_CLOCK") ? (uint64)atoll(getenv("TEST_HCP65_CLOCK")) : TEST_HCP65_CLOCK_RATE);
	cAudio.Init();
	cSDCard.Init();
	cPort.Init();
	//The kernal brings up each socket's card and mounts the first volume it finds: socket 0's card has none.
	puiBlankCard = (uint8*)calloc(1, TEST_HCP65_BLANK_CARD_SIZE);
	AssertTrue(cSDCard.InsertCard(puiBlankCard, TEST_HCP65_BLANK_CARD_SIZE, 0));
	free(puiBlankCard);
	AssertTrue(cSDCard.InsertCard(TEST_HCP65_SD_CARD_IMAGE, false, TEST_HCP65_CARD_SOCKET));
	AssertTrue(cHCP65.SetDevice(TEST_HCP65_AUDIO_SOCKET, &cAudio));
	AssertTrue(cHCP65.SetDevice(TEST_HCP65_SD_SOCKET, &cSDCard));
	AssertTrue(cHCP65.SetDevice(TEST_HCP65_DEBUG_SOCKET, &cPort));
	cStray.Init();
	AssertTrue(cHCP65.SetDevice(TEST_HCP65_STRAY_SOCKET, &cStray));
	cPort.Watch(true);
	AssertTrue(cHCP65.LoadROM(puiROM, HCP65_ROM_SIZE));

	//Fill mode: memory nothing wrote reads $CC (the kernal doesn't clear the banks it gives processes), so a program
	//that relied on zeroed memory would fail here.
	cHCP65.SetMemoryFill(true);
	cHCP65.Reset();

	//The sound board's frames are taken as they are played: it keeps no more than 64K of them.
	acPlayed.Init();
	puiRAM = cHCP65.GetMPURAM();
	for (i = 0; i < (int)(TEST_HCP65_MAX_RUNS * (TEST_HCP65_RUN_CYCLES / uiRunCycles)); i++)
	{
		cHCP65.Run(uiRunCycles);
		do
		{
			uiFrames = cAudio.ReadFrames(auiFrames, 0x1000);
			TestKernalAppend(&acPlayed, auiFrames, uiFrames * 2);
		}
		while (uiFrames > 0);
		TestKernalReadStatus(puiRAM, uiStatus, &sStatus);

		//Once modplay plays (a process with interrupts on), the stray device is enabled and starts interrupting.
		if (cAudio.IsEnabled() && !cHCP65.IsKernelMode() && (cHCP65.GetDeviceEnables() & (1u << TEST_HCP65_STRAY_SOCKET)) == 0)
		{
			cStray.Arm();
			cHCP65.SetDeviceEnables(cHCP65.GetDeviceEnables() | (1u << TEST_HCP65_STRAY_SOCKET));
			cHCP65.UpdateInterrupts();
		}
		if (getenv("TEST_HCP65_VERBOSE"))
		{
			printf("run %d: state %d (boot %d, exit %d, argument %d), process $%02X, depth %d, kernel %d, PC $%06X, sound %s, counter $%04X, frames %d\n", i, sStatus.uiState, sStatus.uiBootResult, sStatus.uiExitCode, sStatus.uiBootArgument, cHCP65.GetProcessID(), cHCP65.GetInterruptDepth(), cHCP65.IsKernelMode(), cHCP65.GetMPU()->GetProgramAddress(), cAudio.IsEnabled() ? "on" : "off", cAudio.GetCounter(), (int)(acPlayed.NumElements() / 2));
			fflush(stdout);
		}
		//Until the start up has copied the kernal's data its status is whatever MPU RAM held ($CC in fill mode).
		if (((sStatus.uiState >= TEST_KERNAL_STATE_BOOT_FAILED) && (sStatus.uiState <= TEST_KERNAL_STATE_BOOT_EXITED)) || ((sStatus.uiState > TEST_KERNAL_STATE_DEVICES_SCANNED) && (sStatus.uiState < TEST_KERNAL_STATE_MOUNTED)) || cHCP65.IsStopped())
		{
			break;
		}
	}

	AssertFalse(cHCP65.IsStopped());
	AssertTrue(cHCP65.IsNativeLatched());
	AssertTrue(cHCP65.IsKernelMode());
	AssertInt(TEST_KERNAL_STATE_BOOT_EXITED, sStatus.uiState);
	AssertInt(0, sStatus.uiSDError);
	AssertInt(0, sStatus.uiFatCode);
	AssertInt(TEST_KERNAL_FAT32, sStatus.uiFileSystem);
	AssertInt(cSDCard.NumSectors(TEST_HCP65_CARD_SOCKET), sStatus.uiNumSectors);
	AssertInt((1 << 0) | (1 << TEST_HCP65_CARD_SOCKET), sStatus.uiSDCards);
	AssertInt(TEST_HCP65_CARD_SOCKET, sStatus.uiSDSocket);
	AssertTrue(sStatus.uiNumClusters >= 65525);
	AssertTrue(sStatus.uiFreeClusters <= sStatus.uiNumClusters);
	AssertTrue(cSDCard.NumSectorsRead() > 0);
	AssertInt(0, cSDCard.NumSectorsWritten());

	//The kernal never used main memory while the SD Card Board's DMA had it, and reset the stray device.
	AssertFalse(cHCP65.IsContention());
	AssertInt(HCP65_DEBUG_FILL_BYTE, cHCP65.GetMainMemory()[0x300000]);
	AssertInt(1 << TEST_HCP65_STRAY_SOCKET, sStatus.uiStrayInterrupts);
	AssertTrue(cStray.IsQuiet());
	AssertInt(uiSeconds, (uint32)sStatus.uiBootArgument);

	//boot.exe ran in user mode (process $FF), multiplied and divided with the tables, gave the kernal 300 * 200 / 3
	//and ran modplay.exe (process $FE), whose exit code it exited with; the kernal is back in kernel mode.
	AssertInt(TEST_KERNAL_PROCESS_EXITED, sStatus.uiBootResult);
	AssertInt(MODPLAY_EXIT_SUCCESS, sStatus.uiExitCode);
	AssertInt(20000, sStatus.uiUserValue);
	AssertInt(0xFF, cHCP65.GetProcessID());
	AssertInt(1, cHCP65.GetInterruptDepth());

	//The kernal told the debug port of each process, in order: boot.exe ($FF) started, modplay.exe ($FE) started and
	//ended, then boot.exe ended with modplay.exe's exit code.  Each start stopped a Run, which went on in the next.
	AssertTrue(cPort.TakeEvent(&eEvent, &sProcess));
	AssertInt(DPE_Started, eEvent);
	AssertInt(0xFF, sProcess.uiProcess);
	AssertString("\\boot.exe", sProcess.szName);
	AssertTrue(cPort.TakeEvent(&eEvent, &sProcess));
	AssertInt(DPE_Started, eEvent);
	AssertInt(0xFE, sProcess.uiProcess);
	AssertString("\\modplay.exe", sProcess.szName);
	AssertTrue(cPort.TakeEvent(&eEvent, &sProcess));
	AssertInt(DPE_Ended, eEvent);
	AssertInt(0xFE, sProcess.uiProcess);
	AssertInt(MODPLAY_EXIT_SUCCESS, sProcess.iExitCode);
	AssertTrue(cPort.TakeEvent(&eEvent, &sProcess));
	AssertInt(DPE_Ended, eEvent);
	AssertInt(0xFF, sProcess.uiProcess);
	AssertInt(sStatus.uiExitCode, sProcess.iExitCode);
	AssertFalse(cPort.TakeEvent(&eEvent, &sProcess));
	AssertSize(0, cPort.NumProcesses());

	//The SD Card Board was enabled at boot, the sound board by modplay.exe, which left it disabled, and the stray device
	//by the test (the kernal's reset of it put its enable back).
	AssertInt((1 << TEST_HCP65_SD_SOCKET) | (1 << TEST_HCP65_AUDIO_SOCKET) | (1 << TEST_HCP65_STRAY_SOCKET), cHCP65.GetDeviceEnables());
	AssertFalse(cAudio.IsEnabled());

	//boot.exe loaded MULT and INVERT's upper half (round(2^32 / $8000) first) from the SD card to main memory at
	//$400000, and the kernal was copied from ROM banks 0-3 to MPU RAM.  The processes' pages of the remap table were
	//cleared when they exited.
	puiMain = cHCP65.GetMainMemory();
	puiTables = (uint8*)malloc(HCP65_MATH_MULT_SIZE + HCP65_MATH_INVERT_SIZE);
	AssertTrue(TestKernalReadTables(puiTables));
	AssertTrue(memcmp(puiTables, &puiMain[HCP65_TABLES_START], HCP65_MATH_MULT_SIZE) == 0);
	AssertTrue(memcmp(&puiTables[HCP65_MATH_MULT_SIZE + HCP65_MATH_INVERT_OFFSET], &puiMain[HCP65_TABLES_START + HCP65_MATH_MULT_SIZE], HCP65_TABLES_END - HCP65_TABLES_START - HCP65_MATH_MULT_SIZE) == 0);
	free(puiTables);
	AssertInt(15, puiMain[HCP65_TABLES_START + ((3 << 8) | 5) * 2]);
	AssertInt(0x020000, puiMain[HCP65_TABLES_START + HCP65_MATH_MULT_SIZE] | (puiMain[HCP65_TABLES_START + HCP65_MATH_MULT_SIZE + 1] << 8) | (puiMain[HCP65_TABLES_START + HCP65_MATH_MULT_SIZE + 2] << 16));
	AssertTrue(memcmp(&puiROM[0x10000], &puiRAM[0x10000], 0x100) == 0);
	for (i = 0; i < 0x400; i++)
	{
		if (puiRAM[HCP65_REMAP_TABLE + 0xFC00 + i] != 0)
		{
			break;
		}
	}
	AssertInt(0x400, i);

	//What the sound board played: the song's first seconds as the host plays them, then silence until it was stopped.
	uiExpectedFrames = uiSeconds * MODPLAY_SAMPLE_RATE;
	acExpected.Init();
	TestKernalRenderSong(uiExpectedFrames, &acExpected);
	AssertInt(uiExpectedFrames * 2, (uint32)acExpected.NumElements());
	AssertTrue(acPlayed.NumElements() / 2 >= uiExpectedFrames);
	AssertTrue(acPlayed.NumElements() / 2 <= uiExpectedFrames + MODPLAY_BLOCK_FRAMES);
	if (acPlayed.NumElements() >= acExpected.NumElements())
	{
		puiPlayed = acPlayed.GetData();
		for (uiFirstDifference = 0; uiFirstDifference < acExpected.NumElements(); uiFirstDifference++)
		{
			if (puiPlayed[uiFirstDifference] != acExpected.GetData()[uiFirstDifference])
			{
				break;
			}
		}
		AssertInt((uint32)acExpected.NumElements(), uiFirstDifference);
		bSilence = true;
		for (i = (int)acExpected.NumElements(); i < (int)acPlayed.NumElements(); i++)
		{
			bSilence &= puiPlayed[i] == 0x80;
		}
		AssertTrue(bSilence);
	}
	AssertTrue(cAudio.NumDroppedFrames() == 0);
	acExpected.Kill();
	acPlayed.Kill();

	cHCP65.Kill();
	cPort.Kill();
	cStray.Kill();
	cSDCard.Kill();
	cAudio.Kill();
	cBuilder.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void TestHCP65Kernal(void)
{
	BeginTests();

	TestHCP65KernalBoot();

	TestStatistics();
}
