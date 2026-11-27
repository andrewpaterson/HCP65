#ifndef __KERNAL_H__
#define __KERNAL_H__
#include "BaseLib/PrimitiveTypes.h"


//The HCP65 starts in two stages (Boot/Docs/hcp65/boot.md).  The boot loader in the ROM (HCP65/Boot) brings up the
//devices and the SD card, loads \OS\OS.SYS into MPU RAM and jumps to it.  OS.SYS (this kernal) starts the devices again,
//loads each board's driver and runs the program \OS\STARTUP.TXT names.
//Design page: Docs/hcp65/design/kernal.md
#define KERNAL_OS_FILE					"\\OS\\OS.SYS"
#define KERNAL_OS_ADDRESS				0x00FF00	//OS.SYS is the bytes from here: bank 0's stubs and vectors, then banks 1 to 3.
#define KERNAL_OS_MAX_SIZE				0x30100		//To the end of bank 3.
#define KERNAL_STARTUP_FILE				"\\OS\\STARTUP.TXT"	//Its first line: the program's path, then its argument (decimal), if any.
#define KERNAL_DEFAULT_STARTUP			"\\OS\\INIT.EXE"	//Without STARTUP.TXT.
#define KERNAL_STARTUP_LENGTH			128			//The longest first line STARTUP.TXT may have.

#define KERNAL_STATE_STARTING			0x0001
#define KERNAL_STATE_DEVICES_SCANNED	0x0002
#define KERNAL_STATE_NO_SD_CARD_BOARD	0x0003
#define KERNAL_STATE_SD_CARD_FAILED		0x0004
#define KERNAL_STATE_MOUNT_FAILED		0x0005
#define KERNAL_STATE_MOUNTED			0x0006
#define KERNAL_STATE_BOOT_FAILED		0x0007		//The start up program couldn't be run.
#define KERNAL_STATE_BOOT_EXITED		0x0008		//The start up program ran and exited.
#define KERNAL_STATE_NO_MAIN_MEMORY		0x0009		//No memory board gave enough for the tables and the heap.
#define KERNAL_STATE_NO_OS				0x000A		//The boot loader: OS.SYS isn't on the card, or isn't an OS (uiBootResult says which).
#define KERNAL_STATE_OS_STARTED			0x000B		//The boot loader jumped to OS.SYS, which hasn't started yet.

//uiBootResult in KERNAL_STATE_NO_OS.
#define KERNAL_OS_NOT_FOUND				1
#define KERNAL_OS_BAD_SIZE				2
#define KERNAL_OS_READ_FAILED			3
#define KERNAL_OS_BAD_ENTRY				4			//Its first byte isn't a jml (KERNAL_OS_ADDRESS is its entry).


//What the boot loader and then the kernal found starting up, for tests and debuggers to read.  It is at
//KERNAL_STATUS_ADDRESS (bank 0, between the stack and the stubs), the same in both, so that a reader needn't know which
//is running: each clears it as it starts.
struct SKernalStatus
{
	uint16	uiState;		//KERNAL_STATE_*.
	uint16	uiSDError;		//KERNAL_SD_* when the card failed.
	uint16	uiFatCode;		//The EFatCode Mount returned.
	uint16	uiFileSystem;	//EFatFileSystemType.
	uint32	uiNumSectors;
	uint32	uiNumClusters;
	uint32	uiFreeClusters;
	uint16	uiBootResult;	//KERNAL_PROCESS_* for the start up program; KERNAL_OS_* in KERNAL_STATE_NO_OS.
	uint16	uiExitCode;		//The start up program's exit code.
	uint16	uiUserValue;	//The last value a process gave the kernal (COP 1).
	uint16	uiBootArgument;	//The start up program's argument (KernalGetArgument), from STARTUP.TXT.
	uint16	uiSDCards;		//Bit n set: the card in socket n came up.
	uint16	uiSDSocket;		//The socket whose card's volume is mounted.
	uint32	uiStrayInterrupts;	//Bit n set: device n interrupted with no handler, and was reset (KernalInterrupts.asm).
	uint32	uiDevices;			//Bit n set: socket n holds a device whose ROM describes it (KERNAL_DEVICE_PRESENT).
	uint32	uiBadDevices;		//Bit n set: socket n holds a board whose ROM isn't usable or that is in the wrong socket.
	uint32	uiLoadedDrivers;	//Bit n set: socket n's board's driver was loaded from the SD card (KernalDriverLoader.h).
	uint16	uiDriverError;		//Why the last driver that didn't load didn't (KERNAL_LOAD_*), 0 if they all did.
	uint32	uiHeapPeak;			//The most of the kernal's heap (MPU RAM) used, once the start up program has exited.
	uint32	uiMainHeapPeak;		//The most of its main memory heap used, then.
	uint16	uiMainBanks;		//The banks of main memory the memory boards gave (KernalMemory.h).
};

#define KERNAL_STATUS_ADDRESS			0x00C000	//KernalInterrupts.asm uses it too.
#define gsKernalStatus					(*(SKernalStatus*)KERNAL_STATUS_ADDRESS)


#endif // __KERNAL_H__

