#ifndef __HCP65_USER_H__
#define __HCP65_USER_H__
#include "BaseLib/PrimitiveTypes.h"


//Kernal calls for a program the HCP65 kernal runs (UserCalls.asm).  A program exits by returning from main (or by
//calling exit).  Paths are FAT paths on the SD card ("\\boot.exe").

#define KERNAL_RESULT_SUCCESS		0
#define KERNAL_RESULT_NOT_FOUND		1		//No such file.
#define KERNAL_RESULT_BAD_FILE		2		//Not an executable, or a file that couldn't be read.
#define KERNAL_RESULT_NO_MEMORY		3
#define KERNAL_RESULT_NO_ENTRY		4		//An executable without UserStart.
#define KERNAL_RESULT_EXITED		5		//KernalExecute: the program ran and exited.
#define KERNAL_RESULT_NO_PROCESS	6		//KernalExecute: all four process IDs are in use.
#define KERNAL_RESULT_BAD_ADDRESS	7		//An executable with code or data where its stack or heap is (below bank 1, from
										//$200000, or in banks $08 to $0F without a UserHeapSize).

#define KERNAL_FILE_NOT_FOUND		0xFFFFFFFF

//Garth Wilson's maths tables, in main memory where every process has them (UserMathTables.asm).  boot.exe loads them
//from the SD card before it runs anything built with them (a program without MathTables="false"): all of MULT, and
//the upper half of INVERT (the reciprocals of $8000 to $FFFF, the soft float's); the file has all of it.
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

//The argument the process was run with (boot.exe's is the kernal's boot argument, zero unless a test sets it).
extern "C" uint16	KernalGetArgument(void);

//The size of the file szPath, or KERNAL_FILE_NOT_FOUND.
extern "C" uint32	KernalFileSize(const char* szPath);

//Reads up to uiMaxSize bytes of the file szPath to pvDest.  KERNAL_RESULT_SUCCESS if it read the whole file.
extern "C" uint16	KernalFileLoad(const char* szPath, void* pvDest, uint32 uiMaxSize);

//Reads the uiSize bytes of the file szPath from uiOffset to pvDest.  KERNAL_RESULT_SUCCESS, KERNAL_RESULT_NOT_FOUND,
//or KERNAL_RESULT_BAD_FILE if the file doesn't have them all.
extern "C" uint16	KernalFileRead(const char* szPath, uint32 uiOffset, void* pvDest, uint32 uiSize);

//Devices: the boards in the device sockets, through their drivers (the kernal's, or a driver it loaded from
//\OS\Drivers).  A program opens a unit of a board and gets a handle, which the other calls take; it has the whole
//board until it closes its last handle on it (or exits): no other program can open it meanwhile.  What a board's
//units, offsets, control codes and wait values mean is its driver's (HCP65SoundBoard.h for the sound board).
#define KERNAL_DEVICE_SUCCESS			0
#define KERNAL_DEVICE_NOT_SUPPORTED		1		//The board's driver doesn't do that.
#define KERNAL_DEVICE_BAD_UNIT			2
#define KERNAL_DEVICE_IN_USE			3		//Another program has the board open.
#define KERNAL_DEVICE_NOT_OPEN			4
#define KERNAL_DEVICE_BAD_ARGUMENT		5
#define KERNAL_DEVICE_FAILED			6
#define KERNAL_DEVICE_TIMEOUT			7		//KernalDeviceWait: what it waited for didn't happen in time.
#define KERNAL_DEVICE_NOT_FOUND			8		//No such board, no driver for it, or no board at that index.
#define KERNAL_DEVICE_NO_HANDLES		9		//The kernal has no handles left.
#define KERNAL_DEVICE_BAD_HANDLE		10		//Not a handle of this program's.

#define KERNAL_DEVICE_NAME_LENGTH		64

//A board in a device socket (KernalDeviceInfo): its device ROM's description.
struct SKernalDeviceInfo
{
	uint16	uiSlot;
	uint16	uiNumUnits;									//0 if it has no driver.
	uint16	uiRevision;
	uint16	uiOpen;										//Not 0: a program has it open.
	char	szName[KERNAL_DEVICE_NAME_LENGTH];
	char	szManufacturer[KERNAL_DEVICE_NAME_LENGTH];
};

//The uiIndex'th board in the device sockets (from 0, in socket order).  KERNAL_DEVICE_NOT_FOUND after the last.
extern "C" uint16	KernalDeviceInfo(uint16 uiIndex, SKernalDeviceInfo* psInfo);

//Opens unit uiUnit of the uiIndex'th board made by szManufacturer called szName (from 0, in socket order).
//*puiHandle is the handle.
extern "C" uint16	KernalDeviceOpen(const char* szManufacturer, const char* szName, uint16 uiIndex, uint16 uiUnit, uint16* puiHandle);

//Closes the handle.  The board is let go when the program's last handle on it is closed.
extern "C" uint16	KernalDeviceClose(uint16 uiHandle);

//Moves uiLength bytes between the program's memory and the unit at uiOffset.
extern "C" uint16	KernalDeviceRead(uint16 uiHandle, uint32 uiOffset, void* pvDest, uint32 uiLength);
extern "C" uint16	KernalDeviceWrite(uint16 uiHandle, uint32 uiOffset, const void* pvSource, uint32 uiLength);

//The driver's control uiCode with uiValue; *puiResult (if not NULL) is what it gives back.
extern "C" uint16	KernalDeviceControl(uint16 uiHandle, uint16 uiCode, uint32 uiValue, uint32* puiResult);

//Waits up to uiTicks (256 a second) for what uiValue says, sleeping.  KERNAL_DEVICE_TIMEOUT if it didn't happen.
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
	uint32	uiResult;		//+20  Control: what it gives back.  Open: the handle.
};

extern "C" uint16	KernalDeviceCall(SKernalDeviceCall* psCall);


#endif // __HCP65_USER_H__
