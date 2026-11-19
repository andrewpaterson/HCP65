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
#include "BaseLib/ConstructorCall.h"
#include "BaseLib/DataMemory.h"
#include "Assembler65Lib/Asm65Object.h"
#include "StandardLib/Objects.h"
#include "SupportLib/MODReader.h"
#include "SupportLib/ModPlayer.h"
#include "SupportLib/ImageReader.h"
#include "SupportLib/ImageCel.h"
#include "SupportLib/SFTWriter.h"
#include "HCP65Lib/HCP65.h"
#include "HCP65Lib/HCP65DebugPort.h"
#include "HCP65Lib/HCP65Audio.h"
#include "HCP65Lib/HCP65SDCard.h"
#include "HCP65Lib/HCP65UART.h"
#include "HCP65Lib/HCP65VIA.h"
#include "HCP65Lib/HCP65PS2.h"
#include "HCP65Lib/HCP65PS2Keyboard.h"
#include "HCP65Lib/HCP65Blitter.h"
#include "HCP65Lib/HCP65Graphics.h"
#include "HCP65Lib/HCP65LCD.h"
#include "HCP65Lib/HCP65Terminal.h"
#include "HCP65Lib/HCP65Video.h"
#include "HCP65Lib/HCP65Mainboard.h"
#include "TestLib/Assert.h"
#include "HCP65KernalBuilder.h"
#include "HCP65ExeBuilder.h"
#include "HCP65DriverBuilder.h"
#include "../../../ModPlay/ModPlaySamples.h"


//Relative to the test's directory (TestHCP65Lib/TestHCP65Lib), as Visual Studio runs it.
#define TEST_HCP65_COMMON			".." _FS_ ".." _FS_ ".." _FS_ ".." _FS_ "CommonLibraries" _FS_ "Libraries"
#define TEST_HCP65_LIBRARIES		".." _FS_ ".." _FS_ ".." _FS_ "Libraries"
#define TEST_HCP65_KERNAL			".." _FS_ ".." _FS_ ".." _FS_ "Kernal"
#define TEST_HCP65_BOOT				".." _FS_ ".." _FS_ ".." _FS_ "Boot"
#define TEST_HCP65_INPUT			".." _FS_ ".." _FS_ "HCP65Emulator" _FS_ "Input"
#define TEST_HCP65_BLANK_SD_CARD	TEST_HCP65_INPUT _FS_ "SDCard.img"	//Freshly formatted; never written.
#define TEST_HCP65_OUTPUT			"Output"
#define TEST_HCP65_BOOT_CARD_IMAGE	TEST_HCP65_OUTPUT _FS_ "BootCard.img"	//TestHCP65KernalBoot's (and ModPlay's, to debug it on).
#define TEST_HCP65_SYSTEM_CARD_IMAGE	TEST_HCP65_OUTPUT _FS_ "SDCard.img"	//The emulator boots this and Boot.rom (HCP65Emulator.h).
#define TEST_HCP65_BOOT_ROM			TEST_HCP65_OUTPUT _FS_ "Boot.rom"
#define TEST_HCP65_OS_FILE			TEST_HCP65_OUTPUT _FS_ "OS.SYS"		//What each card has as \OS\OS.SYS.
#define TEST_HCP65_STARTUP_PATH		"\\OS\\STARTUP.TXT"
#define TEST_HCP65_USER				".." _FS_ ".." _FS_ ".." _FS_ "Libraries" _FS_ "UserLib"
#define TEST_HCP65_BOOT_PROGRAM		".." _FS_ ".." _FS_ ".." _FS_ "BootTest" _FS_ "HCP65BootTest.vcxproj"
#define TEST_HCP65_BOOT_PROGRAM_PATH	"\\boottest.exe"
#define TEST_HCP65_SOUND_DRIVER		".." _FS_ ".." _FS_ ".." _FS_ "Drivers" _FS_ "SoundBoard" _FS_ "HCP65SoundBoard.vcxproj"
#define TEST_HCP65_SOUND_DRIVER_PATH	"\\OS\\Drivers\\SOUND.DRV"
#define TEST_HCP65_MODPLAY_PROGRAM	".." _FS_ ".." _FS_ ".." _FS_ "ModPlay" _FS_ "HCP65ModPlay.vcxproj"
#define TEST_HCP65_SONG				TEST_HCP65_INPUT _FS_ "SpaceDebris.mod"
#define TEST_HCP65_MATH_TABLES		TEST_HCP65_LIBRARIES _FS_ "Libc65" _FS_ "MathTbls"
#define TEST_HCP65_UART_CARD_IMAGE	TEST_HCP65_OUTPUT _FS_ "UARTCard.img"	//HCP65UARTTest.hcp65build's card.
#define TEST_HCP65_UART_PROGRAM		".." _FS_ ".." _FS_ ".." _FS_ "UARTTest" _FS_ "HCP65UARTTest.vcxproj"
#define TEST_HCP65_UART_PROGRAM_PATH	"\\uarttest.exe"
#define TEST_HCP65_UART_DRIVER		".." _FS_ ".." _FS_ ".." _FS_ "Drivers" _FS_ "UARTBoard" _FS_ "HCP65UARTBoard.vcxproj"
#define TEST_HCP65_UART_DRIVER_PATH	"\\OS\\Drivers\\UART.DRV"
#define TEST_HCP65_VIA_CARD_IMAGE	TEST_HCP65_OUTPUT _FS_ "VIACard.img"	//HCP65VIATest.hcp65build's card.
#define TEST_HCP65_VIA_PROGRAM		".." _FS_ ".." _FS_ ".." _FS_ "VIATest" _FS_ "HCP65VIATest.vcxproj"
#define TEST_HCP65_VIA_PROGRAM_PATH	"\\viatest.exe"
#define TEST_HCP65_VIA_DRIVER		".." _FS_ ".." _FS_ ".." _FS_ "Drivers" _FS_ "VIABoard" _FS_ "HCP65VIABoard.vcxproj"
#define TEST_HCP65_VIA_DRIVER_PATH	"\\OS\\Drivers\\VIA.DRV"
#define TEST_HCP65_PS2_CARD_IMAGE	TEST_HCP65_OUTPUT _FS_ "PS2Card.img"	//HCP65PS2Test.hcp65build's card.
#define TEST_HCP65_PS2_PROGRAM		".." _FS_ ".." _FS_ ".." _FS_ "PS2Test" _FS_ "HCP65PS2Test.vcxproj"
#define TEST_HCP65_PS2_PROGRAM_PATH	"\\ps2test.exe"
#define TEST_HCP65_PS2_DRIVER		".." _FS_ ".." _FS_ ".." _FS_ "Drivers" _FS_ "PS2Board" _FS_ "HCP65PS2Board.vcxproj"
#define TEST_HCP65_PS2_DRIVER_PATH	"\\OS\\Drivers\\PS2.DRV"
#define TEST_HCP65_INIT_PROGRAM		".." _FS_ ".." _FS_ ".." _FS_ "Init" _FS_ "HCP65Init.vcxproj"
#define TEST_HCP65_INIT_PATH		"\\OS\\INIT.EXE"
#define TEST_HCP65_SHELL_PROGRAM	".." _FS_ ".." _FS_ ".." _FS_ "Shell" _FS_ "HCP65Shell.vcxproj"
#define TEST_HCP65_SHELL_PATH		"\\OS\\SHELL.EXE"
#define TEST_HCP65_SPRITES_PROGRAM	".." _FS_ ".." _FS_ ".." _FS_ "Sprites" _FS_ "HCP65Sprites.vcxproj"
#define TEST_HCP65_SPRITES_PATH		"\\SPRITES.EXE"
#define TEST_HCP65_SPRITES_HEAP		0x80000		//The stage and Maki's frames.
#define TEST_HCP65_SPRITES_IMAGES	".." _FS_ ".." _FS_ ".." _FS_ "Sprites" _FS_ "Images"
#define TEST_HCP65_STAGE_IMAGE		TEST_HCP65_SPRITES_IMAGES _FS_ "Fighting320.png"
#define TEST_HCP65_MAKI_STAND_IMAGE	TEST_HCP65_SPRITES_IMAGES _FS_ "MakiStand.png"
#define TEST_HCP65_MAKI_WALK_IMAGE	TEST_HCP65_SPRITES_IMAGES _FS_ "MakiWalk.png"
#define TEST_HCP65_MAKI_WALK_FRAMES	6			//Side by side in MakiWalk.png.
#define TEST_HCP65_STAGE_FILE		TEST_HCP65_OUTPUT _FS_ "STAGE.SFT"
#define TEST_HCP65_MAKI_FILE		TEST_HCP65_OUTPUT _FS_ "MAKI.SFT"
#define TEST_HCP65_STAGE_PATH		"\\SPRITES\\STAGE.SFT"
#define TEST_HCP65_MAKI_PATH		"\\SPRITES\\MAKI.SFT"
#define TEST_HCP65_SPRITES_SCALE	2			//The images are drawn twice as big as they are.
#define TEST_HCP65_LCD_PROGRAM		".." _FS_ ".." _FS_ ".." _FS_ "LCDTest" _FS_ "HCP65LCDTest.vcxproj"
#define TEST_HCP65_LCD_PROGRAM_PATH	"\\LCDTEST.EXE"
#define TEST_HCP65_HELLO_PATH		"\\HELLO.TXT"
#define TEST_HCP65_HELLO_TEXT		"Hello from the SD card.\n"
#define TEST_HCP65_SHELL_PROMPT		" $ "			//After the current directory.
#define TEST_HCP65_SHELL_BANNER		"HCP65 shell 1.  Type help for the commands."
#define TEST_HCP65_SHELL_MAX_RUNS	3000		//Of TEST_UART_RUN_CYCLES, for a command.
#define TEST_HCP65_FONT				".." _FS_ ".." _FS_ ".." _FS_ "Fonts" _FS_ "CONSOLE.SFT"
#define TEST_HCP65_FONT_PATH		"\\OS\\Fonts\\CONSOLE.SFT"
#define TEST_HCP65_FONT_SIZE		4096		//Its 256 glyphs, a row a byte, to read the screen with.
#define TEST_HCP65_SCREEN_COLUMNS	80			//The console's 640x480.
#define TEST_HCP65_SCREEN_ROWS		30
#define TEST_HCP65_CONSOLE_INK		0xFFAAAAAA	//Light grey on black.
#define TEST_HCP65_CONSOLE_PAPER	0xFF000000
#define TEST_HCP65_VIDEO_CARD_IMAGE	TEST_HCP65_OUTPUT _FS_ "VideoCard.img"	//The emulator's Machines/Video.xml boots it.
#define TEST_HCP65_VIDEO_PROGRAM	".." _FS_ ".." _FS_ ".." _FS_ "VideoTest" _FS_ "HCP65VideoTest.vcxproj"
#define TEST_HCP65_VIDEO_PROGRAM_PATH	"\\vidtest.exe"
#define TEST_HCP65_VIDEO_DRIVER		".." _FS_ ".." _FS_ ".." _FS_ "Drivers" _FS_ "VideoBoard" _FS_ "HCP65VideoBoard.vcxproj"
#define TEST_HCP65_VIDEO_DRIVER_PATH	"\\OS\\Drivers\\VIDEO.DRV"
#define TEST_HCP65_BLITTER_DRIVER	".." _FS_ ".." _FS_ ".." _FS_ "Drivers" _FS_ "BlitterBoard" _FS_ "HCP65BlitterBoard.vcxproj"
#define TEST_HCP65_BLITTER_DRIVER_PATH	"\\OS\\Drivers\\BLITTER.DRV"
#define TEST_HCP65_GRAPHICS_DRIVER	".." _FS_ ".." _FS_ ".." _FS_ "Drivers" _FS_ "GraphicsBoard" _FS_ "HCP65GraphicsBoard.vcxproj"
#define TEST_HCP65_GRAPHICS_DRIVER_PATH	"\\OS\\Drivers\\GRAPHICS.DRV"
#define TEST_HCP65_LCD_DRIVER		".." _FS_ ".." _FS_ ".." _FS_ "Drivers" _FS_ "LCDBoard" _FS_ "HCP65LCDBoard.vcxproj"
#define TEST_HCP65_LCD_DRIVER_PATH	"\\OS\\Drivers\\LCD.DRV"

//The test's machine is fast enough that modplay.exe never runs out of samples, so what it plays is exactly what the host
//renders.
#define TEST_HCP65_CLOCK_RATE		4000000000ULL
#define TEST_HCP65_RUN_CYCLES		20000000
#define TEST_HCP65_MAX_RUNS			2000
#define TEST_MODPLAY_SECONDS		1			//boottest.exe's argument, in the test's card's STARTUP.TXT.
//The tests' machines are Small Mainboards (sockets 3, 4 and 16 to 19), described in Machines; these say where the
//descriptions put the boards.
#define TEST_HCP65_MACHINES			"Machines"
#define TEST_HCP65_BOOT_MACHINE		TEST_HCP65_MACHINES _FS_ "Boot.xml"
#define TEST_HCP65_UART_MACHINE		TEST_HCP65_MACHINES _FS_ "UART.xml"
#define TEST_HCP65_VIA_MACHINE		TEST_HCP65_MACHINES _FS_ "VIA.xml"
#define TEST_HCP65_PS2_MACHINE		TEST_HCP65_MACHINES _FS_ "PS2.xml"
#define TEST_HCP65_SHELL_MACHINE	TEST_HCP65_MACHINES _FS_ "Shell.xml"
#define TEST_HCP65_CONSOLE_MACHINE	TEST_HCP65_MACHINES _FS_ "Console.xml"
#define TEST_HCP65_BLITTER_MACHINE	TEST_HCP65_MACHINES _FS_ "Blitter.xml"
#define TEST_HCP65_LCD_MACHINE		TEST_HCP65_MACHINES _FS_ "LCD.xml"
#define TEST_HCP65_VIDEO_MACHINE	TEST_HCP65_MACHINES _FS_ "Video.xml"
#define TEST_HCP65_NO_MEMORY_MACHINE	TEST_HCP65_MACHINES _FS_ "NoMainMemory.xml"
#define TEST_HCP65_STRAY_TYPE		"Test Stray Device"		//CTestMainboard's own boards.
#define TEST_HCP65_PRIVATE_TYPE		"Test Private Board"
#define TEST_HCP65_SD_SOCKET		4
#define TEST_HCP65_CARD_SOCKET		2			//The SD Card Board's socket with the volume; socket 0 has a card without one.
#define TEST_HCP65_BLANK_CARD_IMAGE	TEST_HCP65_OUTPUT _FS_ "BlankCard.img"	//Socket 0's card in the boot machine: zeros.
#define TEST_HCP65_BLANK_CARD_SIZE	0x100000
#define TEST_HCP65_STRAY_SOCKET		3			//A device with no driver that interrupts.
#define TEST_HCP65_AUDIO_SOCKET		16
#define TEST_HCP65_AUDIO2_SOCKET	17			//A second sound board, which the same driver drives.
#define TEST_HCP65_UART_SOCKET		16
#define TEST_HCP65_VIA_SOCKET		3
#define TEST_HCP65_PS2_SOCKET		3
#define TEST_HCP65_VIDEO_SOCKET		16
#define TEST_HCP65_BLITTER_SOCKET	17
#define TEST_HCP65_GRAPHICS_SOCKET	19
#define TEST_HCP65_LCD_SOCKET		17
#define TEST_HCP65_MEMORY_SOCKET	18			//The Memory Board (socket V): window 1, $400000 to $7FFFFF.
#define TEST_HCP65_PRIVATE_SOCKET	3			//A private board (socket X), scanned before the Memory Board.
#define TEST_HCP65_SOCKET_V			3			//The main memory channel's sockets, S to Z.
#define TEST_HCP65_SOCKET_X			5

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
#define TEST_KERNAL_STATE_NO_MAIN_MEMORY	9
#define TEST_KERNAL_STATE_NO_OS				10
#define TEST_KERNAL_STATE_OS_STARTED		11
#define TEST_KERNAL_STATUS_ADDRESS			0x00C000	//Kernal/Kernal.h's KERNAL_STATUS_ADDRESS.
#define TEST_KERNAL_MPU_HEAP_SIZE			0x10000		//Kernal/KernalMemory.h's.
#define TEST_KERNAL_PROCESS_EXITED			5
#define TEST_KERNAL_PROCESS_BAD_ADDRESS		7
#define TEST_HCP65_MISPLACED_CARD		TEST_HCP65_OUTPUT _FS_ "Misplaced.img"	//The card with a misplaced modplay.exe.
#define TEST_HCP65_MISPLACED_AT			0x080000	//In the small heap's banks.
#define TEST_KERNAL_FAT32					3


//The two builds the tests boot: the boot loader's ROM and the kernal, OS.SYS, which every card has.
struct STestKernal
{
	CHCP65KernalBuilder	cBoot;
	CHCP65KernalBuilder	cOS;
};


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
	uint32	uiHeapPeak;
	uint32	uiMainHeapPeak;
	uint16	uiMainBanks;
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


//A board in a memory socket whose 4MB is its own (as a coprocessor board's is): its ROM says it provides windows A and B
//and is private.  It answers its window as the Memory Board does, so a process or the kernal's tables put there would
//change its memory from what reset left in it (HCP65_DEBUG_FILL_BYTE in fill mode, else zeros).
class CTestPrivateBoard : public CHCP65MemoryBoard
{
public:
	void	Init(void)
	{
		CHCP65MemoryBoard::Init();
		mcROM.Init("Private Board", "Tester", 0, DEVICE_BUS_32KB, DEVICE_MAIN_BUS_WINDOW_A | DEVICE_MAIN_BUS_WINDOW_B | DEVICE_MAIN_BUS_PRIVATE, "");
	}

	uint32	FirstChanged(uint8 uiReset)	//The offset of its first byte that isn't uiReset (chip A then B), 2 * HCP65_MEMORY_CHIP_SIZE if none.
	{
		uint8*	puiChip;
		uint32	i;
		int		iChip;

		for (iChip = 0; iChip < 2; iChip++)
		{
			puiChip = GetMemoryChip(iChip);
			for (i = 0; i < HCP65_MEMORY_CHIP_SIZE; i++)
			{
				if (puiChip[i] != uiReset)
				{
					return (uint32)iChip * HCP65_MEMORY_CHIP_SIZE + i;
				}
			}
		}
		return 2 * HCP65_MEMORY_CHIP_SIZE;
	}
};


//The tests' mainboards: the HCP65's boards and the tests' own (TEST_HCP65_STRAY_TYPE and TEST_HCP65_PRIVATE_TYPE).
class CTestMainboard : public CHCP65Mainboard
{
protected:
	CHCP65Device* CreateBoard(const char* szType, const char** pszType) override
	{
		CTestStrayDevice*	pcStray;
		CTestPrivateBoard*	pcPrivate;

		if (strcmp(szType, TEST_HCP65_STRAY_TYPE) == 0)
		{
			pcStray = NewMalloc<CTestStrayDevice>();
			pcStray->Init();
			*pszType = TEST_HCP65_STRAY_TYPE;
			return pcStray;
		}
		if (strcmp(szType, TEST_HCP65_PRIVATE_TYPE) == 0)
		{
			pcPrivate = NewMalloc<CTestPrivateBoard>();
			pcPrivate->Init();
			*pszType = TEST_HCP65_PRIVATE_TYPE;
			return pcPrivate;
		}
		return CHCP65Mainboard::CreateBoard(szType, pszType);
	}
};


//////////////////////////////////////////////////////////////////////////
//
// The boards szMachine describes, plugged into pcHCP65.
//
//////////////////////////////////////////////////////////////////////////
static bool TestKernalPlugIn(CTestMainboard* pcMainboard, CHCP65* pcHCP65, const char* szMachine)
{
	CChars	szError;
	bool	bResult;

	pcMainboard->Init();
	szError.Init();
	bResult = pcMainboard->Read(szMachine, &szError);
	AssertString("", szError.Text());
	szError.Kill();
	if (bResult)
	{
		bResult = pcMainboard->PlugIn(pcHCP65);
		AssertTrue(bResult);
	}
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The board in iSocket, which must be of szType.
//
//////////////////////////////////////////////////////////////////////////
static CHCP65Device* TestKernalBoard(CTestMainboard* pcMainboard, int iSocket, const char* szType)
{
	const char*	szIn;

	szIn = pcMainboard->GetBoardType(iSocket);
	AssertString(szType, szIn ? szIn : "");
	return pcMainboard->GetBoard(iSocket);
}


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
	psStatus->uiHeapPeak = (uint32)p[52] | ((uint32)p[53] << 8) | ((uint32)p[54] << 16) | ((uint32)p[55] << 24);  //uint32s are 4 byte aligned.
	psStatus->uiMainHeapPeak = (uint32)p[56] | ((uint32)p[57] << 8) | ((uint32)p[58] << 16) | ((uint32)p[59] << 24);
	psStatus->uiMainBanks = (uint16)(p[60] | (p[61] << 8));
}


//////////////////////////////////////////////////////////////////////////
//
// Whether the start up has finished: the start up program ran (or
// couldn't), or the boot loader or the kernal stopped short (no main
// memory, no card, no volume, no OS.SYS).  Until the boot loader has
// cleared the status it is whatever MPU RAM held ($CC in fill mode, or
// what the last boot left before a reset).
//
//////////////////////////////////////////////////////////////////////////
static bool TestKernalIsDone(STestKernalStatus* psStatus)
{
	if ((psStatus->uiState >= TEST_KERNAL_STATE_BOOT_FAILED) && (psStatus->uiState <= TEST_KERNAL_STATE_NO_OS))
	{
		return true;
	}
	return (psStatus->uiState > TEST_KERNAL_STATE_DEVICES_SCANNED) && (psStatus->uiState < TEST_KERNAL_STATE_MOUNTED);
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
// card image szImage as szName, and in Output by its file name (to compare
// with hcp65build's: Drivers\<Board>\<Board>.sln builds the same bytes).
//
//////////////////////////////////////////////////////////////////////////
static bool TestKernalPutDriver(const char* szImage, const char* szName, const char* szProjectFile)
{
	CHCP65DriverBuilder	cDriver;
	bool				bBuilt;
	size				i;
	FILE*				pFile;
	CChars				szCopy;
	const char*			szFile;

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
		szFile = strrchr(szName, '\\');
		szCopy.Init(TEST_HCP65_OUTPUT _FS_);
		szCopy.Append(szFile ? szFile + 1 : szName);
		AssertTrue(cDriver.Write(szCopy.Text()));
		szCopy.Kill();
	}
	cDriver.Kill();
	return bBuilt;
}


//////////////////////////////////////////////////////////////////////////
//
// A 24 bit address in OS.SYS at uiAddress (a .long: the address and a
// zero byte).
//
//////////////////////////////////////////////////////////////////////////
static uint32 TestKernalImageLong(CHCP65KernalBuilder* pcOS, uint32 uiAddress)
{
	uint8*	puiLong;

	puiLong = pcOS->GetImageAt(uiAddress);
	if (!puiLong)
	{
		return 0;
	}
	return (uint32)puiLong[0] | ((uint32)puiLong[1] << 8) | ((uint32)puiLong[2] << 16);
}


//////////////////////////////////////////////////////////////////////////
//
// The kernal's exports (Kernal/KernalExports.asm) as OS.SYS has them:
// each name is the symbol at its address in the kernal's link, so a
// driver that imports the name gets that symbol.
//
//////////////////////////////////////////////////////////////////////////
static void TestKernalExports(STestKernal* psKernal)
{
	uint8*	puiCount;
	uint32	uiTable;
	uint32	uiCount;
	uint32	uiAddress;
	uint32	uiSymbol;
	char*	szName;
	CChars	szWrong;
	uint32	uiNumExports;
	uint32	i;

	AssertTrue(psKernal->cOS.GetSymbol("_gauiKernalExports", &uiTable));
	AssertTrue(psKernal->cOS.GetSymbol("_guiKernalNumExports", &uiCount));
	puiCount = psKernal->cOS.GetImageAt(uiCount);
	AssertTrue(puiCount != NULL);
	uiNumExports = puiCount ? ((uint32)puiCount[0] | ((uint32)puiCount[1] << 8)) : 0;
	AssertTrue(uiNumExports > 0);

	//The count (which the assembler works out) is right after the table: it counts all of it.
	AssertInt(uiTable + uiNumExports * 8, uiCount);

	szWrong.Init();
	for (i = 0; i < uiNumExports; i++)
	{
		szName = (char*)psKernal->cOS.GetImageAt(TestKernalImageLong(&psKernal->cOS, uiTable + i * 8));
		uiAddress = TestKernalImageLong(&psKernal->cOS, uiTable + i * 8 + 4);
		if (!szName || !psKernal->cOS.GetSymbol(szName, &uiSymbol) || (uiSymbol != uiAddress))
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
// Output/szImage, a copy of the emulator's blank SD card image, with the
// kernal on it (Output/OS.SYS as \OS\OS.SYS) for the boot loader to load.
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
	bResult = bResult && FatPutDiskFileOnImage(szImage, HCP65_OS_PATH, TEST_HCP65_OS_FILE);
	AssertTrue(bResult);
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Makes szProgram, with uiArgument, the start up program of the SD card
// image szImage (its \OS\STARTUP.TXT), and gives where the status is
// (the boot loader's and then the kernal's: KERNAL_STATUS_ADDRESS).
//
//////////////////////////////////////////////////////////////////////////
static void TestKernalSetStartUp(const char* szImage, const char* szProgram, uint16 uiArgument, uint32* puiStatus)
{
	char	szLine[128];

	snprintf(szLine, sizeof(szLine), "%s %u\r\n", szProgram, (unsigned int)uiArgument);
	AssertTrue(FatPutFileOnImage(szImage, TEST_HCP65_STARTUP_PATH, (uint8*)szLine, strlen(szLine)));
	*puiStatus = TEST_KERNAL_STATUS_ADDRESS;
}


//////////////////////////////////////////////////////////////////////////
//
// Watches the debug port, loads the boot loader's ROM and resets the machine,
// its boards in place.  Fill mode: memory nothing wrote reads $CC (the
// kernal doesn't clear the banks it gives processes), so a program that
// relied on zeroed memory would fail.
//
//////////////////////////////////////////////////////////////////////////
static void TestKernalStart(CHCP65* pcHCP65, STestKernal* psKernal)
{
	pcHCP65->GetDebugPort()->Watch(true);
	AssertTrue(pcHCP65->LoadROM(psKernal->cBoot.GetImage(), HCP65_ROM_SIZE));
	pcHCP65->SetMemoryFill(true);
	pcHCP65->Reset();
}


//////////////////////////////////////////////////////////////////////////
//
// The maths tables (\OS\Math\MULT.BIN and INVERT.BIN, from MathTbls' HEX
// files) on the SD card image szImage.
//
//////////////////////////////////////////////////////////////////////////
static bool TestKernalPutTables(const char* szImage)
{
	uint8*	puiTables;
	bool	bResult;

	puiTables = (uint8*)malloc(HCP65_MATH_MULT_SIZE + HCP65_MATH_INVERT_SIZE);
	bResult = TestKernalReadTables(puiTables);
	bResult = bResult && FatPutFileOnImage(szImage, HCP65_MATH_MULT_PATH, puiTables, HCP65_MATH_MULT_SIZE);
	bResult = bResult && FatPutFileOnImage(szImage, HCP65_MATH_INVERT_PATH, &puiTables[HCP65_MATH_MULT_SIZE], HCP65_MATH_INVERT_SIZE);
	free(puiTables);
	AssertTrue(bResult);
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Copies the blank SD card image to Output and puts boottest.exe (HCP65/BootTest),
// modplay.exe (HCP65/ModPlay), the sound board's driver (HCP65/Drivers/
// SoundBoard, \OS\Drivers\SOUND.DRV), the UART Board's (for the
// emulator, which has one), the song modplay plays and the maths tables
// (\OS\Math\MULT.BIN and INVERT.BIN, from MathTbls' HEX files) on the copy.
// Writes Output/BlankCard.img, the boot machine's card without a volume.
//
//////////////////////////////////////////////////////////////////////////
static bool TestKernalPutFiles(void)
{
	static const char*	aszModPlayLibraries[] = { "StandardLib", "SupportLib", NULL };
	uint8*				puiBlank;
	FILE*				pFile;
	bool				bResult;

	if (!TestKernalCopyBlankCard(TEST_HCP65_BOOT_CARD_IMAGE))
	{
		return false;
	}

	//The boot machine's card without a volume.
	puiBlank = (uint8*)calloc(1, TEST_HCP65_BLANK_CARD_SIZE);
	pFile = fopen(TEST_HCP65_BLANK_CARD_IMAGE, "wb");
	bResult = pFile && (fwrite(puiBlank, 1, TEST_HCP65_BLANK_CARD_SIZE, pFile) == TEST_HCP65_BLANK_CARD_SIZE);
	bResult = pFile && (fclose(pFile) == 0) && bResult;
	free(puiBlank);
	AssertTrue(bResult);
	if (!bResult)
	{
		return false;
	}

	//boottest.exe loads the maths tables, so it is built without them; modplay.exe uses them.
	if (!TestKernalPutExe(TEST_HCP65_BOOT_CARD_IMAGE, TEST_HCP65_BOOT_PROGRAM_PATH, TEST_HCP65_BOOT_PROGRAM, NULL, HCP65_EXE_DEFAULT_HEAP, false))
	{
		return false;
	}
	if (!TestKernalPutExe(TEST_HCP65_BOOT_CARD_IMAGE, "\\modplay.exe", TEST_HCP65_MODPLAY_PROGRAM, aszModPlayLibraries, MODPLAY_HEAP_SIZE, true))
	{
		return false;
	}
	if (!TestKernalPutDriver(TEST_HCP65_BOOT_CARD_IMAGE, TEST_HCP65_SOUND_DRIVER_PATH, TEST_HCP65_SOUND_DRIVER))
	{
		return false;
	}
	if (!TestKernalPutDriver(TEST_HCP65_BOOT_CARD_IMAGE, TEST_HCP65_UART_DRIVER_PATH, TEST_HCP65_UART_DRIVER))
	{
		return false;
	}

	if (!TestKernalPutTables(TEST_HCP65_BOOT_CARD_IMAGE))
	{
		return false;
	}

	bResult = FatPutDiskFileOnImage(TEST_HCP65_BOOT_CARD_IMAGE, MODPLAY_FILE, TEST_HCP65_SONG);
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
// Builds one of the two images, which must link with nothing stubbed (it
// is built without the maths tables, so nothing it links may read
// MathMult or MathInvert), and writes it to Output as szFile.  szMap is
// the link map's file, written when TEST_HCP65_MAP is set.
//
//////////////////////////////////////////////////////////////////////////
static bool TestKernalBuildImage(CHCP65KernalBuilder* pcBuilder, const char* szFile, const char* szMap)
{
	CChars	szMissing;
	FILE*	pFile;
	bool	bBuilt;

	bBuilt = pcBuilder->Build(TEST_HCP65_COMMON, TEST_HCP65_LIBRARIES, TEST_HCP65_KERNAL, TEST_HCP65_BOOT);
	AssertString("", pcBuilder->GetErrors());
	AssertTrue(bBuilt);
	if (!bBuilt)
	{
		return false;
	}
	if (getenv("TEST_HCP65_MAP"))
	{
		//The link map, to find what is at an address.
		pFile = fopen(szMap, "w");
		if (pFile)
		{
			fputs(pcBuilder->GetBuilder()->GetLinker()->GetMap(), pFile);
			fclose(pFile);
		}
	}
	szMissing.Init();
	pcBuilder->GetBuilder()->GetMissingNames(&szMissing);
	AssertString("", szMissing.Text());
	szMissing.Kill();
	AssertTrue(pcBuilder->Write(szFile));
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Builds the boot loader's ROM and the kernal, and writes them to
// Output/Boot.rom (which the emulator boots) and Output/OS.SYS (which
// TestKernalCopyBlankCard puts on every card).  The boot loader runs from
// banks 4 and 5, out of the kernal's way; the kernal's exports are right
// and its stubs and vectors are OS.SYS's first page, with ResetStub (its
// entry, a jml) first.
//
//////////////////////////////////////////////////////////////////////////
static bool TestKernalBuild(STestKernal* psKernal)
{
	CAsm65LineTable*	pcLines;
	SAsm65LineEntry*	psLine;
	uint32				uiAddress;

	if (!TestKernalBuildImage(&psKernal->cBoot, TEST_HCP65_BOOT_ROM, TEST_HCP65_OUTPUT _FS_ "Boot.map") ||
		!TestKernalBuildImage(&psKernal->cOS, TEST_HCP65_OS_FILE, TEST_HCP65_OUTPUT _FS_ "Kernal.map"))
	{
		return false;
	}
	AssertTrue(psKernal->cBoot.GetCodeEnd() > HCP65_BOOT_TEXT);
	AssertTrue(psKernal->cBoot.GetCodeEnd() <= HCP65_BOOT_END);
	AssertTrue(psKernal->cBoot.GetSymbol("_main", &uiAddress));
	AssertTrue(uiAddress >= HCP65_BOOT_TEXT);
	AssertTrue(psKernal->cOS.GetCodeEnd() > 0x10000);
	AssertTrue(psKernal->cOS.GetCodeEnd() <= HCP65_OS_CODE_END);
	AssertInt(0x5C, psKernal->cOS.GetImage()[0]);
	AssertInt(HCP65_OS_SIZE, psKernal->cOS.GetImageSize());
	TestKernalExports(psKernal);

	//Start.asm's lines are in the line table, as the C++ units' are, so a debugger can stop at them: the native
	//vectors at $FFE4 are its data.
	pcLines = psKernal->cOS.GetBuilder()->GetLineTable();
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
// Boots the boot loader's ROM with the sound boards and the SD Card
// Board (holding Output/SDCard.img, a copy of the emulator's blank
// Input/SDCard.img with OS.SYS, \boottest.exe as the start up program,
// modplay.exe, Space Debris and the maths tables on it, not written back)
// and checks that the boot loader loaded OS.SYS and the kernal found the
// boards, mounted the FAT32 volume and ran boottest.exe in user mode,
// which loaded the maths tables to main memory and ran modplay.exe as a
// second process: it played the first TEST_MODPLAY_SECONDS of the song
// through the sound board as CModPlayer does on the host.  The emulator
// boots Output/Boot.rom (TestKernalBuild's) and Output/SDCard.img (where
// boottest.exe's argument is zero: modplay.exe plays the whole song).
// Then boots a copy of the card whose modplay.exe is where its heap would
// be, which the kernal refuses to load.
//
//////////////////////////////////////////////////////////////////////////
void TestHCP65KernalBoot(STestKernal* psKernal)
{
	CHCP65				cHCP65;
	CTestMainboard		cMainboard;
	CHCP65Audio*		pcAudio;
	CHCP65Audio*		pcAudio2;
	SHCP65DebugDriver	sDriver;
	uint32				uiKernalEnd;
	CHCP65SDCard*		pcSDCard;
	CTestStrayDevice*	pcStray;
	CHCP65DebugPort*	pcPort;
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

	//boottest.exe's argument is the seconds modplay.exe plays.
	//TEST_MODPLAY_SECONDS, TEST_HCP65_RUN_CYCLES and TEST_HCP65_CLOCK (environment) override them, to time modplay.exe
	//(at the board's 25000000 it must keep the sound board fed, or what it plays won't match).
	uiSeconds = getenv("TEST_MODPLAY_SECONDS") ? (uint32)atoi(getenv("TEST_MODPLAY_SECONDS")) : TEST_MODPLAY_SECONDS;
	uiRunCycles = getenv("TEST_HCP65_RUN_CYCLES") ? (uint64)atoll(getenv("TEST_HCP65_RUN_CYCLES")) : TEST_HCP65_RUN_CYCLES;
	TestKernalSetStartUp(TEST_HCP65_BOOT_CARD_IMAGE, TEST_HCP65_BOOT_PROGRAM_PATH, (uint16)uiSeconds, &uiStatus);

	cHCP65.Init(getenv("TEST_HCP65_CLOCK") ? (uint64)atoll(getenv("TEST_HCP65_CLOCK")) : TEST_HCP65_CLOCK_RATE);
	//The kernal brings up each socket's card and mounts the first volume it finds: socket 0's card has none.
	if (!TestKernalPlugIn(&cMainboard, &cHCP65, TEST_HCP65_BOOT_MACHINE))
	{
		cHCP65.Kill();
		cMainboard.Kill();
		return;
	}
	pcAudio = (CHCP65Audio*)TestKernalBoard(&cMainboard, TEST_HCP65_AUDIO_SOCKET, HCP65_AUDIO_NAME);
	pcAudio2 = (CHCP65Audio*)TestKernalBoard(&cMainboard, TEST_HCP65_AUDIO2_SOCKET, HCP65_AUDIO_NAME);
	pcSDCard = (CHCP65SDCard*)TestKernalBoard(&cMainboard, TEST_HCP65_SD_SOCKET, HCP65_SD_NAME);
	TestKernalBoard(&cMainboard, TEST_HCP65_MEMORY_SOCKET, HCP65_MEMORY_BOARD_NAME);
	pcStray = (CTestStrayDevice*)TestKernalBoard(&cMainboard, TEST_HCP65_STRAY_SOCKET, TEST_HCP65_STRAY_TYPE);
	pcPort = cHCP65.GetDebugPort();
	AssertInt(TEST_HCP65_BLANK_CARD_SIZE / 512, pcSDCard->NumSectors(0));
	TestKernalStart(&cHCP65, psKernal);

	//The sound board's frames are taken as they are played: it keeps no more than 64K of them.
	acPlayed.Init();
	puiRAM = cHCP65.GetMPURAM();
	for (i = 0; i < (int)(TEST_HCP65_MAX_RUNS * (TEST_HCP65_RUN_CYCLES / uiRunCycles)); i++)
	{
		cHCP65.Run(uiRunCycles);
		do
		{
			uiFrames = pcAudio->ReadFrames(auiFrames, 0x1000);
			TestKernalAppend(&acPlayed, auiFrames, uiFrames * 2);
		}
		while (uiFrames > 0);
		TestKernalReadStatus(puiRAM, uiStatus, &sStatus);

		//Once modplay plays (a process with interrupts on), the stray device is enabled and starts interrupting.  Modplay
		//spends nearly all its time in the kernal waiting for the sound board, so this doesn't wait for a run to end in
		//user mode: the dispatch resets the stray device whichever mode the interrupt finds the processor in.
		if (pcAudio->IsEnabled() && (cHCP65.GetProcessID() == 0xFE) && (cHCP65.GetDeviceEnables() & (1u << TEST_HCP65_STRAY_SOCKET)) == 0)
		{
			pcStray->Arm();
			cHCP65.SetDeviceEnables(cHCP65.GetDeviceEnables() | (1u << TEST_HCP65_STRAY_SOCKET));
			cHCP65.UpdateInterrupts();
		}
		if (getenv("TEST_HCP65_VERBOSE"))
		{
			printf("run %d: state %d (boot %d, exit %d, argument %d), process $%02X, depth %d, kernel %d, PC $%06X, sound %s, counter $%04X, frames %d\n", i, sStatus.uiState, sStatus.uiBootResult, sStatus.uiExitCode, sStatus.uiBootArgument, cHCP65.GetProcessID(), cHCP65.GetInterruptDepth(), cHCP65.IsKernelMode(), cHCP65.GetMPU()->GetProgramAddress(), pcAudio->IsEnabled() ? "on" : "off", pcAudio->GetCounter(), (int)(acPlayed.NumElements() / 2));
			fflush(stdout);
		}
		if (TestKernalIsDone(&sStatus) || cHCP65.IsStopped())
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
	AssertInt(pcSDCard->NumSectors(TEST_HCP65_CARD_SOCKET), sStatus.uiNumSectors);
	AssertInt((1 << 0) | (1 << TEST_HCP65_CARD_SOCKET), sStatus.uiSDCards);
	AssertInt(TEST_HCP65_CARD_SOCKET, sStatus.uiSDSocket);
	AssertTrue(sStatus.uiNumClusters >= 65525);
	AssertTrue(sStatus.uiFreeClusters <= sStatus.uiNumClusters);
	AssertTrue(pcSDCard->NumSectorsRead() > 0);
	AssertInt(0, pcSDCard->NumSectorsWritten());

	//The kernal never used main memory while the SD Card Board's DMA had it, and reset the stray device.
	AssertFalse(cHCP65.IsContention());
	AssertInt(HCP65_DEBUG_FILL_BYTE, *cHCP65.MainMemoryAt(0x7F0000));
	AssertInt(1 << TEST_HCP65_STRAY_SOCKET, sStatus.uiStrayInterrupts);

	//The scan found the boards (the memory board too) by their device ROMs, and the stray device's blank ROM unrecognised.
	AssertInt((1 << TEST_HCP65_SD_SOCKET) | (1 << TEST_HCP65_AUDIO_SOCKET) | (1 << TEST_HCP65_AUDIO2_SOCKET) | (1 << TEST_HCP65_MEMORY_SOCKET), sStatus.uiDevices);
	AssertInt(1 << TEST_HCP65_STRAY_SOCKET, sStatus.uiBadDevices);

	//The sound boards' driver was loaded from \OS\Drivers\SOUND.DRV once and drives both (the SD Card Board's is in the
	//ROM); modplay.exe played through the first, with the device calls.  The debug port was told
	//where the driver went, after the kernal, and (watched) stopped the run before the driver ran.
	AssertInt(0, sStatus.uiDriverError);
	AssertInt((1 << TEST_HCP65_AUDIO_SOCKET) | (1 << TEST_HCP65_AUDIO2_SOCKET), sStatus.uiLoadedDrivers);
	AssertSize(1, pcPort->NumDrivers());
	AssertTrue(psKernal->cOS.GetSymbol("__end", &uiKernalEnd));
	AssertString(TEST_HCP65_SOUND_DRIVER_PATH, pcPort->GetDriver(0)->szName);
	AssertInt(TEST_HCP65_AUDIO_SOCKET, pcPort->GetDriver(0)->uiSlot);
	AssertTrue(pcPort->GetDriver(0)->uiAddress >= uiKernalEnd);
	AssertTrue(pcPort->GetDriver(0)->uiSize > 0);
	AssertTrue(pcPort->GetDriver(0)->uiAddress + pcPort->GetDriver(0)->uiSize <= 0x070000);
	AssertTrue(pcPort->TakeDriverEvent(&sDriver));
	AssertInt(pcPort->GetDriver(0)->uiAddress, sDriver.uiAddress);
	AssertFalse(pcPort->TakeDriverEvent(&sDriver));
	AssertTrue(pcStray->IsQuiet());
	AssertInt(uiSeconds, (uint32)sStatus.uiBootArgument);

	//boottest.exe ran in user mode (process $FF), multiplied and divided with the tables, gave the kernal 300 * 200 / 3
	//and ran modplay.exe (process $FE), whose exit code it exited with; the kernal is back in kernel mode.
	AssertInt(TEST_KERNAL_PROCESS_EXITED, sStatus.uiBootResult);
	AssertInt(MODPLAY_EXIT_SUCCESS, sStatus.uiExitCode);

	//The kernal's heap (MPU RAM's bank 6) was enough, and its main memory heap held the SD card's bounce buffer.  The
	//Memory Board is window 1: 64 banks.
	AssertTrue(sStatus.uiHeapPeak > 0);
	AssertTrue(sStatus.uiHeapPeak < TEST_KERNAL_MPU_HEAP_SIZE);
	AssertTrue(sStatus.uiMainHeapPeak >= 512);
	AssertInt(64, sStatus.uiMainBanks);
	if (getenv("TEST_HCP65_VERBOSE"))
	{
		printf("kernal heap peak %u bytes, main memory heap peak %u bytes\n", (unsigned int)sStatus.uiHeapPeak, (unsigned int)sStatus.uiMainHeapPeak);
	}
	AssertInt(20000, sStatus.uiUserValue);
	AssertInt(0xFF, cHCP65.GetProcessID());
	AssertInt(1, cHCP65.GetInterruptDepth());

	//The kernal told the debug port of each process, in order: boottest.exe ($FF) started, boottest.exe again as the child that
	//checks it can't open the sound board ($FE) started and ended, modplay.exe ($FE) started and ended, then boottest.exe
	//ended with modplay.exe's exit code.  Each start stopped a Run, which went on in the next.
	AssertTrue(pcPort->TakeEvent(&eEvent, &sProcess));
	AssertInt(DPE_Started, eEvent);
	AssertInt(0xFF, sProcess.uiProcess);
	AssertString(TEST_HCP65_BOOT_PROGRAM_PATH, sProcess.szName);
	AssertTrue(pcPort->TakeEvent(&eEvent, &sProcess));
	AssertInt(DPE_Started, eEvent);
	AssertInt(0xFE, sProcess.uiProcess);
	AssertString(TEST_HCP65_BOOT_PROGRAM_PATH, sProcess.szName);
	AssertTrue(pcPort->TakeEvent(&eEvent, &sProcess));
	AssertInt(DPE_Ended, eEvent);
	AssertInt(0xFE, sProcess.uiProcess);
	AssertInt(0, sProcess.iExitCode);
	AssertTrue(pcPort->TakeEvent(&eEvent, &sProcess));
	AssertInt(DPE_Started, eEvent);
	AssertInt(0xFE, sProcess.uiProcess);
	AssertString("\\modplay.exe", sProcess.szName);
	AssertTrue(pcPort->TakeEvent(&eEvent, &sProcess));
	AssertInt(DPE_Ended, eEvent);
	AssertInt(0xFE, sProcess.uiProcess);
	AssertInt(MODPLAY_EXIT_SUCCESS, sProcess.iExitCode);
	AssertTrue(pcPort->TakeEvent(&eEvent, &sProcess));
	AssertInt(DPE_Ended, eEvent);
	AssertInt(0xFF, sProcess.uiProcess);
	AssertInt(sStatus.uiExitCode, sProcess.iExitCode);
	AssertFalse(pcPort->TakeEvent(&eEvent, &sProcess));
	AssertSize(0, pcPort->NumProcesses());

	//The drivers took the SD Card Board and both sound boards out of reset at boot (modplay.exe left the
	//first disabled; nothing used the second), and the test the stray device (the kernal's reset of it put its enable
	//back).
	AssertInt((1 << TEST_HCP65_SD_SOCKET) | (1 << TEST_HCP65_AUDIO_SOCKET) | (1 << TEST_HCP65_AUDIO2_SOCKET) | (1 << TEST_HCP65_STRAY_SOCKET), cHCP65.GetDeviceEnables());
	AssertFalse(pcAudio->IsEnabled());
	AssertFalse(pcAudio2->IsEnabled());

	//boottest.exe loaded MULT and INVERT's upper half (round(2^32 / $8000) first) from the SD card to main memory at
	//$400000, and the kernal was copied from ROM banks 0-3 to MPU RAM.  The processes' pages of the remap table were
	//cleared when they exited.
	puiMain = cHCP65.MainMemoryAt(HCP65_TABLES_START) - HCP65_TABLES_START;  //Chip A: $400000 to $5FFFFF, the tables' banks.
	puiTables = (uint8*)malloc(HCP65_MATH_MULT_SIZE + HCP65_MATH_INVERT_SIZE);
	AssertTrue(TestKernalReadTables(puiTables));
	AssertTrue(memcmp(puiTables, &puiMain[HCP65_TABLES_START], HCP65_MATH_MULT_SIZE) == 0);
	AssertTrue(memcmp(&puiTables[HCP65_MATH_MULT_SIZE + HCP65_MATH_INVERT_OFFSET], &puiMain[HCP65_TABLES_START + HCP65_MATH_MULT_SIZE], HCP65_TABLES_END - HCP65_TABLES_START - HCP65_MATH_MULT_SIZE) == 0);
	free(puiTables);
	AssertInt(15, puiMain[HCP65_TABLES_START + ((3 << 8) | 5) * 2]);
	AssertInt(0x020000, puiMain[HCP65_TABLES_START + HCP65_MATH_MULT_SIZE] | (puiMain[HCP65_TABLES_START + HCP65_MATH_MULT_SIZE + 1] << 8) | (puiMain[HCP65_TABLES_START + HCP65_MATH_MULT_SIZE + 2] << 16));
	puiROM = psKernal->cOS.GetImageAt(0x10000);
	AssertTrue(memcmp(puiROM, &puiRAM[0x10000], 0x100) == 0);
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
	AssertTrue(pcAudio->NumDroppedFrames() == 0);
	acExpected.Kill();
	acPlayed.Kill();

	//Booted again with a card whose modplay.exe has its code in the small heap's banks and no UserHeapSize: the kernal
	//refuses to load it (KERNAL_PROCESS_BAD_ADDRESS) and boottest.exe exits with 100 and why.  The card is a copy.
	acMisplaced.Init();
	AssertTrue(TestKernalMakeMisplacedExe(TEST_HCP65_MISPLACED_AT, &acMisplaced));
	AssertTrue(cFileUtil.Copy(TEST_HCP65_BOOT_CARD_IMAGE, TEST_HCP65_MISPLACED_CARD));
	AssertTrue(FatPutFileOnImage(TEST_HCP65_MISPLACED_CARD, "\\modplay.exe", acMisplaced.GetData(), acMisplaced.NumElements()));
	acMisplaced.Kill();
	pcSDCard->RemoveCard(TEST_HCP65_CARD_SOCKET);
	AssertTrue(pcSDCard->InsertCard(TEST_HCP65_MISPLACED_CARD, false, TEST_HCP65_CARD_SOCKET));
	cHCP65.Reset();
	for (i = 0; i < TEST_HCP65_MAX_RUNS; i++)
	{
		cHCP65.Run(TEST_HCP65_RUN_CYCLES);
		TestKernalReadStatus(puiRAM, uiStatus, &sStatus);
		if (TestKernalIsDone(&sStatus) || cHCP65.IsStopped())
		{
			break;
		}
	}
	AssertInt(TEST_KERNAL_STATE_BOOT_EXITED, sStatus.uiState);
	AssertInt(TEST_KERNAL_PROCESS_EXITED, sStatus.uiBootResult);
	AssertInt(100 + TEST_KERNAL_PROCESS_BAD_ADDRESS, sStatus.uiExitCode);
	pcSDCard->RemoveCard(TEST_HCP65_CARD_SOCKET);
	cFileUtil.Delete(TEST_HCP65_MISPLACED_CARD);

	//modplay.exe is debugged on this card (HCP65ModPlay.hcp65build): there boottest.exe's argument is zero, so it plays
	//the whole song.
	TestKernalSetStartUp(TEST_HCP65_BOOT_CARD_IMAGE, TEST_HCP65_BOOT_PROGRAM_PATH, 0, &uiStatus);

	cHCP65.Kill();
	cMainboard.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Copies the blank SD card image to Output/UARTCard.img and puts the UART
// Board's test program (HCP65/UARTTest) on it as \uarttest.exe and its driver
// (HCP65/Drivers/UARTBoard) as \OS\Drivers\UART.DRV.
//
//////////////////////////////////////////////////////////////////////////
static bool TestKernalPutUARTFiles(void)
{
	if (!TestKernalCopyBlankCard(TEST_HCP65_UART_CARD_IMAGE))
	{
		return false;
	}
	if (!TestKernalPutExe(TEST_HCP65_UART_CARD_IMAGE, TEST_HCP65_UART_PROGRAM_PATH, TEST_HCP65_UART_PROGRAM, NULL, HCP65_EXE_DEFAULT_HEAP, false))
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
// far ends the test plays.  UARTTest.cpp, as \uarttest.exe, checks the driver
// and exits 0 if it all did what it should:
//  - Both ports open, the board shows open, and the driver refuses what it
//    doesn't take.  Nothing has arrived, so a read takes nothing.
//  - The TTL port in loopback gets back what it sends.
//  - With the RS232 port closed, a child process (uarttest.exe again) can't
//    open the TTL port but can open the RS232 port; it sends "CHILD" and
//    exits without closing it, and the kernal lets it go.
//  - The RS232 port with RTS/CTS sends 1000 bytes in one write, then the
//    far end sends it 2000, which it takes a full buffer at a time: the
//    UART turns RTS off and the far end waits.
//  - The TTL port at 9600 7E1 echoes 50 bytes xor $20.
//
//////////////////////////////////////////////////////////////////////////
void TestHCP65KernalUART(STestKernal* psKernal)
{
	CHCP65				cHCP65;
	CTestMainboard		cMainboard;
	CHCP65SDCard*		pcSDCard;
	CTestPrivateBoard*	pcPrivate;
	CHCP65UARTBoard*	pcUART;
	CHCP65DebugPort*	pcPort;
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

	//uarttest.exe's argument is 0: it is the parent.
	TestKernalSetStartUp(TEST_HCP65_UART_CARD_IMAGE, TEST_HCP65_UART_PROGRAM_PATH, 0, &uiStatus);

	cHCP65.Init(TEST_UART_CLOCK_RATE);
	if (!TestKernalPlugIn(&cMainboard, &cHCP65, TEST_HCP65_UART_MACHINE))
	{
		cHCP65.Kill();
		cMainboard.Kill();
		return;
	}
	pcSDCard = (CHCP65SDCard*)TestKernalBoard(&cMainboard, TEST_HCP65_SD_SOCKET, HCP65_SD_NAME);
	TestKernalBoard(&cMainboard, TEST_HCP65_MEMORY_SOCKET, HCP65_MEMORY_BOARD_NAME);
	pcPrivate = (CTestPrivateBoard*)TestKernalBoard(&cMainboard, TEST_HCP65_PRIVATE_SOCKET, TEST_HCP65_PRIVATE_TYPE);
	pcUART = (CHCP65UARTBoard*)TestKernalBoard(&cMainboard, TEST_HCP65_UART_SOCKET, HCP65_UART_NAME);
	pcPort = cHCP65.GetDebugPort();
	TestKernalStart(&cHCP65, psKernal);

	//The far ends: the RS232 port's has RTS and DTR on (its CTS and DSR) and DCD off; the TTL port's has RTS on.
	pcTTL = pcUART->GetTTL();
	pcRS232 = pcUART->GetRS232();
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
		if (TestKernalIsDone(&sStatus) || cHCP65.IsStopped())
		{
			break;
		}
	}

	//uarttest.exe did what UARTTest.cpp checks.
	AssertFalse(cHCP65.IsStopped());
	AssertInt(TEST_KERNAL_STATE_BOOT_EXITED, sStatus.uiState);
	AssertInt(TEST_KERNAL_PROCESS_EXITED, sStatus.uiBootResult);
	AssertInt(0, sStatus.uiExitCode);

	//The driver was loaded from the card for the UART Board, after the kernal.
	AssertInt((1 << TEST_HCP65_SD_SOCKET) | (1 << TEST_HCP65_UART_SOCKET) | (1 << TEST_HCP65_MEMORY_SOCKET) | (1 << TEST_HCP65_PRIVATE_SOCKET), sStatus.uiDevices);

	//The Memory Board was given window 1 though the private board's socket is scanned first; the private board got the
	//next, but none of its memory is main memory, and nothing was put in it.
	AssertInt(1, cHCP65.GetMemoryChannel()->GetSocketWindow(TEST_HCP65_SOCKET_V));
	AssertInt(2, cHCP65.GetMemoryChannel()->GetSocketWindow(TEST_HCP65_SOCKET_X));
	AssertInt(64, sStatus.uiMainBanks);
	AssertInt(2 * HCP65_MEMORY_CHIP_SIZE, pcPrivate->FirstChanged(HCP65_DEBUG_FILL_BYTE));
	AssertInt(0, sStatus.uiBadDevices);
	AssertInt(0, sStatus.uiDriverError);
	AssertInt(1 << TEST_HCP65_UART_SOCKET, sStatus.uiLoadedDrivers);
	AssertSize(1, pcPort->NumDrivers());
	AssertString(TEST_HCP65_UART_DRIVER_PATH, pcPort->GetDriver(0)->szName);
	AssertInt(TEST_HCP65_UART_SOCKET, pcPort->GetDriver(0)->uiSlot);

	//The processes: uarttest.exe ($FF) ran itself as the child ($FE), which exited 0.
	AssertTrue(pcPort->TakeEvent(&eEvent, &sProcess));
	AssertInt(DPE_Started, eEvent);
	AssertInt(0xFF, sProcess.uiProcess);
	AssertTrue(pcPort->TakeEvent(&eEvent, &sProcess));
	AssertInt(DPE_Started, eEvent);
	AssertInt(0xFE, sProcess.uiProcess);
	AssertString(TEST_HCP65_UART_PROGRAM_PATH, sProcess.szName);
	AssertTrue(pcPort->TakeEvent(&eEvent, &sProcess));
	AssertInt(DPE_Ended, eEvent);
	AssertInt(0xFE, sProcess.uiProcess);
	AssertInt(0, sProcess.iExitCode);
	AssertTrue(pcPort->TakeEvent(&eEvent, &sProcess));
	AssertInt(DPE_Ended, eEvent);
	AssertInt(0xFF, sProcess.uiProcess);
	AssertFalse(pcPort->TakeEvent(&eEvent, &sProcess));

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
	pcSDCard->RemoveCard(TEST_HCP65_CARD_SOCKET);
	cHCP65.Kill();
	cMainboard.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Copies the blank SD card image to Output/VIACard.img and puts the VIA
// Board's test program (HCP65/VIATest) on it as \viatest.exe and its driver
// (HCP65/Drivers/VIABoard) as \OS\Drivers\VIA.DRV.
//
//////////////////////////////////////////////////////////////////////////
static bool TestKernalPutVIAFiles(void)
{
	if (!TestKernalCopyBlankCard(TEST_HCP65_VIA_CARD_IMAGE))
	{
		return false;
	}
	if (!TestKernalPutExe(TEST_HCP65_VIA_CARD_IMAGE, TEST_HCP65_VIA_PROGRAM_PATH, TEST_HCP65_VIA_PROGRAM, NULL, HCP65_EXE_DEFAULT_HEAP, false))
	{
		return false;
	}
	return TestKernalPutDriver(TEST_HCP65_VIA_CARD_IMAGE, TEST_HCP65_VIA_DRIVER_PATH, TEST_HCP65_VIA_DRIVER);
}


//////////////////////////////////////////////////////////////////////////
//
// Boots the kernal ROM, at the real board's 25MHz, with the SD Card Board
// (holding Output/VIACard.img) and a VIA Board with breakouts 0 and 2,
// whose lines the test plays.  VIATest.cpp, as \viatest.exe, checks the
// driver and exits 0 if it all did what it should:
//  - The board has eight units; only the four fitted open, and the driver
//    refuses what it doesn't take, writing IER among them.
//  - XJ's port B reads the $C3 the test drives; AB's port A drives it
//    inverted.
//  - A wait for AB's one shot T1 ends when it times out; a second times
//    out.
//  - A wait for XJ's CA1 ends when the test takes CA1 low, which it does
//    once the wait has turned CA1's interrupt on.
//  - A child process (viatest.exe again) can't open XJ but can open CD; it
//    drives CD's port B with $A5 and exits without closing it.
//
//////////////////////////////////////////////////////////////////////////
void TestHCP65KernalVIA(STestKernal* psKernal)
{
	CHCP65				cHCP65;
	CTestMainboard		cMainboard;
	CHCP65SDCard*		pcSDCard;
	CHCP65VIABoard*		pcVIA;
	CHCP65DebugPort*	pcPort;
	CHCP65VIA*			pcAB;
	CHCP65VIA*			pcCD;
	CHCP65VIA*			pcXJ;
	STestKernalStatus	sStatus;
	uint8*				puiRAM;
	uint32				uiStatus;
	int					i;
	bool				bDropped;

	if (!TestKernalPutVIAFiles())
	{
		return;
	}

	//viatest.exe's argument is 0: it is the parent.
	TestKernalSetStartUp(TEST_HCP65_VIA_CARD_IMAGE, TEST_HCP65_VIA_PROGRAM_PATH, 0, &uiStatus);

	cHCP65.Init(TEST_UART_CLOCK_RATE);
	if (!TestKernalPlugIn(&cMainboard, &cHCP65, TEST_HCP65_VIA_MACHINE))
	{
		cHCP65.Kill();
		cMainboard.Kill();
		return;
	}
	pcSDCard = (CHCP65SDCard*)TestKernalBoard(&cMainboard, TEST_HCP65_SD_SOCKET, HCP65_SD_NAME);
	TestKernalBoard(&cMainboard, TEST_HCP65_MEMORY_SOCKET, HCP65_MEMORY_BOARD_NAME);
	pcVIA = (CHCP65VIABoard*)TestKernalBoard(&cMainboard, TEST_HCP65_VIA_SOCKET, HCP65_VIA_BOARD_NAME);
	AssertInt(4, pcVIA->NumVIAs());
	pcPort = cHCP65.GetDebugPort();
	pcAB = pcVIA->GetVIA(0);
	pcCD = pcVIA->GetVIA(1);
	pcXJ = pcVIA->GetVIA(4);
	pcXJ->HostDrivePortA(0x77, 0xFF);
	pcXJ->HostDrivePortB(0xC3, 0xFF);
	TestKernalStart(&cHCP65, psKernal);

	bDropped = false;
	puiRAM = cHCP65.GetMPURAM();
	for (i = 0; i < TEST_UART_MAX_RUNS; i++)
	{
		cHCP65.Run(TEST_UART_RUN_CYCLES);

		//XJ's CA1 goes low once the driver has turned its interrupt on for viatest.exe's wait.
		if (!bDropped && (pcXJ->Read(HCP65_VIA_IER) & HCP65_VIA_INT_CA1))
		{
			pcXJ->HostSetCA1(false);
			bDropped = true;
			cHCP65.UpdateInterrupts();
		}

		TestKernalReadStatus(puiRAM, uiStatus, &sStatus);
		if (getenv("TEST_HCP65_VERBOSE"))
		{
			printf("via run %d: state %d, exit %d, process $%02X, PC $%06X\n", i, sStatus.uiState, sStatus.uiExitCode, cHCP65.GetProcessID(), cHCP65.GetMPU()->GetProgramAddress());
			fflush(stdout);
		}
		if (TestKernalIsDone(&sStatus) || cHCP65.IsStopped())
		{
			break;
		}
	}

	//viatest.exe did what VIATest.cpp checks.
	AssertFalse(cHCP65.IsStopped());
	AssertInt(TEST_KERNAL_STATE_BOOT_EXITED, sStatus.uiState);
	AssertInt(TEST_KERNAL_PROCESS_EXITED, sStatus.uiBootResult);
	AssertInt(0, sStatus.uiExitCode);
	AssertTrue(bDropped);

	//The driver was loaded from the card for the VIA Board.
	AssertInt((1 << TEST_HCP65_SD_SOCKET) | (1 << TEST_HCP65_VIA_SOCKET) | (1 << TEST_HCP65_MEMORY_SOCKET), sStatus.uiDevices);
	AssertInt(0, sStatus.uiBadDevices);
	AssertInt(0, sStatus.uiDriverError);
	AssertInt(1 << TEST_HCP65_VIA_SOCKET, sStatus.uiLoadedDrivers);
	AssertSize(1, pcPort->NumDrivers());
	AssertString(TEST_HCP65_VIA_DRIVER_PATH, pcPort->GetDriver(0)->szName);
	AssertInt(TEST_HCP65_VIA_SOCKET, pcPort->GetDriver(0)->uiSlot);

	//Closed, the VIAs are as reset leaves them, every pin an input, and their bus holders keep what was driven: AB's
	//port A $3C, and CD's port B $A5 from the child, which the kernal closed.
	AssertInt(0, pcAB->Read(HCP65_VIA_DDRA));
	AssertInt(0x3C, pcAB->GetPortA());
	AssertInt(0, pcCD->Read(HCP65_VIA_DDRB));
	AssertInt(0xA5, pcCD->GetPortB());
	AssertInt(0x80, pcXJ->Read(HCP65_VIA_IER));
	AssertFalse(pcVIA->IsInterrupting());

	pcSDCard->RemoveCard(TEST_HCP65_CARD_SOCKET);
	cHCP65.Kill();
	cMainboard.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Copies the blank SD card image to Output/PS2Card.img and puts the PS2
// Board's test program (HCP65/PS2Test) on it as \ps2test.exe and its driver
// (HCP65/Drivers/PS2Board) as \OS\Drivers\PS2.DRV.
//
//////////////////////////////////////////////////////////////////////////
static bool TestKernalPutPS2Files(void)
{
	if (!TestKernalCopyBlankCard(TEST_HCP65_PS2_CARD_IMAGE))
	{
		return false;
	}
	if (!TestKernalPutExe(TEST_HCP65_PS2_CARD_IMAGE, TEST_HCP65_PS2_PROGRAM_PATH, TEST_HCP65_PS2_PROGRAM, NULL, HCP65_EXE_DEFAULT_HEAP, false))
	{
		return false;
	}
	return TestKernalPutDriver(TEST_HCP65_PS2_CARD_IMAGE, TEST_HCP65_PS2_DRIVER_PATH, TEST_HCP65_PS2_DRIVER);
}


//////////////////////////////////////////////////////////////////////////
//
// Boots the kernal ROM, at the real board's 25MHz, with the SD Card Board
// (holding Output/PS2Card.img) and the PS2 Board with a keyboard in port
// 1 and nothing in port 2.  PS2Test.cpp, as \ps2test.exe, checks the driver
// and exits 0 if it all did what it should:
//  - Both ports open, and the driver refuses what it doesn't take.
//  - A byte sent to port 2 fails: nothing clocks it in.
//  - The keyboard is reset (FA AA), disabled, has its Num Lock LED set,
//    is identified (FA AB 83) and enabled, each answer read on its own,
//    with nothing of the sends' clocks among them.
//  - Then the test presses A and Cursor Up, which arrive as their scan
//    codes.
//  - A child process (ps2test.exe again) can't open port 1 but can open port
//    2, and exits without closing it.
//
//////////////////////////////////////////////////////////////////////////
void TestHCP65KernalPS2(STestKernal* psKernal)
{
	CHCP65				cHCP65;
	CTestMainboard		cMainboard;
	CHCP65SDCard*		pcSDCard;
	CHCP65PS2Board*		pcPS2;
	CHCP65PS2Keyboard	cKeyboard;
	CHCP65DebugPort*	pcPort;
	STestKernalStatus	sStatus;
	uint8*				puiRAM;
	uint32				uiStatus;
	int					i;
	bool				bPressed;

	if (!TestKernalPutPS2Files())
	{
		return;
	}

	//ps2test.exe's argument is 0: it is the parent.
	TestKernalSetStartUp(TEST_HCP65_PS2_CARD_IMAGE, TEST_HCP65_PS2_PROGRAM_PATH, 0, &uiStatus);

	cHCP65.Init(TEST_UART_CLOCK_RATE);
	if (!TestKernalPlugIn(&cMainboard, &cHCP65, TEST_HCP65_PS2_MACHINE))
	{
		cHCP65.Kill();
		cMainboard.Kill();
		return;
	}
	pcSDCard = (CHCP65SDCard*)TestKernalBoard(&cMainboard, TEST_HCP65_SD_SOCKET, HCP65_SD_NAME);
	TestKernalBoard(&cMainboard, TEST_HCP65_MEMORY_SOCKET, HCP65_MEMORY_BOARD_NAME);
	pcPS2 = (CHCP65PS2Board*)TestKernalBoard(&cMainboard, TEST_HCP65_PS2_SOCKET, HCP65_PS2_BOARD_NAME);
	pcPort = cHCP65.GetDebugPort();
	cKeyboard.Init();
	cKeyboard.Attach(pcPS2->GetPort(0));
	pcPS2->GetPort(1)->SetPlugged(false);
	TestKernalStart(&cHCP65, psKernal);

	bPressed = false;
	puiRAM = cHCP65.GetMPURAM();
	for (i = 0; i < TEST_UART_MAX_RUNS; i++)
	{
		cHCP65.Run(TEST_UART_RUN_CYCLES);

		//The keys go once ps2test.exe has the keyboard scanning again with Num Lock's LED on.
		if (!bPressed && cKeyboard.IsScanning() && (cKeyboard.GetLEDs() == HCP65_PS2_KEYBOARD_LED_NUM))
		{
			AssertTrue(cKeyboard.Key("A", true));
			AssertTrue(cKeyboard.Key("A", false));
			AssertTrue(cKeyboard.Key("Cursor Up", true));
			AssertTrue(cKeyboard.Key("Cursor Up", false));
			bPressed = true;
		}

		TestKernalReadStatus(puiRAM, uiStatus, &sStatus);
		if (getenv("TEST_HCP65_VERBOSE"))
		{
			printf("ps2 run %d: state %d, exit %d, process $%02X, PC $%06X\n", i, sStatus.uiState, sStatus.uiExitCode, cHCP65.GetProcessID(), cHCP65.GetMPU()->GetProgramAddress());
			fflush(stdout);
		}
		if (TestKernalIsDone(&sStatus) || cHCP65.IsStopped())
		{
			break;
		}
	}

	//ps2test.exe did what PS2Test.cpp checks.
	AssertFalse(cHCP65.IsStopped());
	AssertInt(TEST_KERNAL_STATE_BOOT_EXITED, sStatus.uiState);
	AssertInt(TEST_KERNAL_PROCESS_EXITED, sStatus.uiBootResult);
	AssertInt(0, sStatus.uiExitCode);
	AssertTrue(bPressed);

	//The driver was loaded from the card for the PS2 Board.
	AssertInt((1 << TEST_HCP65_SD_SOCKET) | (1 << TEST_HCP65_PS2_SOCKET) | (1 << TEST_HCP65_MEMORY_SOCKET), sStatus.uiDevices);
	AssertInt(0, sStatus.uiBadDevices);
	AssertInt(0, sStatus.uiDriverError);
	AssertInt(1 << TEST_HCP65_PS2_SOCKET, sStatus.uiLoadedDrivers);
	AssertSize(1, pcPort->NumDrivers());
	AssertString(TEST_HCP65_PS2_DRIVER_PATH, pcPort->GetDriver(0)->szName);

	//Every byte the driver sent was clocked in whole, and its lines were let go.
	AssertLong(0, (int64)pcPS2->GetPort(0)->NumErrors());
	AssertInt(HCP65_PS2_KEYBOARD_LED_NUM, cKeyboard.GetLEDs());
	AssertTrue(pcPS2->GetPort(0)->IsClockHigh());
	AssertTrue(pcPS2->GetPort(0)->IsDataHigh());
	AssertTrue(pcPS2->GetPort(1)->IsClockHigh());

	cKeyboard.Kill();
	pcSDCard->RemoveCard(TEST_HCP65_CARD_SOCKET);
	cHCP65.Kill();
	cMainboard.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// uiWidth columns of pSource from uiLeft (all its rows), each pixel
// TEST_HCP65_SPRITES_SCALE times as big, and left to right the other way
// if bMirror.
//
//////////////////////////////////////////////////////////////////////////
static Ptr<CImage> TestSpritesScale(Ptr<CImage> pSource, uint32 uiLeft, uint32 uiWidth, bool bMirror)
{
	Ptr<CImage>	pDest;
	uint8*		puiSource;
	uint8*		puiDest;
	lsize		uiPixel;
	uint32		uiSourceWidth;
	uint32		uiHeight;
	uint32		uiDestWidth;
	uint32		uiColumn;
	uint32		x;
	uint32		y;

	uiSourceWidth = pSource->GetWidth();
	uiHeight = pSource->GetHeight();
	uiDestWidth = uiWidth * TEST_HCP65_SPRITES_SCALE;
	pDest = OMalloc<CImage>(uiDestWidth, uiHeight * TEST_HCP65_SPRITES_SCALE, pSource);
	uiPixel = pSource->GetByteSize() / ((lsize)uiSourceWidth * uiHeight);
	if (pDest->GetByteSize() != uiPixel * uiDestWidth * uiHeight * TEST_HCP65_SPRITES_SCALE)
	{
		return NULL;
	}
	puiSource = (uint8*)pSource->GetData();
	puiDest = (uint8*)pDest->GetData();
	for (y = 0; y < uiHeight * TEST_HCP65_SPRITES_SCALE; y++)
	{
		for (x = 0; x < uiDestWidth; x++)
		{
			uiColumn = x / TEST_HCP65_SPRITES_SCALE;
			if (bMirror)
			{
				uiColumn = uiWidth - 1 - uiColumn;
			}
			memcpy(&puiDest[(y * uiDestWidth + x) * uiPixel], &puiSource[((y / TEST_HCP65_SPRITES_SCALE) * uiSourceWidth + uiLeft + uiColumn) * uiPixel], uiPixel);
		}
	}
	return pDest;
}


//////////////////////////////////////////////////////////////////////////
//
// sprites' images, with SupportLib's SFT writer (the pixels R3G3B2, the
// colours SFTColours shows): Output/STAGE.SFT, the stage (an opaque
// 640 by 400), and Output/MAKI.SFT, Maki's frames in a container,
// transparent and 84 by 182 each: standing then six walking, facing
// right, then the same facing left.
//
//////////////////////////////////////////////////////////////////////////
static bool TestKernalMakeSprites(void)
{
	CArrayImageCel	acCels;
	Ptr<CImage>		pStand;
	Ptr<CImage>		pWalk;
	Ptr<CImage>		pStage;
	uint32			uiFrameWidth;
	uint32			i;
	bool			bResult;
	bool			bMirror;
	int				iSide;

	DataIOInit();
	ObjectsInit();
	pStage = ReadImage((char*)TEST_HCP65_STAGE_IMAGE);
	pStand = ReadImage((char*)TEST_HCP65_MAKI_STAND_IMAGE);
	pWalk = ReadImage((char*)TEST_HCP65_MAKI_WALK_IMAGE);
	bResult = pStage.IsNotNull() && pStand.IsNotNull() && pWalk.IsNotNull();
	if (bResult)
	{
		pStage = TestSpritesScale(pStage, 0, pStage->GetWidth(), false);
		bResult = pStage.IsNotNull() && SaveSFT(pStage, (char*)TEST_HCP65_STAGE_FILE);
	}
	if (bResult)
	{
		acCels.Init();
		uiFrameWidth = pWalk->GetWidth() / TEST_HCP65_MAKI_WALK_FRAMES;
		for (iSide = 0; iSide < 2; iSide++)
		{
			bMirror = iSide == 1;
			acCels.Add(OMalloc<CImageCel>(TestSpritesScale(pStand, 0, pStand->GetWidth(), bMirror)));
			for (i = 0; i < TEST_HCP65_MAKI_WALK_FRAMES; i++)
			{
				acCels.Add(OMalloc<CImageCel>(TestSpritesScale(pWalk, i * uiFrameWidth, uiFrameWidth, bMirror)));
			}
		}
		bResult = SaveSFTCelArray(&acCels, (char*)TEST_HCP65_MAKI_FILE);
		acCels.Kill();
	}
	pStage = NULL;
	pStand = NULL;
	pWalk = NULL;
	ObjectsKill(false);
	DataIOKill();
	AssertTrue(bResult);
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Copies the blank SD card image to Output/SDCard.img, the system card
// the emulator boots: \OS\INIT.EXE (HCP65/Init) and \OS\SHELL.EXE
// (HCP65/Shell), the programs, the drivers, the console's font, sprites'
// images (TestKernalMakeSprites), the maths tables and \HELLO.TXT.  It has no
// \OS\STARTUP.TXT: the kernal runs INIT.EXE.
//
//////////////////////////////////////////////////////////////////////////
static bool TestKernalPutSystemFiles(void)
{
	bool	bResult;

	bResult = TestKernalCopyBlankCard(TEST_HCP65_SYSTEM_CARD_IMAGE);
	bResult = bResult && TestKernalPutExe(TEST_HCP65_SYSTEM_CARD_IMAGE, TEST_HCP65_INIT_PATH, TEST_HCP65_INIT_PROGRAM, NULL, HCP65_EXE_DEFAULT_HEAP, false);
	bResult = bResult && TestKernalPutExe(TEST_HCP65_SYSTEM_CARD_IMAGE, TEST_HCP65_SHELL_PATH, TEST_HCP65_SHELL_PROGRAM, NULL, HCP65_EXE_DEFAULT_HEAP, false);
	bResult = bResult && TestKernalPutExe(TEST_HCP65_SYSTEM_CARD_IMAGE, TEST_HCP65_SPRITES_PATH, TEST_HCP65_SPRITES_PROGRAM, NULL, TEST_HCP65_SPRITES_HEAP, false);
	bResult = bResult && TestKernalPutExe(TEST_HCP65_SYSTEM_CARD_IMAGE, TEST_HCP65_LCD_PROGRAM_PATH, TEST_HCP65_LCD_PROGRAM, NULL, HCP65_EXE_DEFAULT_HEAP, false);
	bResult = bResult && TestKernalPutDriver(TEST_HCP65_SYSTEM_CARD_IMAGE, TEST_HCP65_UART_DRIVER_PATH, TEST_HCP65_UART_DRIVER);
	bResult = bResult && TestKernalPutDriver(TEST_HCP65_SYSTEM_CARD_IMAGE, TEST_HCP65_PS2_DRIVER_PATH, TEST_HCP65_PS2_DRIVER);
	bResult = bResult && TestKernalPutDriver(TEST_HCP65_SYSTEM_CARD_IMAGE, TEST_HCP65_VIDEO_DRIVER_PATH, TEST_HCP65_VIDEO_DRIVER);
	bResult = bResult && TestKernalPutDriver(TEST_HCP65_SYSTEM_CARD_IMAGE, TEST_HCP65_BLITTER_DRIVER_PATH, TEST_HCP65_BLITTER_DRIVER);
	bResult = bResult && TestKernalPutDriver(TEST_HCP65_SYSTEM_CARD_IMAGE, TEST_HCP65_GRAPHICS_DRIVER_PATH, TEST_HCP65_GRAPHICS_DRIVER);
	bResult = bResult && TestKernalPutDriver(TEST_HCP65_SYSTEM_CARD_IMAGE, TEST_HCP65_LCD_DRIVER_PATH, TEST_HCP65_LCD_DRIVER);
	bResult = bResult && TestKernalPutDriver(TEST_HCP65_SYSTEM_CARD_IMAGE, TEST_HCP65_SOUND_DRIVER_PATH, TEST_HCP65_SOUND_DRIVER);
	bResult = bResult && TestKernalPutTables(TEST_HCP65_SYSTEM_CARD_IMAGE);
	bResult = bResult && FatPutDiskFileOnImage(TEST_HCP65_SYSTEM_CARD_IMAGE, TEST_HCP65_FONT_PATH, TEST_HCP65_FONT);
	bResult = bResult && TestKernalMakeSprites();
	bResult = bResult && FatPutDiskFileOnImage(TEST_HCP65_SYSTEM_CARD_IMAGE, TEST_HCP65_STAGE_PATH, TEST_HCP65_STAGE_FILE);
	bResult = bResult && FatPutDiskFileOnImage(TEST_HCP65_SYSTEM_CARD_IMAGE, TEST_HCP65_MAKI_PATH, TEST_HCP65_MAKI_FILE);
	bResult = bResult && FatPutFileOnImage(TEST_HCP65_SYSTEM_CARD_IMAGE, TEST_HCP65_HELLO_PATH, (uint8*)TEST_HCP65_HELLO_TEXT, strlen(TEST_HCP65_HELLO_TEXT));
	AssertTrue(bResult);
	return bResult;
}


//The shell's machine and the terminal on the UART Board's TTL port, the console's.
struct STestShell
{
	CHCP65				cHCP65;
	CTestMainboard		cMainboard;
	CHCP65UARTBoard*	pcUART;
	CHCP65Terminal		cTerminal;
	uint32				uiStatus;
};


//////////////////////////////////////////////////////////////////////////
//
// Runs the machine until the terminal's last line is a prompt (the current
// directory, then TEST_HCP65_SHELL_PROMPT), the cursor after it, and puts what it shows in *pszText.  False if it
// doesn't in TEST_HCP65_SHELL_MAX_RUNS.
//
//////////////////////////////////////////////////////////////////////////
static bool TestShellRunToPrompt(STestShell* psShell, CChars* pszText)
{
	STestKernalStatus	sStatus;
	size				uiLast;
	char*				szLine;
	size				uiLength;
	int					i;

	for (i = 0; i < TEST_HCP65_SHELL_MAX_RUNS; i++)
	{
		psShell->cHCP65.Run(TEST_UART_RUN_CYCLES);
		psShell->cTerminal.Update();
		uiLast = psShell->cTerminal.NumLines() - 1;
		szLine = psShell->cTerminal.GetLine(uiLast);
		uiLength = strlen(szLine);
		if ((szLine[0] == '/') && (uiLength >= 4) && (strcmp(&szLine[uiLength - 3], TEST_HCP65_SHELL_PROMPT) == 0) && (psShell->cTerminal.GetColumn() == uiLength) &&
			(psShell->pcUART->GetTTL()->NumHostToSend() == 0))
		{
			pszText->Kill();
			pszText->Init();
			psShell->cTerminal.GetText(pszText);
			return true;
		}
		TestKernalReadStatus(psShell->cHCP65.GetMPURAM(), psShell->uiStatus, &sStatus);
		if (TestKernalIsDone(&sStatus) || psShell->cHCP65.IsStopped())
		{
			break;
		}
	}
	if (getenv("TEST_HCP65_VERBOSE"))
	{
		pszText->Kill();
		pszText->Init();
		psShell->cTerminal.GetText(pszText);
		printf("No prompt: state %d, stopped %d, PC $%06X, process $%02X, depth %d, terminal:\n%s\n", sStatus.uiState, psShell->cHCP65.IsStopped(), psShell->cHCP65.GetMPU()->GetProgramAddress(), psShell->cHCP65.GetProcessID(), psShell->cHCP65.GetInterruptDepth(), pszText->Text());
		fflush(stdout);
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// Types szCommand and Enter at the terminal, and runs until the shell
// prompts again.  *pszOutput is what it showed from the command's line on.
//
//////////////////////////////////////////////////////////////////////////
static bool TestShellCommand(STestShell* psShell, const char* szCommand, CChars* pszOutput)
{
	CChars	szText;
	CChars	szLine;
	bool	bPrompted;
	size	uiCommand;

	psShell->cTerminal.Clear();
	psShell->pcUART->GetTTL()->HostSend((const uint8*)szCommand, strlen(szCommand));
	psShell->pcUART->GetTTL()->HostSend((const uint8*)"\r", 1);
	szText.Init();
	bPrompted = TestShellRunToPrompt(psShell, &szText);
	AssertTrue(bPrompted);

	//What follows the command as it was echoed.
	szLine.Init(szCommand);
	uiCommand = szText.Find(0, szLine.Text());
	pszOutput->Kill();
	pszOutput->Init((uiCommand != ARRAY_ELEMENT_NOT_FOUND) ? szText.Text(uiCommand + szLine.Length()) : szText.Text());
	szLine.Kill();
	szText.Kill();
	if (getenv("TEST_HCP65_VERBOSE"))
	{
		printf("%s:\n%s\n", szCommand, pszOutput->Text());
		fflush(stdout);
	}
	return bPrompted;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether szText has szPart in it.
//
//////////////////////////////////////////////////////////////////////////
static bool TestShellHas(CChars* pszText, const char* szPart)
{
	return strstr(pszText->Text(), szPart) != NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// Boots the system card (Output/SDCard.img) at the real board's 25MHz with
// the SD Card Board, the UART Board and a Memory Board, and uses the shell
// through the console, a terminal on the UART Board's TTL port.  The
// kernal runs \OS\INIT.EXE (there is no STARTUP.TXT), which loads the
// maths tables and runs the shell; the shell lists directories, shows,
// copies and deletes files, makes a directory, says what the boards and
// drivers are, what the memory is and how big the console is, refuses
// what isn't a command or a program, and runs a program (itself).  "exit"
// ends it, and INIT.EXE starts it again.
//
//////////////////////////////////////////////////////////////////////////
void TestHCP65KernalShell(STestKernal* psKernal)
{
	STestShell			sShell;
	STestKernalStatus	sStatus;
	CHCP65SDCard*		pcSDCard;
	CChars				szOutput;

	if (!TestKernalPutSystemFiles())
	{
		return;
	}
	sShell.uiStatus = TEST_KERNAL_STATUS_ADDRESS;

	sShell.cHCP65.Init(TEST_UART_CLOCK_RATE);
	if (!TestKernalPlugIn(&sShell.cMainboard, &sShell.cHCP65, TEST_HCP65_SHELL_MACHINE))
	{
		sShell.cHCP65.Kill();
		sShell.cMainboard.Kill();
		return;
	}
	pcSDCard = (CHCP65SDCard*)TestKernalBoard(&sShell.cMainboard, TEST_HCP65_SD_SOCKET, HCP65_SD_NAME);
	TestKernalBoard(&sShell.cMainboard, TEST_HCP65_MEMORY_SOCKET, HCP65_MEMORY_BOARD_NAME);
	sShell.pcUART = (CHCP65UARTBoard*)TestKernalBoard(&sShell.cMainboard, TEST_HCP65_UART_SOCKET, HCP65_UART_NAME);
	sShell.cTerminal.Init();
	sShell.cTerminal.Attach(sShell.pcUART->GetTTL());
	TestKernalStart(&sShell.cHCP65, psKernal);

	//INIT.EXE ran the shell, which said what it is and prompted.
	szOutput.Init();
	AssertTrue(TestShellRunToPrompt(&sShell, &szOutput));
	AssertTrue(TestShellHas(&szOutput, TEST_HCP65_SHELL_BANNER));
	TestKernalReadStatus(sShell.cHCP65.GetMPURAM(), sShell.uiStatus, &sStatus);
	AssertInt(TEST_KERNAL_STATE_MOUNTED, sStatus.uiState);
	AssertInt(0, sStatus.uiDriverError);
	AssertInt(1 << TEST_HCP65_UART_SOCKET, sStatus.uiLoadedDrivers);

	AssertTrue(TestShellCommand(&sShell, "ls", &szOutput));
	AssertTrue(TestShellHas(&szOutput, "  OS/\n"));
	AssertTrue(TestShellHas(&szOutput, "24  HELLO.TXT"));
	AssertTrue(TestShellCommand(&sShell, "cat hello.txt", &szOutput));
	AssertTrue(TestShellHas(&szOutput, "\nHello from the SD card.\n"));
	AssertTrue(TestShellCommand(&sShell, "cp hello.txt copy.txt", &szOutput));
	AssertTrue(TestShellHas(&szOutput, "24 bytes copied."));
	AssertTrue(TestShellCommand(&sShell, "cat copy.txt", &szOutput));
	AssertTrue(TestShellHas(&szOutput, "\nHello from the SD card.\n"));
	AssertTrue(TestShellCommand(&sShell, "rm copy.txt", &szOutput));
	AssertTrue(TestShellCommand(&sShell, "cat copy.txt", &szOutput));
	AssertTrue(TestShellHas(&szOutput, "/copy.txt isn't there."));
	AssertTrue(TestShellCommand(&sShell, "mkdir work", &szOutput));
	AssertTrue(TestShellCommand(&sShell, "cd work", &szOutput));
	AssertTrue(TestShellCommand(&sShell, "pwd", &szOutput));
	AssertTrue(TestShellHas(&szOutput, "\n/work\n"));
	AssertTrue(TestShellCommand(&sShell, "cd ../os", &szOutput));
	AssertTrue(TestShellCommand(&sShell, "ls", &szOutput));
	AssertTrue(TestShellHas(&szOutput, "SHELL.EXE"));
	AssertTrue(TestShellHas(&szOutput, "  Drivers/\n") || TestShellHas(&szOutput, "  DRIVERS/\n"));
	AssertTrue(TestShellCommand(&sShell, "cd", &szOutput));
	AssertTrue(TestShellCommand(&sShell, "pwd", &szOutput));
	AssertTrue(TestShellHas(&szOutput, "\n/\n"));
	AssertTrue(TestShellCommand(&sShell, "lsdev", &szOutput));
	AssertTrue(TestShellHas(&szOutput, "SD Card Board"));
	AssertTrue(TestShellHas(&szOutput, "built in"));
	AssertTrue(TestShellHas(&szOutput, "UART Board            UART.DRV"));
	AssertTrue(TestShellCommand(&sShell, "lsmod", &szOutput));
	AssertTrue(TestShellHas(&szOutput, "UART.DRV      UART Board"));
	AssertTrue(TestShellCommand(&sShell, "free", &szOutput));
	AssertTrue(TestShellHas(&szOutput, "Main memory        4096KB (64 banks)"));
	AssertTrue(TestShellCommand(&sShell, "mode", &szOutput));
	AssertTrue(TestShellHas(&szOutput, "80 columns, 24 rows"));
	AssertTrue(TestShellCommand(&sShell, "nosuch", &szOutput));
	AssertTrue(TestShellHas(&szOutput, "nosuch isn't a command or a program."));
	AssertTrue(TestShellCommand(&sShell, "help", &szOutput));
	AssertTrue(TestShellHas(&szOutput, "lsdev    The boards in the device sockets"));

	//A program run from the shell: the shell itself, found in \OS, whose exit comes back here.
	AssertTrue(TestShellCommand(&sShell, "shell", &szOutput));
	AssertTrue(TestShellHas(&szOutput, TEST_HCP65_SHELL_BANNER));
	AssertTrue(TestShellCommand(&sShell, "exit", &szOutput));
	AssertFalse(TestShellHas(&szOutput, TEST_HCP65_SHELL_BANNER));

	//The first shell's exit: INIT.EXE starts it again.
	AssertTrue(TestShellCommand(&sShell, "exit", &szOutput));
	AssertTrue(TestShellHas(&szOutput, TEST_HCP65_SHELL_BANNER));

	AssertFalse(sShell.cHCP65.IsStopped());
	AssertTrue(pcSDCard->NumSectorsWritten() > 0);
	szOutput.Kill();
	sShell.cTerminal.Kill();
	pcSDCard->RemoveCard(TEST_HCP65_CARD_SOCKET);
	sShell.cHCP65.Kill();
	sShell.cMainboard.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Copies the blank SD card image to Output/VideoCard.img and puts the
// Video Board's test program (HCP65/VideoTest) on it as \vidtest.exe and its
// driver (HCP65/Drivers/VideoBoard) as \OS\Drivers\VIDEO.DRV.
//
//////////////////////////////////////////////////////////////////////////
static bool TestKernalPutVideoFiles(void)
{
	if (!TestKernalCopyBlankCard(TEST_HCP65_VIDEO_CARD_IMAGE))
	{
		return false;
	}
	if (!TestKernalPutExe(TEST_HCP65_VIDEO_CARD_IMAGE, TEST_HCP65_VIDEO_PROGRAM_PATH, TEST_HCP65_VIDEO_PROGRAM, NULL, HCP65_EXE_DEFAULT_HEAP, false))
	{
		return false;
	}
	return TestKernalPutDriver(TEST_HCP65_VIDEO_CARD_IMAGE, TEST_HCP65_VIDEO_DRIVER_PATH, TEST_HCP65_VIDEO_DRIVER);
}


//////////////////////////////////////////////////////////////////////////
//
// Boots the kernal ROM, at the real board's 25MHz, with the SD Card Board
// (holding Output/VideoCard.img), the Video Board and a Memory Board.
// VideoTest.cpp, as \vidtest.exe, checks the driver and exits 0 if it all did
// what it should, leaving 640x480 shown: the pixel at (x, y) is palette
// 0's colour (x + y) & $FF (red i, green 255 - i, blue $40), but row 0,
// palette 5's (red), and row 1, which the row table makes row 0 again.
// The card it writes is the one the emulator's Machines/Video.xml boots.
//
//////////////////////////////////////////////////////////////////////////
void TestHCP65KernalVideo(STestKernal* psKernal)
{
	CHCP65				cHCP65;
	CTestMainboard		cMainboard;
	CHCP65SDCard*		pcSDCard;
	CHCP65VideoBoard*	pcVideo;
	CHCP65DebugPort*	pcPort;
	STestKernalStatus	sStatus;
	uint32*				puiFrame;
	uint32				uiStatus;
	uint32				uiWidth;
	uint32				uiHeight;
	uint32				uiExpected;
	uint32				uiWrong;
	uint32				x;
	uint32				y;
	uint8				uiIndex;
	int					i;

	if (!TestKernalPutVideoFiles())
	{
		return;
	}
	TestKernalSetStartUp(TEST_HCP65_VIDEO_CARD_IMAGE, TEST_HCP65_VIDEO_PROGRAM_PATH, 0, &uiStatus);

	cHCP65.Init(TEST_UART_CLOCK_RATE);
	if (!TestKernalPlugIn(&cMainboard, &cHCP65, TEST_HCP65_VIDEO_MACHINE))
	{
		cHCP65.Kill();
		cMainboard.Kill();
		return;
	}
	pcSDCard = (CHCP65SDCard*)TestKernalBoard(&cMainboard, TEST_HCP65_SD_SOCKET, HCP65_SD_NAME);
	TestKernalBoard(&cMainboard, TEST_HCP65_MEMORY_SOCKET, HCP65_MEMORY_BOARD_NAME);
	pcVideo = (CHCP65VideoBoard*)TestKernalBoard(&cMainboard, TEST_HCP65_VIDEO_SOCKET, HCP65_VIDEO_BOARD_NAME);
	pcPort = cHCP65.GetDebugPort();
	TestKernalStart(&cHCP65, psKernal);

	for (i = 0; i < TEST_UART_MAX_RUNS; i++)
	{
		cHCP65.Run(TEST_UART_RUN_CYCLES);
		TestKernalReadStatus(cHCP65.GetMPURAM(), uiStatus, &sStatus);
		if (getenv("TEST_HCP65_VERBOSE"))
		{
			printf("video run %d: state %d, exit %d, PC $%06X, frames %d\n", i, sStatus.uiState, sStatus.uiExitCode, cHCP65.GetMPU()->GetProgramAddress(), (int)pcVideo->NumFrames());
			fflush(stdout);
		}
		if (TestKernalIsDone(&sStatus) || cHCP65.IsStopped())
		{
			break;
		}
	}

	//vidtest.exe did what VideoTest.cpp checks, with the driver loaded from the card.
	AssertFalse(cHCP65.IsStopped());
	AssertInt(TEST_KERNAL_STATE_BOOT_EXITED, sStatus.uiState);
	AssertInt(TEST_KERNAL_PROCESS_EXITED, sStatus.uiBootResult);
	AssertInt(0, sStatus.uiExitCode);
	AssertInt((1 << TEST_HCP65_SD_SOCKET) | (1 << TEST_HCP65_VIDEO_SOCKET) | (1 << TEST_HCP65_MEMORY_SOCKET), sStatus.uiDevices);
	AssertInt(0, sStatus.uiDriverError);
	AssertInt(1 << TEST_HCP65_VIDEO_SOCKET, sStatus.uiLoadedDrivers);
	AssertSize(1, pcPort->NumDrivers());
	AssertString(TEST_HCP65_VIDEO_DRIVER_PATH, pcPort->GetDriver(0)->szName);

	//640x480 at 60Hz: 800 clocks a line, 525 lines, 25.175MHz.
	AssertLong(25175000, (int64)pcVideo->GetPixelClockRate());
	AssertInt(800, (int)pcVideo->GetLineLength());
	AssertInt(525, (int)pcVideo->GetFrameLines());

	//Two more frames, so that the last is drawn after vidtest.exe's last change, then the picture.
	cHCP65.Run(2 * 800 * 525 + 1000);
	puiFrame = (uint32*)malloc(640 * 480 * sizeof(uint32));
	AssertTrue(pcVideo->GetFrame(puiFrame, 640, 480, &uiWidth, &uiHeight));
	AssertInt(640, (int)uiWidth);
	AssertInt(480, (int)uiHeight);
	uiWrong = 0;
	for (y = 0; y < 480; y++)
	{
		for (x = 0; x < 640; x++)
		{
			if (y < 2)
			{
				uiExpected = 0xFFFF0000;
			}
			else
			{
				uiIndex = (uint8)(x + y);
				uiExpected = 0xFF000040 | ((uint32)uiIndex << 16) | ((uint32)(255 - uiIndex) << 8);
			}
			if (puiFrame[y * 640 + x] != uiExpected)
			{
				uiWrong++;
			}
		}
	}
	AssertInt(0, (int)uiWrong);
	free(puiFrame);

	pcSDCard->RemoveCard(TEST_HCP65_CARD_SOCKET);
	cHCP65.Kill();
	cMainboard.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Without a memory board the kernal scans the devices and stops: the SD
// Card Board has nothing to read into.  Its heap is MPU RAM's, so it gets
// that far.  A private board's memory isn't main memory, though it is
// given a window.
//
//////////////////////////////////////////////////////////////////////////
void TestHCP65KernalNoMainMemory(STestKernal* psKernal)
{
	CHCP65				cHCP65;
	CTestMainboard		cMainboard;
	CHCP65SDCard*		pcSDCard;
	CTestPrivateBoard*	pcPrivate;
	STestKernalStatus	sStatus;
	uint32				uiStatus;
	int					i;

	uiStatus = TEST_KERNAL_STATUS_ADDRESS;
	cHCP65.Init(TEST_HCP65_CLOCK_RATE);
	if (!TestKernalPlugIn(&cMainboard, &cHCP65, TEST_HCP65_NO_MEMORY_MACHINE))
	{
		cHCP65.Kill();
		cMainboard.Kill();
		return;
	}
	pcSDCard = (CHCP65SDCard*)TestKernalBoard(&cMainboard, TEST_HCP65_SD_SOCKET, HCP65_SD_NAME);
	pcPrivate = (CTestPrivateBoard*)TestKernalBoard(&cMainboard, TEST_HCP65_PRIVATE_SOCKET, TEST_HCP65_PRIVATE_TYPE);
	AssertTrue(cHCP65.LoadROM(psKernal->cBoot.GetImage(), HCP65_ROM_SIZE));
	cHCP65.Reset();
	sStatus.uiState = 0;
	for (i = 0; (i < TEST_HCP65_MAX_RUNS) && !TestKernalIsDone(&sStatus) && !cHCP65.IsStopped(); i++)
	{
		cHCP65.Run(TEST_HCP65_RUN_CYCLES);
		TestKernalReadStatus(cHCP65.GetMPURAM(), uiStatus, &sStatus);
	}
	AssertInt(TEST_KERNAL_STATE_NO_MAIN_MEMORY, sStatus.uiState);
	AssertInt(0, sStatus.uiMainBanks);
	AssertInt((1 << TEST_HCP65_SD_SOCKET) | (1 << TEST_HCP65_PRIVATE_SOCKET), sStatus.uiDevices);
	AssertInt(1, cHCP65.GetMemoryChannel()->GetSocketWindow(TEST_HCP65_SOCKET_X));
	AssertInt(2 * HCP65_MEMORY_CHIP_SIZE, pcPrivate->FirstChanged(0));
	AssertInt(0, pcSDCard->NumSectorsRead());
	cHCP65.Kill();
	cMainboard.Kill();
}


//The console's machine: the shell on the Video Board's text and the PS2 Board's keyboard.
struct STestConsole
{
	CHCP65				cHCP65;
	CTestMainboard		cMainboard;
	CHCP65VideoBoard*	pcVideo;
	CHCP65PS2Board*		pcPS2;
	CHCP65PS2Keyboard	cKeyboard;
	uint8				auiFont[TEST_HCP65_FONT_SIZE];
	uint32*				puiFrame;
	uint32				uiStatus;
};


//////////////////////////////////////////////////////////////////////////
//
// The character whose glyph is the cell at (uiColumn, uiRow) of the
// picture, either way round (the cursor's cell may be inverted); '?' if
// none is.
//
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//
// The console's font (an SFT container of 256 transparent images of 8 by
// 16: UserLib/HCP65SFT.h) as each glyph's 16 rows, a row a byte with its
// leftmost pixel bit 7.
//
//////////////////////////////////////////////////////////////////////////
static bool TestConsoleReadFont(uint8* puiFont)
{
	static uint8	auiFile[0x10000];
	FILE*			pFile;
	size			uiSize;
	size			uiPosition;
	size			uiCode;
	size			uiNext;
	size			uiGlyph;
	size			x;
	size			y;
	size			i;
	uint8			c;

	pFile = fopen(TEST_HCP65_FONT, "rb");
	if (!pFile)
	{
		return false;
	}
	uiSize = fread(auiFile, 1, sizeof(auiFile), pFile);
	fclose(pFile);
	memset(auiFile + uiSize, 0, sizeof(auiFile) - uiSize);
	memset(puiFont, 0, TEST_HCP65_FONT_SIZE);
	if ((uiSize < 4) || (auiFile[2] != 0) || (auiFile[3] != 1))
	{
		return false;
	}

	uiPosition = 4;
	for (uiGlyph = 0; uiGlyph < 256; uiGlyph++)
	{
		uiNext = auiFile[uiPosition] | ((size)auiFile[uiPosition + 1] << 8);
		x = 0;
		y = 0;
		for (uiCode = uiPosition + 8; (uiCode < uiSize) && (auiFile[uiCode] != 0); )
		{
			c = auiFile[uiCode++];
			if (c >= 0xE0)
			{
				//A mixed run: its pixels that aren't 0.
				c = (uint8)((c & 0x1F) + 1);
				if ((y >= 16) || (x + c > 8))
				{
					return false;
				}
				for (i = 0; i < c; i++, x++)
				{
					if (auiFile[uiCode + i] != 0)
					{
						puiFont[uiGlyph * 16 + y] |= (uint8)(0x80 >> x);
					}
				}
				uiCode += c;
			}
			else if (c >= 0xC0)
			{
				y += (c & 0x1F) + 1;
				x = 0;
			}
			else if (c >= 0x80)
			{
				x += (c & 0x3F) + 1;
			}
			else
			{
				if ((y >= 16) || (x + c > 8))
				{
					return false;
				}
				for (i = 0; i < c; i++, x++)
				{
					puiFont[uiGlyph * 16 + y] |= (uint8)(0x80 >> x);
				}
				uiCode += c;
			}
		}
		if ((uiGlyph < 255) && ((uiNext == 0) || (uiPosition + uiNext + 8 > uiSize)))
		{
			return false;
		}
		uiPosition += uiNext;
	}
	return true;
}


static char TestConsoleCell(STestConsole* psConsole, uint32 uiWidth, uint32 uiColumn, uint32 uiRow)
{
	uint8	auiCell[16];
	uint8	uiByte;
	uint32	uiPixel;
	uint32	x;
	uint32	y;
	int		c;
	int		iInverted;

	for (y = 0; y < 16; y++)
	{
		uiByte = 0;
		for (x = 0; x < 8; x++)
		{
			uiPixel = psConsole->puiFrame[(uiRow * 16 + y) * uiWidth + uiColumn * 8 + x];
			if (uiPixel != TEST_HCP65_CONSOLE_PAPER)
			{
				uiByte |= (uint8)(0x80 >> x);
			}
		}
		auiCell[y] = uiByte;
	}
	for (iInverted = 0; iInverted < 2; iInverted++)
	{
		for (c = ' '; c < 0x100; c++)
		{
			for (y = 0; y < 16; y++)
			{
				if (auiCell[y] != (uint8)(iInverted ? ~psConsole->auiFont[c * 16 + y] : psConsole->auiFont[c * 16 + y]))
				{
					break;
				}
			}
			if (y == 16)
			{
				return (char)c;
			}
		}
	}
	return '?';
}


//////////////////////////////////////////////////////////////////////////
//
// What the Video Board shows, read back a cell at a time, each row a line
// without its trailing spaces.  False if it isn't showing 80 by 30 text.
//
//////////////////////////////////////////////////////////////////////////
static bool TestConsoleScreen(STestConsole* psConsole, CChars* pszScreen)
{
	uint32	uiWidth;
	uint32	uiHeight;
	uint32	uiRow;
	uint32	uiColumn;
	char	szLine[TEST_HCP65_SCREEN_COLUMNS + 1];
	int		iEnd;

	pszScreen->Kill();
	pszScreen->Init();
	if (!psConsole->pcVideo->GetFrame(psConsole->puiFrame, 640, 480, &uiWidth, &uiHeight) || (uiWidth != 640) || (uiHeight != 480))
	{
		return false;
	}
	for (uiRow = 0; uiRow < TEST_HCP65_SCREEN_ROWS; uiRow++)
	{
		for (uiColumn = 0; uiColumn < TEST_HCP65_SCREEN_COLUMNS; uiColumn++)
		{
			szLine[uiColumn] = TestConsoleCell(psConsole, uiWidth, uiColumn, uiRow);
		}
		for (iEnd = TEST_HCP65_SCREEN_COLUMNS; (iEnd > 0) && (szLine[iEnd - 1] == ' '); iEnd--)
		{
		}
		szLine[iEnd] = '\0';
		pszScreen->Append(szLine);
		pszScreen->Append('\n');
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Runs the machine until szText is on the screen, which is then in
// *pszScreen.  False if it isn't in TEST_HCP65_SHELL_MAX_RUNS.
//
//////////////////////////////////////////////////////////////////////////
static bool TestConsoleRunUntil(STestConsole* psConsole, const char* szText, CChars* pszScreen)
{
	STestKernalStatus	sStatus;
	int					i;

	for (i = 0; i < TEST_HCP65_SHELL_MAX_RUNS; i++)
	{
		psConsole->cHCP65.Run(TEST_UART_RUN_CYCLES);
		if (((i % 10) == 9) && TestConsoleScreen(psConsole, pszScreen) && strstr(pszScreen->Text(), szText))
		{
			return true;
		}
		TestKernalReadStatus(psConsole->cHCP65.GetMPURAM(), psConsole->uiStatus, &sStatus);
		if (TestKernalIsDone(&sStatus) || psConsole->cHCP65.IsStopped())
		{
			break;
		}
	}
	if (getenv("TEST_HCP65_VERBOSE"))
	{
		printf("Not on the screen: [%s], PC $%06X, screen:\n%s\n", szText, psConsole->cHCP65.GetMPU()->GetProgramAddress(), pszScreen->Text());
		fflush(stdout);
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// szText typed on the keyboard, a key at a time: lower case letters,
// digits, spaces, '\\', '.' and '\r' (Enter).
//
//////////////////////////////////////////////////////////////////////////
static void TestConsoleType(STestConsole* psConsole, const char* szText)
{
	char	szKey[2];
	char	c;

	for (; *szText; szText++)
	{
		c = *szText;
		szKey[1] = '\0';
		if ((c >= 'a') && (c <= 'z'))
		{
			szKey[0] = (char)(c - 'a' + 'A');
			psConsole->cKeyboard.Key(szKey, true);
			psConsole->cKeyboard.Key(szKey, false);
		}
		else if (c == ' ')
		{
			psConsole->cKeyboard.Key("Space", true);
			psConsole->cKeyboard.Key("Space", false);
		}
		else if (c == '\r')
		{
			psConsole->cKeyboard.Key("Enter", true);
			psConsole->cKeyboard.Key("Enter", false);
		}
		else if (c == '\x1B')
		{
			psConsole->cKeyboard.Key("Escape", true);
			psConsole->cKeyboard.Key("Escape", false);
		}
		else
		{
			szKey[0] = c;
			psConsole->cKeyboard.Key(szKey, true);
			psConsole->cKeyboard.Key(szKey, false);
		}
	}
}


#define TEST_SPRITES_STAGE_Y		40			//Sprites.cpp's picture: the stage's top,
#define TEST_SPRITES_MAKI_Y			246			//Maki's,
#define TEST_SPRITES_MAKI_START_X	278			//her left to start with,
#define TEST_SPRITES_MAKI_WIDTH		84
#define TEST_SPRITES_MAKI_ROW		(TEST_SPRITES_MAKI_Y + 60)	//and a row through her.


//////////////////////////////////////////////////////////////////////////
//
// The colour a pixel of uiValue is in SFTColours' palette.
//
//////////////////////////////////////////////////////////////////////////
static uint32 TestSpritesColour(uint8 uiValue)
{
	uint32	uiRed;
	uint32	uiGreen;
	uint32	uiBlue;

	uiRed = uiValue & 7;
	uiGreen = (uiValue >> 3) & 7;
	uiBlue = (uiValue >> 6) & 3;
	uiRed = (uiRed << 5) | (uiRed << 2) | (uiRed >> 1);
	uiGreen = (uiGreen << 5) | (uiGreen << 2) | (uiGreen >> 1);
	uiBlue = (uiBlue << 6) | (uiBlue << 4) | (uiBlue << 2) | uiBlue;
	return 0xFF000000 | (uiRed << 16) | (uiGreen << 8) | uiBlue;
}


//////////////////////////////////////////////////////////////////////////
//
// Output/STAGE.SFT (TestKernalMakeSprites), to free; NULL if it can't be
// read.
//
//////////////////////////////////////////////////////////////////////////
static uint8* TestSpritesReadStage(void)
{
	FILE*	pFile;
	uint8*	puiStage;
	size_t	uiRead;

	pFile = fopen(TEST_HCP65_STAGE_FILE, "rb");
	if (!pFile)
	{
		return NULL;
	}
	puiStage = (uint8*)malloc(sizeof(SSFTImage) + 640 * 400);
	uiRead = fread(puiStage, 1, sizeof(SSFTImage) + 640 * 400, pFile);
	fclose(pFile);
	if (uiRead != sizeof(SSFTImage) + 640 * 400)
	{
		free(puiStage);
		return NULL;
	}
	return puiStage;
}


//////////////////////////////////////////////////////////////////////////
//
// The colour the stage is at (x, y) on the screen.
//
//////////////////////////////////////////////////////////////////////////
static uint32 TestSpritesStage(const uint8* puiStage, uint32 x, uint32 y)
{
	return TestSpritesColour(puiStage[sizeof(SSFTImage) + (y - TEST_SPRITES_STAGE_Y) * 640 + x]);
}


//////////////////////////////////////////////////////////////////////////
//
// Where Maki is in the frame: the first and last pixels of the row through
// her that aren't the stage's.  False if there are none, or they are
// further apart than she is wide.
//
//////////////////////////////////////////////////////////////////////////
static bool TestSpritesFindMaki(const uint32* puiFrame, const uint8* puiStage, int* piLeft, int* piRight)
{
	int		x;

	*piLeft = -1;
	*piRight = -1;
	for (x = 0; x < 640; x++)
	{
		if (puiFrame[TEST_SPRITES_MAKI_ROW * 640 + x] != TestSpritesStage(puiStage, (uint32)x, TEST_SPRITES_MAKI_ROW))
		{
			if (*piLeft < 0)
			{
				*piLeft = x;
			}
			*piRight = x;
		}
	}
	return (*piLeft >= 0) && ((*piRight - *piLeft) < TEST_SPRITES_MAKI_WIDTH);
}


//////////////////////////////////////////////////////////////////////////
//
// The screen's last frame, and where Maki is in it (TestSpritesFindMaki).
//
//////////////////////////////////////////////////////////////////////////
static bool TestSpritesFrame(STestConsole* psConsole, const uint8* puiStage, int* piLeft, int* piRight)
{
	uint32	uiWidth;
	uint32	uiHeight;

	if (!psConsole->pcVideo->GetFrame(psConsole->puiFrame, 640, 480, &uiWidth, &uiHeight) || (uiWidth != 640) || (uiHeight != 480))
	{
		return false;
	}
	return TestSpritesFindMaki(psConsole->puiFrame, puiStage, piLeft, piRight);
}


//////////////////////////////////////////////////////////////////////////
//
// sprites typed at the prompt, and its picture checked: black, with the
// stage from y 40 (Sprites.cpp) and Maki standing on it in the middle.
// It is left running, waiting for Escape: a key that isn't doesn't end
// it.
//
//////////////////////////////////////////////////////////////////////////
static void TestConsoleSprites(STestConsole* psConsole, const uint8* puiStage)
{
	int		iLeft;
	int		iRight;
	bool	bShown;
	int		i;

	TestConsoleType(psConsole, "sprites\r");
	bShown = false;
	for (i = 0; (i < 4000) && !bShown; i++)
	{
		psConsole->cHCP65.Run(TEST_UART_RUN_CYCLES);
		if ((i % 10) == 9)
		{
			bShown = TestSpritesFrame(psConsole, puiStage, &iLeft, &iRight) && (psConsole->puiFrame[50 * 640 + 10] == TestSpritesStage(puiStage, 10, 50));
		}
	}
	AssertTrue(bShown);
	AssertInt((int)0xFF000000, (int)psConsole->puiFrame[20 * 640 + 600]);
	AssertInt((int)0xFF000000, (int)psConsole->puiFrame[470 * 640 + 630]);
	AssertInt((int)TestSpritesStage(puiStage, 0, 40), (int)psConsole->puiFrame[40 * 640 + 0]);
	AssertInt((int)TestSpritesStage(puiStage, 320, 100), (int)psConsole->puiFrame[100 * 640 + 320]);
	AssertInt((int)TestSpritesStage(puiStage, 639, 439), (int)psConsole->puiFrame[439 * 640 + 639]);
	AssertTrue(iLeft >= TEST_SPRITES_MAKI_START_X);
	AssertTrue(iRight < TEST_SPRITES_MAKI_START_X + TEST_SPRITES_MAKI_WIDTH);
	AssertTrue(iRight - iLeft > 10);

	TestConsoleType(psConsole, " ");
	for (i = 0; i < 20; i++)
	{
		psConsole->cHCP65.Run(TEST_UART_RUN_CYCLES);
	}
	AssertTrue(TestSpritesFrame(psConsole, puiStage, &iLeft, &iRight));
}


//////////////////////////////////////////////////////////////////////////
//
// Boots the system card (Output/SDCard.img) at the real board's 25MHz with
// the SD Card Board, the Video Board, the PS2 Board (the test's keyboard
// in port 1) and a Memory Board: the console is the Video Board's text,
// 640x480's 80 by 30 in CONSOLE.SFT, and the keyboard.  The shell's
// banner and prompt are on the screen; what is typed is echoed and done;
// a listing longer than the screen scrolls it; Caps Lock types capitals
// and lights its LED; mode 2 is 1024x768's 128 by 48.
//
//////////////////////////////////////////////////////////////////////////
void TestHCP65KernalConsole(STestKernal* psKernal)
{
	STestConsole		sConsole;
	STestKernalStatus	sStatus;
	CHCP65SDCard*		pcSDCard;
	CChars				szScreen;
	uint8*				puiStage;
	uint32				uiWidth;
	uint32				uiHeight;
	int					i;

	AssertTrue(TestConsoleReadFont(sConsole.auiFont));
	if (!TestKernalPutSystemFiles())
	{
		return;
	}
	sConsole.uiStatus = TEST_KERNAL_STATUS_ADDRESS;
	sConsole.puiFrame = (uint32*)malloc(1024 * 768 * sizeof(uint32));

	sConsole.cHCP65.Init(TEST_UART_CLOCK_RATE);
	if (!TestKernalPlugIn(&sConsole.cMainboard, &sConsole.cHCP65, TEST_HCP65_CONSOLE_MACHINE))
	{
		free(sConsole.puiFrame);
		sConsole.cHCP65.Kill();
		sConsole.cMainboard.Kill();
		return;
	}
	pcSDCard = (CHCP65SDCard*)TestKernalBoard(&sConsole.cMainboard, TEST_HCP65_SD_SOCKET, HCP65_SD_NAME);
	TestKernalBoard(&sConsole.cMainboard, TEST_HCP65_MEMORY_SOCKET, HCP65_MEMORY_BOARD_NAME);
	sConsole.pcVideo = (CHCP65VideoBoard*)TestKernalBoard(&sConsole.cMainboard, TEST_HCP65_VIDEO_SOCKET, HCP65_VIDEO_BOARD_NAME);
	sConsole.pcPS2 = (CHCP65PS2Board*)TestKernalBoard(&sConsole.cMainboard, TEST_HCP65_PS2_SOCKET, HCP65_PS2_BOARD_NAME);
	sConsole.cKeyboard.Init();
	sConsole.cKeyboard.Attach(sConsole.pcPS2->GetPort(0));
	sConsole.pcPS2->GetPort(1)->SetPlugged(false);
	TestKernalStart(&sConsole.cHCP65, psKernal);

	//The shell's banner and its prompt, in light grey on black.
	szScreen.Init();
	AssertTrue(TestConsoleRunUntil(&sConsole, TEST_HCP65_SHELL_BANNER "\n/ $\n", &szScreen));
	AssertTrue(szScreen.StartsWith(TEST_HCP65_SHELL_BANNER "\n/ $\n"));
	TestKernalReadStatus(sConsole.cHCP65.GetMPURAM(), sConsole.uiStatus, &sStatus);
	AssertInt((1 << TEST_HCP65_VIDEO_SOCKET) | (1 << TEST_HCP65_PS2_SOCKET), sStatus.uiLoadedDrivers);
	AssertLong(25175000, (int64)sConsole.pcVideo->GetPixelClockRate());
	AssertTrue(sConsole.pcVideo->GetFrame(sConsole.puiFrame, 640, 480, &uiWidth, &uiHeight));
	AssertInt(TEST_HCP65_CONSOLE_INK, sConsole.puiFrame[4 * 640 + 1]);  //The banner's H: its left stroke, and inside it.
	AssertInt(TEST_HCP65_CONSOLE_PAPER, sConsole.puiFrame[4 * 640 + 3]);

	//What is typed is shown, then done.
	TestConsoleType(&sConsole, "cat hello.txt\r");
	AssertTrue(TestConsoleRunUntil(&sConsole, "/ $ cat hello.txt\nHello from the SD card.\n/ $\n", &szScreen));
	TestConsoleType(&sConsole, "mode\r");
	AssertTrue(TestConsoleRunUntil(&sConsole, "/ $ mode\n80 columns, 30 rows\n/ $\n", &szScreen));

	//Caps Lock types capitals, and its LED is lit.
	sConsole.cKeyboard.Key("Caps Lock", true);
	sConsole.cKeyboard.Key("Caps Lock", false);
	TestConsoleType(&sConsole, "pwd");
	AssertTrue(TestConsoleRunUntil(&sConsole, "/ $ PWD\n", &szScreen));
	sConsole.cKeyboard.Key("Caps Lock", true);
	sConsole.cKeyboard.Key("Caps Lock", false);
	TestConsoleType(&sConsole, "\r");
	AssertTrue(TestConsoleRunUntil(&sConsole, "PWD isn't a command or a program.\n/ $\n", &szScreen));
	AssertInt(0, sConsole.cKeyboard.GetLEDs());

	//help is longer than what is left of the screen: it scrolls, the last line at the bottom.
	for (i = 0; i < 2; i++)
	{
		TestConsoleType(&sConsole, "help\r");
		AssertTrue(TestConsoleRunUntil(&sConsole, "Anything else is a program to run, with a number for its argument.\n/ $\n", &szScreen));
	}
	AssertFalse(strstr(szScreen.Text(), TEST_HCP65_SHELL_BANNER) != NULL);
	AssertTrue(szScreen.EndsWith("Anything else is a program to run, with a number for its argument.\n/ $\n"));

	puiStage = TestSpritesReadStage();
	AssertTrue(puiStage != NULL);
	TestConsoleSprites(&sConsole, puiStage);
	free(puiStage);
	TestConsoleType(&sConsole, "\x1B");
	AssertTrue(TestConsoleRunUntil(&sConsole, "/ $\n", &szScreen));
	AssertFalse(strstr(szScreen.Text(), "sprites") != NULL);
	AssertFalse(strstr(szScreen.Text(), "?") != NULL);

	//mode 2: 1024x768, 128 by 48, cleared.
	TestConsoleType(&sConsole, "mode 2\r");
	for (i = 0; i < 2000; i++)
	{
		sConsole.cHCP65.Run(TEST_UART_RUN_CYCLES);
		if ((sConsole.pcVideo->GetActiveWidth() == 1024) && (sConsole.pcVideo->GetPixelClockRate() == 65000000))
		{
			break;
		}
	}
	AssertInt(1024, (int)sConsole.pcVideo->GetActiveWidth());
	AssertLong(65000000, (int64)sConsole.pcVideo->GetPixelClockRate());

	AssertFalse(sConsole.cHCP65.IsStopped());
	szScreen.Kill();
	sConsole.cKeyboard.Kill();
	free(sConsole.puiFrame);
	pcSDCard->RemoveCard(TEST_HCP65_CARD_SOCKET);
	sConsole.cHCP65.Kill();
	sConsole.cMainboard.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The console's machine with a Blitter Board and a Graphics Board too
// (Machines/Blitter.xml).  The Graphics Board's driver is loaded from
// \OS\Drivers\GRAPHICS.DRV and enables it; nothing draws with it yet.  The
// Blitter Board's is loaded from \OS\Drivers\BLITTER.DRV, and the copies that
// are long enough are the board's, not the processor's: the screen's and
// a row's clearing, and sprites' tiles.  The text and the picture are the
// same, and the processor never touches main memory while the board has
// it.
//
//////////////////////////////////////////////////////////////////////////
void TestHCP65KernalBlitter(STestKernal* psKernal)
{
	STestConsole		sConsole;
	STestKernalStatus	sStatus;
	CHCP65SDCard*		pcSDCard;
	CHCP65BlitterBoard*	pcBlitter;
	CHCP65GraphicsBoard*	pcGraphics;
	CChars				szScreen;
	uint8*				puiStage;
	uint64				uiText;
	int					i;

	AssertTrue(TestConsoleReadFont(sConsole.auiFont));
	if (!TestKernalPutSystemFiles())
	{
		return;
	}
	sConsole.uiStatus = TEST_KERNAL_STATUS_ADDRESS;
	sConsole.puiFrame = (uint32*)malloc(1024 * 768 * sizeof(uint32));

	sConsole.cHCP65.Init(TEST_UART_CLOCK_RATE);
	if (!TestKernalPlugIn(&sConsole.cMainboard, &sConsole.cHCP65, TEST_HCP65_BLITTER_MACHINE))
	{
		free(sConsole.puiFrame);
		sConsole.cHCP65.Kill();
		sConsole.cMainboard.Kill();
		return;
	}
	pcSDCard = (CHCP65SDCard*)TestKernalBoard(&sConsole.cMainboard, TEST_HCP65_SD_SOCKET, HCP65_SD_NAME);
	TestKernalBoard(&sConsole.cMainboard, TEST_HCP65_MEMORY_SOCKET, HCP65_MEMORY_BOARD_NAME);
	sConsole.pcVideo = (CHCP65VideoBoard*)TestKernalBoard(&sConsole.cMainboard, TEST_HCP65_VIDEO_SOCKET, HCP65_VIDEO_BOARD_NAME);
	sConsole.pcPS2 = (CHCP65PS2Board*)TestKernalBoard(&sConsole.cMainboard, TEST_HCP65_PS2_SOCKET, HCP65_PS2_BOARD_NAME);
	pcBlitter = (CHCP65BlitterBoard*)TestKernalBoard(&sConsole.cMainboard, TEST_HCP65_BLITTER_SOCKET, HCP65_BLITTER_NAME);
	pcGraphics = (CHCP65GraphicsBoard*)TestKernalBoard(&sConsole.cMainboard, TEST_HCP65_GRAPHICS_SOCKET, HCP65_GRAPHICS_NAME);
	sConsole.cKeyboard.Init();
	sConsole.cKeyboard.Attach(sConsole.pcPS2->GetPort(0));
	sConsole.pcPS2->GetPort(1)->SetPlugged(false);
	TestKernalStart(&sConsole.cHCP65, psKernal);

	//The boards' drivers are loaded and have enabled them.
	szScreen.Init();
	AssertTrue(TestConsoleRunUntil(&sConsole, TEST_HCP65_SHELL_BANNER "\n/ $\n", &szScreen));
	TestKernalReadStatus(sConsole.cHCP65.GetMPURAM(), sConsole.uiStatus, &sStatus);
	AssertInt((1 << TEST_HCP65_VIDEO_SOCKET) | (1 << TEST_HCP65_PS2_SOCKET) | (1 << TEST_HCP65_BLITTER_SOCKET) | (1 << TEST_HCP65_GRAPHICS_SOCKET), sStatus.uiLoadedDrivers);
	AssertInt(HCP65_BLITTER_FLAG_ENABLE, pcBlitter->Read(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_FLAGS) & HCP65_BLITTER_FLAG_ENABLE);
	AssertInt(HCP65_BLITTER_FLAG_ENABLE, pcGraphics->Read(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_FLAGS) & HCP65_BLITTER_FLAG_ENABLE);
	AssertFalse(pcGraphics->IsCopying());

	//The screen was cleared by the board, a line at a time (640 by 480 and a row more).  The banner's glyphs' runs are
	//too short for it: only a run of all eight pixels is the board's.
	uiText = pcBlitter->NumBytesCopied();
	AssertTrue(uiText >= 640 * 496);
	AssertTrue(uiText < 640 * 496 * 2 + 4096);

	//help twice scrolls the screen: each row that comes up is cleared by the board, sixteen lines of 640.
	for (i = 0; i < 2; i++)
	{
		TestConsoleType(&sConsole, "help\r");
		AssertTrue(TestConsoleRunUntil(&sConsole, "Anything else is a program to run, with a number for its argument.\n/ $\n", &szScreen));
	}
	AssertTrue(pcBlitter->NumBytesCopied() > uiText + 10 * 16 * 640);
	uiText = pcBlitter->NumBytesCopied();

	//sprites' picture, drawn by the board: the stage is 640 by 400.
	puiStage = TestSpritesReadStage();
	AssertTrue(puiStage != NULL);
	TestConsoleSprites(&sConsole, puiStage);
	free(puiStage);
	AssertTrue(pcBlitter->NumBytesCopied() > uiText + 640 * 400);
	AssertFalse(pcBlitter->IsCopying());
	AssertFalse(sConsole.cHCP65.IsContention());
	TestConsoleType(&sConsole, "\x1B");
	AssertTrue(TestConsoleRunUntil(&sConsole, "/ $\n", &szScreen));

	AssertFalse(sConsole.cHCP65.IsStopped());
	szScreen.Kill();
	sConsole.cKeyboard.Kill();
	free(sConsole.puiFrame);
	pcSDCard->RemoveCard(TEST_HCP65_CARD_SOCKET);
	sConsole.cHCP65.Kill();
	sConsole.cMainboard.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The first line of the LCD, its 16 characters, into szLine; the column
// its "HCP65" starts at, -1 if it isn't on it once among spaces.
//
//////////////////////////////////////////////////////////////////////////
static int TestLCDLine(CHCP65LCDBoard* pcLCD, char* szLine)
{
	int		i;
	int		iColumn;
	char*	szFound;

	for (i = 0; i < HCP65_LCD_COLUMNS; i++)
	{
		szLine[i] = (char)pcLCD->GetDDRAM((uint8)i);
	}
	szLine[HCP65_LCD_COLUMNS] = '\0';
	szFound = strstr(szLine, "HCP65");
	if (!szFound)
	{
		return -1;
	}
	iColumn = (int)(szFound - szLine);
	for (i = 0; i < HCP65_LCD_COLUMNS; i++)
	{
		if (((i < iColumn) || (i >= iColumn + 5)) && (szLine[i] != ' '))
		{
			return -1;
		}
	}
	return iColumn;
}


//////////////////////////////////////////////////////////////////////////
//
// The console's machine with an LCD and SNES Board (Machines/LCD.xml).
// Its driver is loaded from \OS\Drivers\LCD.DRV and sets the display up:
// on, two lines, blank.  lcdtest (\LCDTEST.EXE, LCDTest.cpp) bounces
// HCP65 along the first line, a character at a time, from column 0 to 11
// and back (a line is looked at as it is being written now and then: it is
// skipped); a key ends it and the display is cleared.  The driver never
// gives the display anything while it is busy.  sprites' Maki is walked
// by the first SNES controller.
//
//////////////////////////////////////////////////////////////////////////
void TestHCP65KernalLCD(STestKernal* psKernal)
{
	STestConsole		sConsole;
	STestKernalStatus	sStatus;
	CHCP65SDCard*		pcSDCard;
	CHCP65LCDBoard*		pcLCD;
	SHCP65LCDPicture	sPicture;
	CChars				szScreen;
	char				szLine[HCP65_LCD_COLUMNS + 1];
	int					iColumn;
	int					iLast;
	int					iMax;
	int					iTorn;
	bool				bBack;
	uint8*				puiStage;
	int					iStart;
	int					iMoved;
	int					iRight;
	int					i;

	AssertTrue(TestConsoleReadFont(sConsole.auiFont));
	if (!TestKernalPutSystemFiles())
	{
		return;
	}
	sConsole.uiStatus = TEST_KERNAL_STATUS_ADDRESS;
	sConsole.puiFrame = (uint32*)malloc(1024 * 768 * sizeof(uint32));

	sConsole.cHCP65.Init(TEST_UART_CLOCK_RATE);
	if (!TestKernalPlugIn(&sConsole.cMainboard, &sConsole.cHCP65, TEST_HCP65_LCD_MACHINE))
	{
		free(sConsole.puiFrame);
		sConsole.cHCP65.Kill();
		sConsole.cMainboard.Kill();
		return;
	}
	pcSDCard = (CHCP65SDCard*)TestKernalBoard(&sConsole.cMainboard, TEST_HCP65_SD_SOCKET, HCP65_SD_NAME);
	TestKernalBoard(&sConsole.cMainboard, TEST_HCP65_MEMORY_SOCKET, HCP65_MEMORY_BOARD_NAME);
	sConsole.pcVideo = (CHCP65VideoBoard*)TestKernalBoard(&sConsole.cMainboard, TEST_HCP65_VIDEO_SOCKET, HCP65_VIDEO_BOARD_NAME);
	sConsole.pcPS2 = (CHCP65PS2Board*)TestKernalBoard(&sConsole.cMainboard, TEST_HCP65_PS2_SOCKET, HCP65_PS2_BOARD_NAME);
	pcLCD = (CHCP65LCDBoard*)TestKernalBoard(&sConsole.cMainboard, TEST_HCP65_LCD_SOCKET, HCP65_LCD_BOARD_NAME);
	sConsole.cKeyboard.Init();
	sConsole.cKeyboard.Attach(sConsole.pcPS2->GetPort(0));
	sConsole.pcPS2->GetPort(1)->SetPlugged(false);
	TestKernalStart(&sConsole.cHCP65, psKernal);

	//The driver is loaded and has set the display up: on and blank.
	szScreen.Init();
	AssertTrue(TestConsoleRunUntil(&sConsole, TEST_HCP65_SHELL_BANNER "\n/ $\n", &szScreen));
	TestKernalReadStatus(sConsole.cHCP65.GetMPURAM(), sConsole.uiStatus, &sStatus);
	AssertInt((1 << TEST_HCP65_VIDEO_SOCKET) | (1 << TEST_HCP65_PS2_SOCKET) | (1 << TEST_HCP65_LCD_SOCKET), sStatus.uiLoadedDrivers);
	pcLCD->GetPicture(&sPicture, sConsole.cHCP65.NumCycles());
	AssertTrue(sPicture.bOn);
	AssertInt(-1, TestLCDLine(pcLCD, szLine));
	AssertString("                ", szLine);

	//lcdtest: HCP65 goes right a column at a time to 11, then back.
	TestConsoleType(&sConsole, "lcdtest\r");
	iLast = -1;
	iMax = -1;
	iTorn = 0;
	bBack = false;
	for (i = 0; (i < 4000) && !bBack; i++)
	{
		sConsole.cHCP65.Run(TEST_UART_RUN_CYCLES);
		iColumn = TestLCDLine(pcLCD, szLine);
		if (iLast == -1)
		{
			if (iColumn != -1)
			{
				AssertInt(0, iColumn);
				iLast = iColumn;
			}
			continue;
		}
		if (iColumn == -1)
		{
			//The line is being written, a character at a time.
			iTorn++;
			continue;
		}
		AssertTrue(iColumn <= HCP65_LCD_COLUMNS - 5);
		AssertTrue((iColumn == iLast) || (iColumn == iLast + 1) || (iColumn == iLast - 1));
		if (iColumn > iMax)
		{
			iMax = iColumn;
		}
		if (iColumn < iLast)
		{
			AssertInt(HCP65_LCD_COLUMNS - 5, iMax);
			bBack = true;
		}
		iLast = iColumn;
	}
	AssertTrue(bBack);
	AssertInt(10, iLast);
	AssertTrue(iTorn < 10);

	//A key ends it, and the display is blank again.
	TestConsoleType(&sConsole, " ");
	AssertTrue(TestConsoleRunUntil(&sConsole, "/ $\n", &szScreen));
	AssertInt(-1, TestLCDLine(pcLCD, szLine));
	AssertString("                ", szLine);
	AssertLong(0, (int64)pcLCD->NumIgnored());

	//sprites: the first controller walks Maki right while Right is held, then left while Left is, and she stands
	//still when nothing is.  Without a Blitter Board a frame takes the processor longer than one of the screen's, so
	//she steps about every third: 0.6s of a direction is about 25 pixels.
	puiStage = TestSpritesReadStage();
	AssertTrue(puiStage != NULL);
	TestConsoleSprites(&sConsole, puiStage);
	AssertTrue(TestSpritesFrame(&sConsole, puiStage, &iStart, &iRight));
	pcLCD->SetButtons(0, HCP65_SNES_RIGHT);
	for (i = 0; i < 60; i++)
	{
		sConsole.cHCP65.Run(TEST_UART_RUN_CYCLES);
	}
	AssertTrue(TestSpritesFrame(&sConsole, puiStage, &iMoved, &iRight));
	AssertTrue(iMoved > iStart + 20);
	pcLCD->SetButtons(0, 0);
	for (i = 0; i < 30; i++)
	{
		sConsole.cHCP65.Run(TEST_UART_RUN_CYCLES);
	}
	AssertTrue(TestSpritesFrame(&sConsole, puiStage, &iStart, &iRight));
	for (i = 0; i < 20; i++)
	{
		sConsole.cHCP65.Run(TEST_UART_RUN_CYCLES);
	}
	AssertTrue(TestSpritesFrame(&sConsole, puiStage, &iMoved, &iRight));
	AssertInt(iStart, iMoved);
	pcLCD->SetButtons(0, HCP65_SNES_LEFT);
	for (i = 0; i < 20; i++)
	{
		sConsole.cHCP65.Run(TEST_UART_RUN_CYCLES);
	}
	AssertTrue(TestSpritesFrame(&sConsole, puiStage, &iStart, &iRight));	//Facing left now: her frames are the other way round.
	for (i = 0; i < 60; i++)
	{
		sConsole.cHCP65.Run(TEST_UART_RUN_CYCLES);
	}
	AssertTrue(TestSpritesFrame(&sConsole, puiStage, &iMoved, &iRight));
	AssertTrue(iMoved < iStart - 20);
	pcLCD->SetButtons(0, 0);
	free(puiStage);
	TestConsoleType(&sConsole, "\x1B");
	AssertTrue(TestConsoleRunUntil(&sConsole, "/ $\n", &szScreen));

	AssertFalse(sConsole.cHCP65.IsStopped());
	szScreen.Kill();
	sConsole.cKeyboard.Kill();
	free(sConsole.puiFrame);
	pcSDCard->RemoveCard(TEST_HCP65_CARD_SOCKET);
	sConsole.cHCP65.Kill();
	sConsole.cMainboard.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Whether the kernal test szName (TestHCP65Kernal<szName>) runs: all of
// them do unless TEST_HCP65_KERNAL_ONLY names one.
//
//////////////////////////////////////////////////////////////////////////
static bool TestKernalRuns(const char* szName)
{
	const char*	szOnly;

	szOnly = getenv("TEST_HCP65_KERNAL_ONLY");
	return !szOnly || (strcmp(szOnly, szName) == 0);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void TestHCP65Kernal(void)
{
	STestKernal	sKernal;

	BeginTests();
	DataMemoryInit();  //The machines' XML.

	sKernal.cBoot.Init(HKI_Boot);
	sKernal.cOS.Init(HKI_OS);
	if (TestKernalBuild(&sKernal))
	{
		if (TestKernalRuns("Boot"))
		{
			TestHCP65KernalBoot(&sKernal);
		}
		if (TestKernalRuns("UART"))
		{
			TestHCP65KernalUART(&sKernal);
		}
		if (TestKernalRuns("VIA"))
		{
			TestHCP65KernalVIA(&sKernal);
		}
		if (TestKernalRuns("PS2"))
		{
			TestHCP65KernalPS2(&sKernal);
		}
		if (TestKernalRuns("Shell"))
		{
			TestHCP65KernalShell(&sKernal);
		}
		if (TestKernalRuns("Console"))
		{
			TestHCP65KernalConsole(&sKernal);
		}
		if (TestKernalRuns("Blitter"))
		{
			TestHCP65KernalBlitter(&sKernal);
		}
		if (TestKernalRuns("LCD"))
		{
			TestHCP65KernalLCD(&sKernal);
		}
		if (TestKernalRuns("Video"))
		{
			TestHCP65KernalVideo(&sKernal);
		}
		if (TestKernalRuns("NoMainMemory"))
		{
			TestHCP65KernalNoMainMemory(&sKernal);
		}
	}
	sKernal.cOS.Kill();
	sKernal.cBoot.Kill();

	DataMemoryKill();
	TestStatistics();
}
