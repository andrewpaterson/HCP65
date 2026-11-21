#ifndef __HCP65_USER_H__
#define __HCP65_USER_H__
#include "BaseLib/PrimitiveTypes.h"


//Kernal calls for a program the HCP65 kernal runs (UserCalls.asm).  A program exits by returning from main (or by
//calling exit).  Paths are FAT paths on the SD card ("\\OS\\INIT.EXE").

#define KERNAL_RESULT_SUCCESS		0
#define KERNAL_RESULT_NOT_FOUND		1		//No such file.
#define KERNAL_RESULT_BAD_FILE		2		//Not an executable, or a file that couldn't be read.
#define KERNAL_RESULT_NO_MEMORY		3
#define KERNAL_RESULT_NO_ENTRY		4		//An executable without UserStart.
#define KERNAL_RESULT_EXITED		5		//KernalExecute: the program ran and exited.
#define KERNAL_RESULT_NO_PROCESS	6		//KernalExecute: all four process IDs are in use.
#define KERNAL_RESULT_BAD_ADDRESS	7		//An executable with code or data where its stack or heap is (below bank 1, from
										//$200000, or in banks $08 to $0F without a UserHeapSize).  Or memory a call
										//was given that the program hasn't.
#define KERNAL_RESULT_END			8		//KernalDirectoryEntry: there are no more entries.

#define KERNAL_FILE_NOT_FOUND		0xFFFFFFFF

//Garth Wilson's maths tables, in main memory where every process has them (UserMathTables.asm).  The start up program
//(the one \OS\STARTUP.TXT names) loads them from the SD card before it runs anything built with them (a program
//without MathTables="false"): all of MULT, and the upper half of INVERT (the reciprocals of $8000 to $FFFF, the soft float's); the file has all of it.
#define KERNAL_MATH_MULT_PATH		"\\OS\\Math\\MULT.BIN"
#define KERNAL_MATH_MULT_ADDRESS	0x400000
#define KERNAL_MATH_MULT_SIZE		0x20000
#define KERNAL_MATH_INVERT_PATH		"\\OS\\Math\\INVERT.BIN"
#define KERNAL_MATH_INVERT_OFFSET	0x20000		//Where the upper half starts in the file.
#define KERNAL_MATH_INVERT_ADDRESS	0x420000
#define KERNAL_MATH_INVERT_SIZE		0x20000


//Gives the kernal a value it keeps in its status (for tests).
extern "C" void		KernalPutValue(uint16 uiValue);

//Runs the executable szPath as a new process and waits for it to exit.  It gets uiArgument (KernalGetArgument).
//Returns KERNAL_RESULT_EXITED with its exit code in *puiExitCode, or why it couldn't run.
extern "C" uint16	KernalExecute(const char* szPath, uint16 uiArgument, uint16* puiExitCode);

//The argument the process was run with (the start up program's is the number after its path in \OS\STARTUP.TXT, 0
//without one).
extern "C" uint16	KernalGetArgument(void);

//The size of the file szPath, or KERNAL_FILE_NOT_FOUND.
extern "C" uint32	KernalFileSize(const char* szPath);

//Reads up to uiMaxSize bytes of the file szPath to pvDest.  KERNAL_RESULT_SUCCESS if it read the whole file.
extern "C" uint16	KernalFileLoad(const char* szPath, void* pvDest, uint32 uiMaxSize);

//Reads the uiSize bytes of the file szPath from uiOffset to pvDest.  KERNAL_RESULT_SUCCESS, KERNAL_RESULT_NOT_FOUND,
//or KERNAL_RESULT_BAD_FILE if the file doesn't have them all.
extern "C" uint16	KernalFileRead(const char* szPath, uint32 uiOffset, void* pvDest, uint32 uiSize);

//Directories and files beyond reading them (UserFiles.cpp, COP 13): an entry of a directory, a file written whole
//(made, or emptied first), a file deleted, a directory made.  Each returns KERNAL_RESULT_SUCCESS, or
//KERNAL_RESULT_NOT_FOUND (KernalDirectoryEntry: it isn't a directory) or KERNAL_RESULT_BAD_FILE.
#define KERNAL_FILE_NAME_LENGTH		64
#define KERNAL_FILE_READ_ONLY		0x01		//SKernalDirectoryEntry's attributes (FAT's).
#define KERNAL_FILE_HIDDEN			0x02
#define KERNAL_FILE_SYSTEM			0x04
#define KERNAL_FILE_DIRECTORY		0x10
#define KERNAL_FILE_ARCHIVE			0x20

struct SKernalDirectoryEntry
{
	char	szName[KERNAL_FILE_NAME_LENGTH];
	uint32	uiSize;
	uint8	uiAttributes;		//KERNAL_FILE_*.
};

//The uiIndex'th entry (from 0, without "." and "..") of the directory szPath.  KERNAL_RESULT_END after the last.
extern "C" uint16	KernalDirectoryEntry(const char* szPath, uint16 uiIndex, SKernalDirectoryEntry* psEntry);
extern "C" uint16	KernalFileWrite(const char* szPath, const void* pvSource, uint32 uiSize);
extern "C" uint16	KernalFileDelete(const char* szPath);
extern "C" uint16	KernalMakeDirectory(const char* szPath);

//The kernal's memory.
struct SKernalMemoryInfo
{
	uint16	uiMainBanks;		//64KB banks of main memory, all of it.
	uint16	uiProcessBanks;		//Those processes can have.
	uint16	uiFreeBanks;		//Those no process has.
	uint32	uiHeapSize;			//The kernal's own heap (MPU RAM).
	uint32	uiHeapUsed;
};

extern "C" uint16	KernalMemoryInfo(SKernalMemoryInfo* psInfo);

//The program's heap: banks of main memory the kernal maps into it as it asks for them, from KERNAL_HEAP_START up to
//KERNAL_HEAP_END but the maths tables' (KERNAL_MATH_MULT_ADDRESS to KERNAL_MATH_INVERT_ADDRESS +
//KERNAL_MATH_INVERT_SIZE).  The kernal says so in the block at KERNAL_HEAP_INFO_ADDRESS, which Libc65 reads; the
//banks of the program's UserHeapSize are already mapped when it starts.
#define KERNAL_HEAP_INFO_ADDRESS	0xFF0300
#define KERNAL_HEAP_MARKER			0x50414548		//"HEAP": the heap's banks are mapped as the program asks for them.
#define KERNAL_HEAP_START			0x200000
#define KERNAL_HEAP_END				0xFF0000		//Bank $FF is the program's, where this block is.
#define KERNAL_HEAP_BANK_SIZE		0x10000

struct SKernalHeapInfo
{
	uint32	uiMemorySize;	//+0   The end of the banks mapped at load (KERNAL_HEAP_START + UserHeapSize), or 0.
	uint32	uiMarker;		//+4   KERNAL_HEAP_MARKER.
	uint32	uiHeapStart;	//+8   The first byte of the heap.
	uint32	uiHeapEnd;		//+12  One past its last.
};

//Maps uiBanks banks of the heap from the one at pvBank (the first byte of a bank) into the program, or none of them:
//KERNAL_RESULT_NO_MEMORY if that would take main memory's last free bank (the kernal keeps one for itself), and
//KERNAL_RESULT_BAD_ADDRESS if they aren't all the heap's.  Banks the program already has are left as they are.  What
//was in a bank before isn't cleared.
extern "C" uint16	KernalMapBanks(void* pvBank, uint16 uiBanks);

//Gives uiBanks banks of the heap from the one at pvBank back to the kernal (those the program has).
//KERNAL_RESULT_BAD_ADDRESS if they aren't all the heap's.
extern "C" uint16	KernalUnmapBanks(void* pvBank, uint16 uiBanks);

//How the system calls reach the kernal: one block, which the kernal reads and writes in the program's memory.
#define KERNAL_SYSTEM_CALL_DIRECTORY		0
#define KERNAL_SYSTEM_CALL_FILE_WRITE		1
#define KERNAL_SYSTEM_CALL_FILE_DELETE		2
#define KERNAL_SYSTEM_CALL_MAKE_DIRECTORY	3
#define KERNAL_SYSTEM_CALL_MEMORY			4
#define KERNAL_SYSTEM_CALL_MAP_BANKS		5
#define KERNAL_SYSTEM_CALL_UNMAP_BANKS		6

struct SKernalSystemCall
{
	uint16	uiCall;			//+0   KERNAL_SYSTEM_CALL_*.
	uint16	uiIndex;		//+2   Directory: the entry.  Map and unmap banks: how many.
	uint32	uiPath;			//+4   The path (in the program's memory), or 0.
	uint32	uiAddress;		//+8   Directory and Memory: where the answer goes.  File write: the bytes.  Map and unmap
							//     banks: the first bank's.
	uint32	uiLength;		//+12  File write: how many.
	uint32	uiResult;		//+16
};

extern "C" uint16	KernalSystemCall(SKernalSystemCall* psCall);

//Devices: the boards in the device sockets, through their drivers (the kernal's, or a driver it loaded from
//\OS\Drivers).  A program opens a unit of a board and gets a handle, which the other calls take; it has the whole
//board until it closes its last handle on it (or exits): no other program can open it meanwhile.  What a board's
//units, offsets, control codes and wait values mean is its driver's (HCP65SoundBoard.h for the sound board).
//
//Every program also has the standard handles, which it needn't open (and can't close): the console's input and output
//(the Video Board's text, or else the UART Board's TTL port; the PS2 Board's keyboard, or else that port).  Output is
//text, '\n' starting the next line.  Input is what is typed, without waiting (Read can give nothing) and without being
//shown: KernalDeviceWait with KERNAL_CONSOLE_WAIT_INPUT waits for it.  Enter is '\r', Backspace 8, and arrows and the
//like a VT100's escape sequences (ESC [ A is up).  Opened handles are from KERNAL_FIRST_DEVICE_HANDLE.
#define KERNAL_STANDARD_INPUT			0
#define KERNAL_STANDARD_OUTPUT			1
#define KERNAL_FIRST_DEVICE_HANDLE		2

//The console's control codes (KernalDeviceControl on KERNAL_STANDARD_OUTPUT).
#define KERNAL_CONSOLE_GET_COLUMNS		0
#define KERNAL_CONSOLE_GET_ROWS			1
#define KERNAL_CONSOLE_CLEAR			2		//The screen blank, the cursor at the top left.
#define KERNAL_CONSOLE_SET_MODE			3		//The Video Board's mode (VIDEO_MODE_* in HCP65VideoBoard.h): 0 80x30, 1 100x37, 2 128x48.
#define KERNAL_CONSOLE_SET_COLOUR		4		//Foreground (D0-D3) and background (D4-D6) of what is written next: KERNAL_COLOUR_*.

#define KERNAL_CONSOLE_WAIT_INPUT		0		//KernalDeviceWait on KERNAL_STANDARD_INPUT: something has been typed.

//The console's colours (a VGA's sixteen).  The background is one of the first eight.
#define KERNAL_COLOUR_BLACK				0
#define KERNAL_COLOUR_BLUE				1
#define KERNAL_COLOUR_GREEN				2
#define KERNAL_COLOUR_CYAN				3
#define KERNAL_COLOUR_RED				4
#define KERNAL_COLOUR_MAGENTA			5
#define KERNAL_COLOUR_BROWN				6
#define KERNAL_COLOUR_LIGHT_GREY		7
#define KERNAL_COLOUR_DARK_GREY			8
#define KERNAL_COLOUR_BRIGHT_BLUE		9
#define KERNAL_COLOUR_BRIGHT_GREEN		10
#define KERNAL_COLOUR_BRIGHT_CYAN		11
#define KERNAL_COLOUR_BRIGHT_RED		12
#define KERNAL_COLOUR_BRIGHT_MAGENTA	13
#define KERNAL_COLOUR_YELLOW			14
#define KERNAL_COLOUR_WHITE				15
#define KERNAL_CONSOLE_DEFAULT_COLOUR	KERNAL_COLOUR_LIGHT_GREY
#define KERNAL_DEVICE_SUCCESS			0
#define KERNAL_DEVICE_NOT_SUPPORTED		1		//The board's driver doesn't do that.
#define KERNAL_DEVICE_BAD_UNIT			2
#define KERNAL_DEVICE_IN_USE			3		//Another program has the board (or the unit) open.
#define KERNAL_DEVICE_NOT_OPEN			4
#define KERNAL_DEVICE_BAD_ARGUMENT		5
#define KERNAL_DEVICE_FAILED			6
#define KERNAL_DEVICE_TIMEOUT			7		//KernalDeviceWait: what it waited for didn't happen in time.
#define KERNAL_DEVICE_NOT_FOUND			8		//No such board, no driver for it, or no board at that index.
#define KERNAL_DEVICE_NO_HANDLES		9		//The kernal has no handles left.
#define KERNAL_DEVICE_BAD_HANDLE		10		//Not a handle of this program's.

#define KERNAL_DEVICE_NAME_LENGTH		64
#define KERNAL_DEVICE_DRIVER_LENGTH		32
#define KERNAL_DEVICE_TICKS_PER_SECOND	256		//KernalDeviceWait's ticks.

//A board in a device socket (KernalDeviceInfo): its device ROM's description.
struct SKernalDeviceInfo
{
	uint16	uiSlot;
	uint16	uiNumUnits;									//0 if it has no driver.
	uint16	uiRevision;
	uint16	uiOpen;										//Not 0: a program has it open.
	char	szName[KERNAL_DEVICE_NAME_LENGTH];
	char	szManufacturer[KERNAL_DEVICE_NAME_LENGTH];
	char	szDriverFile[KERNAL_DEVICE_DRIVER_LENGTH];	//The driver loaded for it from \OS\Drivers, "" if it is built in or none.
};

//The uiIndex'th board in the device sockets (from 0, in socket order).  KERNAL_DEVICE_NOT_FOUND after the last.
extern "C" uint16	KernalDeviceInfo(uint16 uiIndex, SKernalDeviceInfo* psInfo);

//Whether a program has the uiIndex'th board made by szManufacturer called szName open (from 0, in socket order);
//false if there isn't one.
extern "C" bool		KernalDeviceIsOpen(const char* szManufacturer, const char* szName, uint16 uiIndex);

//Opens unit uiUnit of the uiIndex'th board made by szManufacturer called szName (from 0, in socket order).
//*puiHandle is the handle.
extern "C" uint16	KernalDeviceOpen(const char* szManufacturer, const char* szName, uint16 uiIndex, uint16 uiUnit, uint16* puiHandle);

//Closes the handle.  The unit is let go when the program's last handle on it is closed.
extern "C" uint16	KernalDeviceClose(uint16 uiHandle);

//Moves up to uiLength bytes between the program's memory and the unit at uiOffset.  *puiDone (if not NULL) is how
//many it moved: a unit that streams (a UART) moves what it has, which may be none.
extern "C" uint16	KernalDeviceRead(uint16 uiHandle, uint32 uiOffset, void* pvDest, uint32 uiLength, uint32* puiDone);
extern "C" uint16	KernalDeviceWrite(uint16 uiHandle, uint32 uiOffset, const void* pvSource, uint32 uiLength, uint32* puiDone);

//The driver's control uiCode with uiValue; *puiResult (if not NULL) is what it gives back.
extern "C" uint16	KernalDeviceControl(uint16 uiHandle, uint16 uiCode, uint32 uiValue, uint32* puiResult);

//Waits up to uiTicks (KERNAL_DEVICE_TICKS_PER_SECOND) for what uiValue says, sleeping.  KERNAL_DEVICE_TIMEOUT if it
//didn't happen.
extern "C" uint16	KernalDeviceWait(uint16 uiHandle, uint32 uiValue, uint16 uiTicks);


//How the device calls reach the kernal (UserDevices.cpp, COP 12 in UserCalls.asm): one block, which the kernal reads
//and writes in the program's memory.
#define KERNAL_DEVICE_CALL_INFO			0
#define KERNAL_DEVICE_CALL_OPEN			1
#define KERNAL_DEVICE_CALL_CLOSE		2
#define KERNAL_DEVICE_CALL_READ			3
#define KERNAL_DEVICE_CALL_WRITE		4
#define KERNAL_DEVICE_CALL_CONTROL		5
#define KERNAL_DEVICE_CALL_WAIT			6

struct SKernalDeviceCall
{
	uint16	uiCall;			//+0   KERNAL_DEVICE_CALL_*.
	uint16	uiHandle;		//+2   Open: the board's index.  Info: the index.
	uint16	uiUnit;			//+4
	uint16	uiCode;			//+6   Control: the code.  Wait: the ticks.
	uint32	uiValue;		//+8   Read and Write: the offset.  Control and Wait: the value.
	uint32	uiAddress;		//+12  Read, Write and Info: the program's memory.  Open: the manufacturer.
	uint32	uiLength;		//+16  Read and Write: the length.  Open: the name.
	uint32	uiResult;		//+20  Control: what it gives back.  Open: the handle.  Read and Write: the bytes moved.
};

extern "C" uint16	KernalDeviceCall(SKernalDeviceCall* psCall);


//The screen: the picture the computer shows, which is the kernal's (it is drawn on the Video Board, and by a Blitter
//Board when there is one to give the drawing to).  It shows the console's text until a program asks for a mode
//(KernalScreenSetMode): then it is that program's picture until the program gives it back (KernalScreenSetText) or
//ends, and what is written to the console meanwhile isn't shown.  KERNAL_DEVICE_IN_USE while another program has it;
//KERNAL_DEVICE_NOT_OPEN for what needs a mode from a program that hasn't set one.  The results are KERNAL_DEVICE_*.
//
//The picture is two frames: one is shown while the other is drawn, and KernalScreenFlip swaps them at the next
//vertical blank (KernalScreenWaitBlank waits for it).  A pixel is a byte at y x width + x in the frame being drawn
//(KernalScreenWrite, KernalScreenRead): the colour it shows is its palette's (KernalScreenSetColours gives a palette
//its 256 reds, then greens, then blues).  A pixel takes the palette set (KernalScreenSetPalette) as it is drawn.
#define KERNAL_SCREEN_640_480			0			//60Hz.
#define KERNAL_SCREEN_800_600			1
#define KERNAL_SCREEN_1024_768			2
#define KERNAL_SCREEN_NUM_PALETTES		128
#define KERNAL_SCREEN_NO_PALETTE		0xFF		//KernalScreenSetPalette: pixels keep the palette they have.
#define KERNAL_SCREEN_COLOURS_SIZE		768

//A blit: an image drawn with its top left at (iX, iY).  The image is an opaque or a transparent SFT image (HCP65SFT.h)
//in the program's memory, of which no more than uiSize bytes are read.  What is outside the picture isn't drawn, nor
//with KERNAL_BLIT_CLIP what is outside the rectangle from (iClipLeft, iClipTop) to just before (iClipRight,
//iClipBottom).  KERNAL_DEVICE_BAD_ARGUMENT for what isn't an image or is cut short (what came before the fault has
//been drawn).
//
//With KERNAL_BLIT_SPANS (and KERNAL_BLIT_CLIP) an opaque image is drawn only inside a span of each row of the clip
//rectangle: uiSpans is the address of a pair of words for each of its rows from the top, the first pixel across the
//rectangle drawn and the one just past the last (the same for none).  It is how what a sprite covered is drawn again
//from a picture: only the part of each row that the sprite was in, a copy a row.  A transparent image ignores it.
#define KERNAL_BLIT_CLIP				0x0001
#define KERNAL_BLIT_SPANS				0x0002

struct SKernalBlit
{
	uint32	uiImage;		//+0
	uint32	uiSize;			//+4
	int16	iX;				//+8
	int16	iY;				//+10
	uint16	uiFlags;		//+12  KERNAL_BLIT_*.
	int16	iClipLeft;		//+14
	int16	iClipTop;		//+16
	int16	iClipRight;		//+18
	int16	iClipBottom;	//+20
	uint32	uiSpans;		//+22  KERNAL_BLIT_SPANS: each row's span, two words a row.
};

extern "C" uint16	KernalScreenSetMode(uint16 uiMode);
extern "C" uint16	KernalScreenSetText(void);
extern "C" uint16	KernalScreenGetSize(uint16* puiWidth, uint16* puiHeight);
extern "C" uint16	KernalScreenBlit(const SKernalBlit* psBlit);
extern "C" uint16	KernalScreenFlip(void);
extern "C" uint16	KernalScreenWaitBlank(uint16 uiTicks);
extern "C" uint16	KernalScreenSetColours(uint16 uiPalette, const uint8* puiColours);
extern "C" uint16	KernalScreenSetPalette(uint16 uiPalette);
extern "C" uint16	KernalScreenWrite(uint32 uiOffset, const void* pvSource, uint32 uiLength);
extern "C" uint16	KernalScreenRead(uint32 uiOffset, void* pvDest, uint32 uiLength);


//How the screen calls reach the kernal (UserScreen.cpp, COP 14 in UserCalls.asm): one block in the program's memory.
#define KERNAL_SCREEN_CALL_SET_MODE		0
#define KERNAL_SCREEN_CALL_SET_TEXT		1
#define KERNAL_SCREEN_CALL_GET_SIZE		2
#define KERNAL_SCREEN_CALL_BLIT			3
#define KERNAL_SCREEN_CALL_FLIP			4
#define KERNAL_SCREEN_CALL_WAIT_BLANK	5
#define KERNAL_SCREEN_CALL_SET_COLOURS	6
#define KERNAL_SCREEN_CALL_SET_PALETTE	7
#define KERNAL_SCREEN_CALL_WRITE		8
#define KERNAL_SCREEN_CALL_READ			9

struct SKernalScreenCall
{
	uint16	uiCall;			//+0   KERNAL_SCREEN_CALL_*.
	uint16	uiValue;		//+2   The mode, the palette or the ticks.
	uint32	uiOffset;		//+4   Write and Read: the first pixel.
	uint32	uiAddress;		//+8   The program's memory: the blit, the colours or the pixels.
	uint32	uiLength;		//+12  Write and Read: the pixels.
	uint32	uiResult;		//+16  Get Size: the width, and the height in its high word.
};

extern "C" uint16	KernalScreenCall(SKernalScreenCall* psCall);


#endif // __HCP65_USER_H__
