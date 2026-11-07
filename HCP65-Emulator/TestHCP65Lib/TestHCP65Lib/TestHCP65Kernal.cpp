#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "BaseLib/FileUtil.h"
#include "BaseLib/FatImageFile.h"
#include "BaseLib/ArrayUint8.h"
#include "BaseLib/GlobalDataTypesIO.h"
#include "BaseLib/IntelHex.h"
#include "BaseLib/MemoryFile.h"
#include "BaseLib/PointerFunctions.h"
#include "Assembler65Lib/Asm65Object.h"
#include "StandardLib/Objects.h"
#include "SupportLib/MODReader.h"
#include "SupportLib/ModPlayer.h"
#include "HCP65Lib/HCP65.h"
#include "HCP65Lib/HCP65DebugPort.h"
#include "HCP65Lib/HCP65Audio.h"
#include "HCP65Lib/HCP65SDCard.h"
#include "HCP65Lib/HCP65UART.h"
#include "TestLib/Assert.h"
#include "HCP65KernalBuilder.h"
#include "HCP65ExeBuilder.h"
#include "HCP65DriverBuilder.h"
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
#define TEST_HCP65_SOUND_DRIVER		".." _FS_ ".." _FS_ ".." _FS_ "Drivers" _FS_ "SoundBoard" _FS_ "HCP65SoundBoard.vcxproj"
#define TEST_HCP65_SOUND_DRIVER_PATH	"\\OS\\Drivers\\SOUND.DRV"
#define TEST_HCP65_MODPLAY_PROGRAM	".." _FS_ ".." _FS_ ".." _FS_ "ModPlay" _FS_ "HCP65ModPlay.vcxproj"
#define TEST_HCP65_SONG				TEST_HCP65_INPUT _FS_ "SpaceDebris.mod"
#define TEST_HCP65_MATH_TABLES		TEST_HCP65_LIBRARIES _FS_ "Libc65" _FS_ "MathTbls"
#define TEST_HCP65_UART_CARD_IMAGE	TEST_HCP65_OUTPUT _FS_ "UARTCard.img"	//HCP65UARTTest.hcp65build's card.
#define TEST_HCP65_UART_PROGRAM		".." _FS_ ".." _FS_ ".." _FS_ "UARTTest" _FS_ "HCP65UARTTest.vcxproj"
#define TEST_HCP65_UART_DRIVER		".." _FS_ ".." _FS_ ".." _FS_ "Drivers" _FS_ "UARTBoard" _FS_ "HCP65UARTBoard.vcxproj"
#define TEST_HCP65_UART_DRIVER_PATH	"\\OS\\Drivers\\UART.DRV"

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
#define TEST_HCP65_AUDIO2_SOCKET	17			//A second sound board, which the same driver drives.
#define TEST_HCP65_DEBUG_SOCKET		7			//As the emulator has it.
#define TEST_HCP65_UART_SOCKET		4

//UARTTest.cpp's: what it sends and what the far end sends it.
#define TEST_UART_SENT				1000
#define TEST_UART_RECEIVED			2000
#define TEST_UART_ECHOES			50
#define TEST_UART_CLOCK_RATE		HCP65_DEFAULT_CLOCK_RATE	//The real board's: the driver must keep up at 115200 baud.
#define TEST_UART_RUN_CYCLES		250000						//10ms.
#define TEST_UART_MAX_RUNS			3000

//The kernal's states (Kernal/Kernal.h, which is W65C816 code).
#define TEST_KERNAL_STATE_DEVICES_SCANNED	2
#define TEST_KERNAL_STATE_MOUNTED			6
#define TEST_KERNAL_STATE_BOOT_FAILED		7
#define TEST_KERNAL_STATE_BOOT_EXITED		8
#define TEST_KERNAL_PROCESS_EXITED			5
#define TEST_KERNAL_PROCESS_BAD_ADDRESS		7
#define TEST_HCP65_MISPLACED_CARD		TEST_HCP65_OUTPUT _FS_ "Misplaced.img"	//The card with a misplaced modplay.exe.
#define TEST_HCP65_MISPLACED_AT			0x080000	//In the small heap's banks.
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
	uint32	uiDevices;
	uint32	uiBadDevices;
	uint32	uiLoadedDrivers;
	uint16	uiDriverError;
};


//A board the kernal knows nothing of that starts interrupting once armed (and enabled): the kernal has no handler for it,
//so it resets it, which quietens it for good.  Its ROM is blank ($00), so the kernal's scan finds it unrecognised.
class CTestStrayDevice : public CHCP65Device
{
protected:
	bool	mbArmed;
	bool	mbQuiet;

public:
	void	Init(void)						{ CHCP65Device::Init(HCP65_SMALL_SLOT_SIZE); mbArmed = false; mbQuiet = false; }
	uint8	Read(uint32 uiOffset) override	{ return (uiOffset < DEVICE_ROM_SIZE) ? 0x00 : 0xFF; }
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
	psStatus->uiDevices = (uint32)p[36] | ((uint32)p[37] << 8) | ((uint32)p[38] << 16) | ((uint32)p[39] << 24);
	psStatus->uiBadDevices = (uint32)p[40] | ((uint32)p[41] << 8) | ((uint32)p[42] << 16) | ((uint32)p[43] << 24);
	psStatus->uiLoadedDrivers = (uint32)p[44] | ((uint32)p[45] << 8) | ((uint32)p[46] << 16) | ((uint32)p[47] << 24);
	psStatus->uiDriverError = (uint16)(p[48] | (p[49] << 8));
}


//////////////////////////////////////////////////////////////////////////
//
// Builds an executable from its project, szProjectFile (as HCP65-IDE
// builds it; bMathTables as its MathTables), and puts it on the SD card
// image szImage as szName.
//
//////////////////////////////////////////////////////////////////////////
static bool TestKernalPutExe(const char* szImage, const char* szName, const char* szProjectFile, const char** aszLibraries, uint32 uiHeapSize, bool bMathTables)
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
		bBuilt = FatPutFileOnImage(szImage, szName, cExe.GetExecutable(), cExe.GetExecutableSize());
		AssertTrue(bBuilt);
	}
	cExe.Kill();
	return bBuilt;
}


//////////////////////////////////////////////////////////////////////////
//
// Builds a driver from its project, szProjectFile, and puts it on the SD
// card image szImage as szName.
//
//////////////////////////////////////////////////////////////////////////
static bool TestKernalPutDriver(const char* szImage, const char* szName, const char* szProjectFile)
{
	CHCP65DriverBuilder	cDriver;
	bool				bBuilt;
	size				i;
	FILE*				pFile;

	cDriver.Init();
	bBuilt = cDriver.Build(TEST_HCP65_COMMON, TEST_HCP65_LIBRARIES, TEST_HCP65_KERNAL, szProjectFile);
	AssertString("", cDriver.GetErrors());
	AssertTrue(bBuilt);
	if (bBuilt)
	{
		AssertTrue(cDriver.GetModuleSize() > 0);
		if (getenv("TEST_HCP65_VERBOSE"))
		{
			printf("%s: %d bytes, imports", szName, (int)cDriver.GetModuleSize());
			for (i = 0; i < cDriver.GetLinker()->NumMissing(); i++)
			{
				printf(" %s", cDriver.GetLinker()->GetMissing(i));
			}
			printf("\n");
		}
		if (getenv("TEST_HCP65_MAP"))
		{
			pFile = fopen(TEST_HCP65_OUTPUT _FS_ "Driver.map", "w");
			if (pFile)
			{
				fputs(cDriver.GetLinker()->GetMap(), pFile);
				fclose(pFile);
			}
		}
		bBuilt = FatPutFileOnImage(szImage, szName, cDriver.GetModule(), cDriver.GetModuleSize());
		AssertTrue(bBuilt);
	}
	cDriver.Kill();
	return bBuilt;
}


//////////////////////////////////////////////////////////////////////////
//
// A 24 bit address in the kernal's ROM image at uiAddress (a .long: the
// address and a zero byte).
//
//////////////////////////////////////////////////////////////////////////
static uint32 TestKernalROMLong(uint8* puiROM, uint32 uiAddress)
{
	return (uint32)puiROM[uiAddress] | ((uint32)puiROM[uiAddress + 1] << 8) | ((uint32)puiROM[uiAddress + 2] << 16);
}


//////////////////////////////////////////////////////////////////////////
//
// The kernal's exports (Kernal/KernalExports.asm) as its ROM has them:
// each name is the symbol at its address in the kernal's link, so a
// driver that imports the name gets that symbol.
//
//////////////////////////////////////////////////////////////////////////
static void TestKernalExports(CHCP65KernalBuilder* pcBuilder)
{
	uint8*	puiROM;
	uint32	uiTable;
	uint32	uiCount;
	uint32	uiAddress;
	uint32	uiSymbol;
	char*	szName;
	CChars	szWrong;
	uint32	uiNumExports;
	uint32	i;

	AssertTrue(pcBuilder->GetSymbol("_gauiKernalExports", &uiTable));
	AssertTrue(pcBuilder->GetSymbol("_guiKernalNumExports", &uiCount));
	puiROM = pcBuilder->GetROM();
	uiNumExports = (uint32)puiROM[uiCount] | ((uint32)puiROM[uiCount + 1] << 8);
	AssertTrue(uiNumExports > 0);

	szWrong.Init();
	for (i = 0; i < uiNumExports; i++)
	{
		szName = (char*)&puiROM[TestKernalROMLong(puiROM, uiTable + i * 8)];
		uiAddress = TestKernalROMLong(puiROM, uiTable + i * 8 + 4);
		if (!pcBuilder->GetSymbol(szName, &uiSymbol) || (uiSymbol != uiAddress))
		{
			szWrong.Append(szName);
			szWrong.Append(" ");
		}
	}
	AssertString("", szWrong.Text());
	szWrong.Kill();
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
// Output/szImage, a copy of the emulator's blank SD card image.
//
//////////////////////////////////////////////////////////////////////////
static bool TestKernalCopyBlankCard(const char* szImage)
{
	CFileUtil	cFileUtil;
	bool		bResult;

	//TouchDir returns false when Output is already there (every run after the first).
	cFileUtil.TouchDir(TEST_HCP65_OUTPUT);
	AssertTrue(cFileUtil.IsDirectory(TEST_HCP65_OUTPUT));
	bResult = cFileUtil.Copy(TEST_HCP65_BLANK_SD_CARD, szImage);
	AssertTrue(bResult);
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Sets boot.exe's argument to uiArgument and gives where the kernal's
// status is.  guiKernalBootArgument is image data: in the ROM at its MPU
// RAM address (the kernal's status is set by the start up).
//
//////////////////////////////////////////////////////////////////////////
static void TestKernalSetBootArgument(CHCP65KernalBuilder* pcBuilder, uint16 uiArgument, uint32* puiStatus)
{
	uint8*	puiROM;
	uint32	uiAddress;

	puiROM = pcBuilder->GetROM();
	AssertTrue(pcBuilder->GetSymbol("_gsKernalStatus", puiStatus));
	AssertTrue(pcBuilder->GetSymbol("_guiKernalBootArgument", &uiAddress));
	puiROM[uiAddress] = (uint8)uiArgument;
	puiROM[uiAddress + 1] = (uint8)(uiArgument >> 8);
	if (getenv("TEST_HCP65_VERBOSE"))
	{
		printf("status at $%06X, boot argument at $%06X\n", *puiStatus, uiAddress);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Puts the debug port in its socket, watched, loads the kernal ROM and
// resets the machine, its other boards in place.  Fill mode: memory
// nothing wrote reads $CC (the kernal doesn't clear the banks it gives
// processes), so a program that relied on zeroed memory would fail.
//
//////////////////////////////////////////////////////////////////////////
static void TestKernalStart(CHCP65* pcHCP65, CHCP65DebugPort* pcPort, CHCP65KernalBuilder* pcBuilder)
{
	AssertTrue(pcHCP65->SetDevice(TEST_HCP65_DEBUG_SOCKET, pcPort));
	pcPort->Watch(true);
	AssertTrue(pcHCP65->LoadROM(pcBuilder->GetROM(), HCP65_ROM_SIZE));
	pcHCP65->SetMemoryFill(true);
	pcHCP65->Reset();
}


//////////////////////////////////////////////////////////////////////////
//
// Copies the blank SD card image to Output and puts boot.exe (HCP65/OperatingSystem),
// modplay.exe (HCP65/ModPlay), the sound board's driver (HCP65/Drivers/
// SoundBoard, \OS\Drivers\SOUND.DRV), the UART Board's (for the
// emulator, which has one), the song modplay plays and the maths tables
// (\OS\Math\MULT.BIN and INVERT.BIN, from MathTbls' HEX files) on the copy.
//
//////////////////////////////////////////////////////////////////////////
static bool TestKernalPutFiles(void)
{
	static const char*	aszModPlayLibraries[] = { "StandardLib", "SupportLib", NULL };
	uint8*				puiTables;
	bool				bResult;

	if (!TestKernalCopyBlankCard(TEST_HCP65_SD_CARD_IMAGE))
	{
		return false;
	}

	//boot.exe loads the maths tables, so it is built without them; modplay.exe uses them.
	if (!TestKernalPutExe(TEST_HCP65_SD_CARD_IMAGE, "\\boot.exe", TEST_HCP65_BOOT_PROGRAM, NULL, HCP65_EXE_DEFAULT_HEAP, false))
	{
		return false;
	}
	if (!TestKernalPutExe(TEST_HCP65_SD_CARD_IMAGE, "\\modplay.exe", TEST_HCP65_MODPLAY_PROGRAM, aszModPlayLibraries, MODPLAY_HEAP_SIZE, true))
	{
		return false;
	}
	if (!TestKernalPutDriver(TEST_HCP65_SD_CARD_IMAGE, TEST_HCP65_SOUND_DRIVER_PATH, TEST_HCP65_SOUND_DRIVER))
	{
		return false;
	}
	if (!TestKernalPutDriver(TEST_HCP65_SD_CARD_IMAGE, TEST_HCP65_UART_DRIVER_PATH, TEST_HCP65_UART_DRIVER))
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
// An executable the kernal must refuse: its code (a few STPs) at uiAt, its
// UserStart there, and no UserHeapSize, so it would have the small heap.
//
//////////////////////////////////////////////////////////////////////////
static bool TestKernalMakeMisplacedExe(uint32 uiAt, CArrayUint8* pacDest)
{
	CAsm65Object		cObject;
	SAsm65Section*		psSection;
	SAsm65ObjectSymbol*	psSymbol;
	CMemoryFile*		pcMemoryFile;
	uint32				uiSection;
	bool				bResult;

	cObject.Init("Misplaced");
	uiSection = cObject.AddSection("segment0", "", ASK65_Data);
	psSection = cObject.GetSection(uiSection);
	psSection->bAt = true;
	psSection->uiAt = uiAt;
	psSection->bKeep = true;
	psSection->uiSize = 4;
	psSection->cBytes.Resize(4);
	memset(psSection->cBytes.GetData(), 0xDB, 4);
	psSymbol = cObject.GetSymbol(cObject.PutSymbol(HCP65_EXE_ENTRY_SYMBOL));
	psSymbol->iSection = -1;
	psSymbol->iValue = uiAt;
	psSymbol->eBinding = AB65_Global;

	pcMemoryFile = MemoryFile();
	pcMemoryFile->mbBasicFileMustFree = false;
	bResult = cObject.Write(pcMemoryFile);
	if (bResult)
	{
		pacDest->Resize((size)pcMemoryFile->GetBufferSize());
		memcpy(pacDest->GetData(), pcMemoryFile->GetBufferPointer(), (size)pcMemoryFile->GetBufferSize());
	}
	SafeKill(pcMemoryFile);
	cObject.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Builds the kernal ROM and writes it to Output/Kernal.rom, which the
// emulator boots: its exports are right, nothing is stubbed and its boot
// argument is $FFFF.
//
//////////////////////////////////////////////////////////////////////////
static bool TestKernalBuild(CHCP65KernalBuilder* pcBuilder)
{
	CAsm65LineTable*	pcLines;
	SAsm65LineEntry*	psLine;
	CChars				szMissing;
	uint8*				puiROM;
	uint32				uiArgument;
	FILE*				pFile;
	bool				bBuilt;

	bBuilt = pcBuilder->Build(TEST_HCP65_COMMON, TEST_HCP65_LIBRARIES, TEST_HCP65_KERNAL);
	AssertString("", pcBuilder->GetErrors());
	AssertTrue(bBuilt);
	if (!bBuilt)
	{
		return false;
	}
	AssertTrue(pcBuilder->GetCodeEnd() > 0x10000);
	AssertTrue(pcBuilder->GetCodeEnd() <= HCP65_ROM_CODE_END);
	if (getenv("TEST_HCP65_MAP"))
	{
		//The kernal's link map, to find what is at an address.
		pFile = fopen(TEST_HCP65_OUTPUT _FS_ "Kernal.map", "w");
		if (pFile)
		{
			fputs(pcBuilder->GetBuilder()->GetLinker()->GetMap(), pFile);
			fclose(pFile);
		}
	}

	//Nothing is stubbed: the kernal is built without the maths tables (they aren't in memory until boot.exe loads
	//them), so nothing it links may read MathMult or MathInvert.
	szMissing.Init();
	pcBuilder->GetBuilder()->GetMissingNames(&szMissing);
	AssertString("", szMissing.Text());
	TestKernalExports(pcBuilder);
	szMissing.Kill();

	puiROM = pcBuilder->GetROM();
	AssertTrue(pcBuilder->GetSymbol("_guiKernalBootArgument", &uiArgument));
	AssertInt(0xFFFF, (int32)(puiROM[uiArgument] | (puiROM[uiArgument + 1] << 8)));
	AssertTrue(pcBuilder->Write(TEST_HCP65_KERNAL_ROM));

	//Start.asm's lines are in the line table, as the C++ units' are, so a debugger can stop at them: the native
	//vectors at $FFE4 are its data.
	pcLines = pcBuilder->GetBuilder()->GetLineTable();
	psLine = pcLines->FindLine(0xFFE4);
	AssertTrue(psLine != NULL);
	if (psLine)
	{
		AssertTrue(strstr(pcLines->GetFile(psLine->uiFile), "Start.asm") != NULL);
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Boots the kernal ROM with the sound boards and the SD Card
// Board (holding Output/SDCard.img, a copy of the emulator's blank
// Input/SDCard.img with the test's boot.exe, modplay.exe, Space Debris and
// the maths tables on it, not written back) and checks that it found both,
// mounted the FAT32 volume and ran boot.exe in user mode, which loaded the
// maths tables to main memory and ran modplay.exe as a second process: it
// played the first TEST_MODPLAY_SECONDS of the song through the sound
// board as CModPlayer does on the host.  The emulator boots Output/
// Kernal.rom (TestKernalBuild's) and Output/SDCard.img (where boot.exe's
// argument is zero: modplay.exe plays the whole song).  Then
// boots a copy of the card whose modplay.exe is where its heap would be,
// which the kernal refuses to load.
//
//////////////////////////////////////////////////////////////////////////
void TestHCP65KernalBoot(CHCP65KernalBuilder* pcBuilder)
{
	CHCP65				cHCP65;
	CHCP65Audio			cAudio;
	CHCP65Audio			cAudio2;
	SHCP65DebugDriver	sDriver;
	uint32				uiKernalEnd;
	CHCP65SDCard		cSDCard;
	uint8*				puiBlankCard;
	CTestStrayDevice	cStray;
	CHCP65DebugPort		cPort;
	EDebugProcessEvent	eEvent;
	SDebugProcess		sProcess;
	STestKernalStatus	sStatus;
	CArrayUint8			acPlayed;
	CArrayUint8			acExpected;
	CArrayUint8			acMisplaced;
	CFileUtil			cFileUtil;
	uint8				auiFrames[0x1000 * 2];
	uint32				uiStatus;
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
	int					i;
	bool				bSilence;

	if (!TestKernalPutFiles())
	{
		return;
	}

	//boot.exe's argument is the seconds modplay.exe plays.
	//TEST_MODPLAY_SECONDS, TEST_HCP65_RUN_CYCLES and TEST_HCP65_CLOCK (environment) override them, to time modplay.exe
	//(at the board's 25000000 it must keep the sound board fed, or what it plays won't match).
	uiSeconds = getenv("TEST_MODPLAY_SECONDS") ? (uint32)atoi(getenv("TEST_MODPLAY_SECONDS")) : TEST_MODPLAY_SECONDS;
	uiRunCycles = getenv("TEST_HCP65_RUN_CYCLES") ? (uint64)atoll(getenv("TEST_HCP65_RUN_CYCLES")) : TEST_HCP65_RUN_CYCLES;
	TestKernalSetBootArgument(pcBuilder, (uint16)uiSeconds, &uiStatus);

	cHCP65.Init(getenv("TEST_HCP65_CLOCK") ? (uint64)atoll(getenv("TEST_HCP65_CLOCK")) : TEST_HCP65_CLOCK_RATE);
	cAudio.Init();
	cAudio2.Init();
	cSDCard.Init();
	cPort.Init();
	//The kernal brings up each socket's card and mounts the first volume it finds: socket 0's card has none.
	puiBlankCard = (uint8*)calloc(1, TEST_HCP65_BLANK_CARD_SIZE);
	AssertTrue(cSDCard.InsertCard(puiBlankCard, TEST_HCP65_BLANK_CARD_SIZE, 0));
	free(puiBlankCard);
	AssertTrue(cSDCard.InsertCard(TEST_HCP65_SD_CARD_IMAGE, false, TEST_HCP65_CARD_SOCKET));
	AssertTrue(cHCP65.SetDevice(TEST_HCP65_AUDIO_SOCKET, &cAudio));
	AssertTrue(cHCP65.SetDevice(TEST_HCP65_AUDIO2_SOCKET, &cAudio2));
	AssertTrue(cHCP65.SetDevice(TEST_HCP65_SD_SOCKET, &cSDCard));
	cStray.Init();
	AssertTrue(cHCP65.SetDevice(TEST_HCP65_STRAY_SOCKET, &cStray));
	TestKernalStart(&cHCP65, &cPort, pcBuilder);

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

		//Once modplay plays (a process with interrupts on), the stray device is enabled and starts interrupting.  Modplay
		//spends nearly all its time in the kernal waiting for the sound board, so this doesn't wait for a run to end in
		//user mode: the dispatch resets the stray device whichever mode the interrupt finds the processor in.
		if (cAudio.IsEnabled() && (cHCP65.GetProcessID() == 0xFE) && (cHCP65.GetDeviceEnables() & (1u << TEST_HCP65_STRAY_SOCKET)) == 0)
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

	//The scan found the three boards by their device ROMs, and the stray device's blank ROM unrecognised.
	AssertInt((1 << TEST_HCP65_SD_SOCKET) | (1 << TEST_HCP65_AUDIO_SOCKET) | (1 << TEST_HCP65_AUDIO2_SOCKET) | (1 << TEST_HCP65_DEBUG_SOCKET), sStatus.uiDevices);
	AssertInt(1 << TEST_HCP65_STRAY_SOCKET, sStatus.uiBadDevices);

	//The sound boards' driver was loaded from \OS\Drivers\SOUND.DRV once and drives both (the SD Card Board's and the
	//debug port's are in the ROM); modplay.exe played through the first, with the device calls.  The debug port was told
	//where the driver went, after the kernal, and (watched) stopped the run before the driver ran.
	AssertInt(0, sStatus.uiDriverError);
	AssertInt((1 << TEST_HCP65_AUDIO_SOCKET) | (1 << TEST_HCP65_AUDIO2_SOCKET), sStatus.uiLoadedDrivers);
	AssertSize(1, cPort.NumDrivers());
	AssertTrue(pcBuilder->GetSymbol("__end", &uiKernalEnd));
	AssertString(TEST_HCP65_SOUND_DRIVER_PATH, cPort.GetDriver(0)->szName);
	AssertInt(TEST_HCP65_AUDIO_SOCKET, cPort.GetDriver(0)->uiSlot);
	AssertTrue(cPort.GetDriver(0)->uiAddress >= uiKernalEnd);
	AssertTrue(cPort.GetDriver(0)->uiSize > 0);
	AssertTrue(cPort.GetDriver(0)->uiAddress + cPort.GetDriver(0)->uiSize <= 0x070000);
	AssertTrue(cPort.TakeDriverEvent(&sDriver));
	AssertInt(cPort.GetDriver(0)->uiAddress, sDriver.uiAddress);
	AssertFalse(cPort.TakeDriverEvent(&sDriver));
	AssertTrue(cStray.IsQuiet());
	AssertInt(uiSeconds, (uint32)sStatus.uiBootArgument);

	//boot.exe ran in user mode (process $FF), multiplied and divided with the tables, gave the kernal 300 * 200 / 3
	//and ran modplay.exe (process $FE), whose exit code it exited with; the kernal is back in kernel mode.
	AssertInt(TEST_KERNAL_PROCESS_EXITED, sStatus.uiBootResult);
	AssertInt(MODPLAY_EXIT_SUCCESS, sStatus.uiExitCode);
	AssertInt(20000, sStatus.uiUserValue);
	AssertInt(0xFF, cHCP65.GetProcessID());
	AssertInt(1, cHCP65.GetInterruptDepth());

	//The kernal told the debug port of each process, in order: boot.exe ($FF) started, boot.exe again as the child that
	//checks it can't open the sound board ($FE) started and ended, modplay.exe ($FE) started and ended, then boot.exe
	//ended with modplay.exe's exit code.  Each start stopped a Run, which went on in the next.
	AssertTrue(cPort.TakeEvent(&eEvent, &sProcess));
	AssertInt(DPE_Started, eEvent);
	AssertInt(0xFF, sProcess.uiProcess);
	AssertString("\\boot.exe", sProcess.szName);
	AssertTrue(cPort.TakeEvent(&eEvent, &sProcess));
	AssertInt(DPE_Started, eEvent);
	AssertInt(0xFE, sProcess.uiProcess);
	AssertString("\\boot.exe", sProcess.szName);
	AssertTrue(cPort.TakeEvent(&eEvent, &sProcess));
	AssertInt(DPE_Ended, eEvent);
	AssertInt(0xFE, sProcess.uiProcess);
	AssertInt(0, sProcess.iExitCode);
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

	//The drivers took the SD Card Board, the debug port and both sound boards out of reset at boot (modplay.exe left the
	//first disabled; nothing used the second), and the test the stray device (the kernal's reset of it put its enable
	//back).
	AssertInt((1 << TEST_HCP65_SD_SOCKET) | (1 << TEST_HCP65_AUDIO_SOCKET) | (1 << TEST_HCP65_AUDIO2_SOCKET) | (1 << TEST_HCP65_DEBUG_SOCKET) | (1 << TEST_HCP65_STRAY_SOCKET), cHCP65.GetDeviceEnables());
	AssertFalse(cAudio.IsEnabled());
	AssertFalse(cAudio2.IsEnabled());

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
	puiROM = pcBuilder->GetROM();
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

	//Booted again with a card whose modplay.exe has its code in the small heap's banks and no UserHeapSize: the kernal
	//refuses to load it (KERNAL_PROCESS_BAD_ADDRESS) and boot.exe exits with 100 and why.  The card is a copy.
	acMisplaced.Init();
	AssertTrue(TestKernalMakeMisplacedExe(TEST_HCP65_MISPLACED_AT, &acMisplaced));
	AssertTrue(cFileUtil.Copy(TEST_HCP65_SD_CARD_IMAGE, TEST_HCP65_MISPLACED_CARD));
	AssertTrue(FatPutFileOnImage(TEST_HCP65_MISPLACED_CARD, "\\modplay.exe", acMisplaced.GetData(), acMisplaced.NumElements()));
	acMisplaced.Kill();
	cSDCard.RemoveCard(TEST_HCP65_CARD_SOCKET);
	AssertTrue(cSDCard.InsertCard(TEST_HCP65_MISPLACED_CARD, false, TEST_HCP65_CARD_SOCKET));
	cHCP65.Reset();
	for (i = 0; i < TEST_HCP65_MAX_RUNS; i++)
	{
		cHCP65.Run(TEST_HCP65_RUN_CYCLES);
		TestKernalReadStatus(puiRAM, uiStatus, &sStatus);
		if (((sStatus.uiState >= TEST_KERNAL_STATE_BOOT_FAILED) && (sStatus.uiState <= TEST_KERNAL_STATE_BOOT_EXITED)) || cHCP65.IsStopped())
		{
			break;
		}
	}
	AssertInt(TEST_KERNAL_STATE_BOOT_EXITED, sStatus.uiState);
	AssertInt(TEST_KERNAL_PROCESS_EXITED, sStatus.uiBootResult);
	AssertInt(100 + TEST_KERNAL_PROCESS_BAD_ADDRESS, sStatus.uiExitCode);
	cSDCard.RemoveCard(TEST_HCP65_CARD_SOCKET);
	cFileUtil.Delete(TEST_HCP65_MISPLACED_CARD);

	cHCP65.Kill();
	cPort.Kill();
	cStray.Kill();
	cSDCard.Kill();
	cAudio2.Kill();
	cAudio.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Copies the blank SD card image to Output/UARTCard.img and puts the UART
// Board's test program (HCP65/UARTTest) on it as \boot.exe and its driver
// (HCP65/Drivers/UARTBoard) as \OS\Drivers\UART.DRV.
//
//////////////////////////////////////////////////////////////////////////
static bool TestKernalPutUARTFiles(void)
{
	if (!TestKernalCopyBlankCard(TEST_HCP65_UART_CARD_IMAGE))
	{
		return false;
	}
	if (!TestKernalPutExe(TEST_HCP65_UART_CARD_IMAGE, "\\boot.exe", TEST_HCP65_UART_PROGRAM, NULL, HCP65_EXE_DEFAULT_HEAP, false))
	{
		return false;
	}
	return TestKernalPutDriver(TEST_HCP65_UART_CARD_IMAGE, TEST_HCP65_UART_DRIVER_PATH, TEST_HCP65_UART_DRIVER);
}


//////////////////////////////////////////////////////////////////////////
//
// UARTTest.cpp's patterns: uiLength bytes from uiFirst, uiStep more each.
//
//////////////////////////////////////////////////////////////////////////
static void TestKernalUARTPattern(uint8* puiDest, uint32 uiLength, uint8 uiFirst, uint8 uiStep)
{
	uint32	i;

	for (i = 0; i < uiLength; i++)
	{
		puiDest[i] = uiFirst;
		uiFirst = (uint8)(uiFirst + uiStep);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Boots the kernal ROM, at the real board's 25MHz, with the SD Card Board
// (holding Output/UARTCard.img) and the UART Board, whose two ports'
// far ends the test plays.  UARTTest.cpp, as boot.exe, checks the driver
// and exits 0 if it all did what it should:
//  - Both ports open, the board shows open, and the driver refuses what it
//    doesn't take.  Nothing has arrived, so a read takes nothing.
//  - The TTL port in loopback gets back what it sends.
//  - With the RS232 port closed, a child process (boot.exe again) can't
//    open the TTL port but can open the RS232 port; it sends "CHILD" and
//    exits without closing it, and the kernal lets it go.
//  - The RS232 port with RTS/CTS sends 1000 bytes in one write, then the
//    far end sends it 2000, which it takes a full buffer at a time: the
//    UART turns RTS off and the far end waits.
//  - The TTL port at 9600 7E1 echoes 50 bytes xor $20.
//
//////////////////////////////////////////////////////////////////////////
void TestHCP65KernalUART(CHCP65KernalBuilder* pcBuilder)
{
	CHCP65				cHCP65;
	CHCP65SDCard		cSDCard;
	CHCP65UARTBoard		cUART;
	CHCP65DebugPort		cPort;
	CHCP65UART*			pcTTL;
	CHCP65UART*			pcRS232;
	EDebugProcessEvent	eEvent;
	SDebugProcess		sProcess;
	STestKernalStatus	sStatus;
	CArrayUint8			acFromRS232;
	CArrayUint8			acFromTTL;
	uint8				auiBuffer[0x400];
	uint8				auiExpected[TEST_UART_RECEIVED];
	uint8*				puiRAM;
	uint32				uiStatus;
	size				uiCount;
	int					i;
	bool				bSentRS232;
	bool				bSentTTL;

	if (!TestKernalPutUARTFiles())
	{
		return;
	}

	//boot.exe's argument is 0: it is the parent.
	TestKernalSetBootArgument(pcBuilder, 0, &uiStatus);

	cHCP65.Init(TEST_UART_CLOCK_RATE);
	cSDCard.Init();
	cUART.Init();
	cPort.Init();
	AssertTrue(cSDCard.InsertCard(TEST_HCP65_UART_CARD_IMAGE, false, TEST_HCP65_CARD_SOCKET));
	AssertTrue(cHCP65.SetDevice(TEST_HCP65_SD_SOCKET, &cSDCard));
	AssertTrue(cHCP65.SetDevice(TEST_HCP65_UART_SOCKET, &cUART));
	TestKernalStart(&cHCP65, &cPort, pcBuilder);

	//The far ends: the RS232 port's has RTS and DTR on (its CTS and DSR) and DCD off; the TTL port's has RTS on.
	pcTTL = cUART.GetTTL();
	pcRS232 = cUART.GetRS232();
	pcRS232->HostSetCTS(true);
	pcRS232->HostSetDSR(true);
	pcTTL->HostSetCTS(true);

	acFromRS232.Init();
	acFromTTL.Init();
	bSentRS232 = false;
	bSentTTL = false;
	puiRAM = cHCP65.GetMPURAM();
	for (i = 0; i < TEST_UART_MAX_RUNS; i++)
	{
		cHCP65.Run(TEST_UART_RUN_CYCLES);

		do
		{
			uiCount = pcRS232->HostReceive(auiBuffer, sizeof(auiBuffer));
			acFromRS232.Add(auiBuffer, uiCount);
		}
		while (uiCount > 0);
		do
		{
			uiCount = pcTTL->HostReceive(auiBuffer, sizeof(auiBuffer));
			acFromTTL.Add(auiBuffer, uiCount);
		}
		while (uiCount > 0);

		//Once the RS232 port has had "CHILD" and the first pattern, the far end sends the second.  The TTL port's far
		//end sends once the port is at 9600 7E1.
		if (!bSentRS232 && (acFromRS232.NumElements() >= 5 + TEST_UART_SENT))
		{
			TestKernalUARTPattern(auiExpected, TEST_UART_RECEIVED, 5, 13);
			pcRS232->HostSend(auiExpected, TEST_UART_RECEIVED);
			bSentRS232 = true;
		}
		if (!bSentTTL && (pcTTL->GetBaudRate() == 9600) && ((pcTTL->GetLineControl() & 0x3F) == 0x1A))
		{
			TestKernalUARTPattern(auiBuffer, TEST_UART_ECHOES, 0x41, 1);
			pcTTL->HostSend(auiBuffer, TEST_UART_ECHOES);
			bSentTTL = true;
		}
		cHCP65.UpdateInterrupts();

		TestKernalReadStatus(puiRAM, uiStatus, &sStatus);
		if (getenv("TEST_HCP65_VERBOSE"))
		{
			printf("uart run %d: state %d, exit %d, process $%02X, PC $%06X, RS232 %d in %d to send, TTL %d in\n", i, sStatus.uiState, sStatus.uiExitCode, cHCP65.GetProcessID(), cHCP65.GetMPU()->GetProgramAddress(), (int)acFromRS232.NumElements(), (int)pcRS232->NumHostToSend(), (int)acFromTTL.NumElements());
			fflush(stdout);
		}
		if (((sStatus.uiState >= TEST_KERNAL_STATE_BOOT_FAILED) && (sStatus.uiState <= TEST_KERNAL_STATE_BOOT_EXITED)) || ((sStatus.uiState > TEST_KERNAL_STATE_DEVICES_SCANNED) && (sStatus.uiState < TEST_KERNAL_STATE_MOUNTED)) || cHCP65.IsStopped())
		{
			break;
		}
	}

	//boot.exe did what UARTTest.cpp checks.
	AssertFalse(cHCP65.IsStopped());
	AssertInt(TEST_KERNAL_STATE_BOOT_EXITED, sStatus.uiState);
	AssertInt(TEST_KERNAL_PROCESS_EXITED, sStatus.uiBootResult);
	AssertInt(0, sStatus.uiExitCode);

	//The driver was loaded from the card for the UART Board, after the kernal.
	AssertInt((1 << TEST_HCP65_SD_SOCKET) | (1 << TEST_HCP65_UART_SOCKET) | (1 << TEST_HCP65_DEBUG_SOCKET), sStatus.uiDevices);
	AssertInt(0, sStatus.uiBadDevices);
	AssertInt(0, sStatus.uiDriverError);
	AssertInt(1 << TEST_HCP65_UART_SOCKET, sStatus.uiLoadedDrivers);
	AssertSize(1, cPort.NumDrivers());
	AssertString(TEST_HCP65_UART_DRIVER_PATH, cPort.GetDriver(0)->szName);
	AssertInt(TEST_HCP65_UART_SOCKET, cPort.GetDriver(0)->uiSlot);

	//The processes: boot.exe ($FF) ran itself as the child ($FE), which exited 0.
	AssertTrue(cPort.TakeEvent(&eEvent, &sProcess));
	AssertInt(DPE_Started, eEvent);
	AssertInt(0xFF, sProcess.uiProcess);
	AssertTrue(cPort.TakeEvent(&eEvent, &sProcess));
	AssertInt(DPE_Started, eEvent);
	AssertInt(0xFE, sProcess.uiProcess);
	AssertString("\\boot.exe", sProcess.szName);
	AssertTrue(cPort.TakeEvent(&eEvent, &sProcess));
	AssertInt(DPE_Ended, eEvent);
	AssertInt(0xFE, sProcess.uiProcess);
	AssertInt(0, sProcess.iExitCode);
	AssertTrue(cPort.TakeEvent(&eEvent, &sProcess));
	AssertInt(DPE_Ended, eEvent);
	AssertInt(0xFF, sProcess.uiProcess);
	AssertFalse(cPort.TakeEvent(&eEvent, &sProcess));

	//The RS232 far end got "CHILD" (sent before the kernal closed the child's port) then the first pattern, and sent all
	//of the second, held off by RTS on the way.  Nothing was lost on either port.
	AssertTrue(bSentRS232);
	AssertTrue(pcRS232->NumHoldOffs() > 0);
	AssertSize(0, pcRS232->NumHostToSend());
	AssertSize(5 + TEST_UART_SENT, acFromRS232.NumElements());
	if (acFromRS232.NumElements() == 5 + TEST_UART_SENT)
	{
		AssertTrue(memcmp(acFromRS232.GetData(), "CHILD", 5) == 0);
		TestKernalUARTPattern(auiExpected, TEST_UART_SENT, 3, 7);
		AssertTrue(memcmp(acFromRS232.GetData() + 5, auiExpected, TEST_UART_SENT) == 0);
	}
	AssertLong(0, (int64)pcRS232->NumOverruns());

	//The TTL far end got its 50 bytes back xor $20 ("LOOP" stayed in the UART).
	AssertTrue(bSentTTL);
	AssertSize(0, pcTTL->NumHostToSend());
	AssertSize(TEST_UART_ECHOES, acFromTTL.NumElements());
	if (acFromTTL.NumElements() == TEST_UART_ECHOES)
	{
		for (i = 0; i < TEST_UART_ECHOES; i++)
		{
			auiExpected[i] = (uint8)((0x41 + i) ^ 0x20);
		}
		AssertTrue(memcmp(acFromTTL.GetData(), auiExpected, TEST_UART_ECHOES) == 0);
	}
	AssertLong(0, (int64)pcTTL->NumOverruns());

	//Closed, the ports are back at reset: outputs off, 115200.
	AssertFalse(pcRS232->IsRTSOn());
	AssertFalse(pcRS232->IsDTROn());
	AssertInt(115200, pcTTL->GetBaudRate());

	acFromTTL.Kill();
	acFromRS232.Kill();
	cSDCard.RemoveCard(TEST_HCP65_CARD_SOCKET);
	cHCP65.Kill();
	cPort.Kill();
	cUART.Kill();
	cSDCard.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void TestHCP65Kernal(void)
{
	CHCP65KernalBuilder	cBuilder;

	BeginTests();

	cBuilder.Init();
	if (TestKernalBuild(&cBuilder))
	{
		TestHCP65KernalBoot(&cBuilder);
		TestHCP65KernalUART(&cBuilder);
	}
	cBuilder.Kill();

	TestStatistics();
}
