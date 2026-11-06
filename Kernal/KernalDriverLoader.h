#ifndef __KERNAL_DRIVER_LOADER_H__
#define __KERNAL_DRIVER_LOADER_H__
#include "BaseLib/PrimitiveTypes.h"
#include "KernalDriver.h"


#define KERNAL_DRIVER_DIRECTORY			"\\OS\\Drivers\\"		//Where a board's device ROM's driver file is.
#define KERNAL_DRIVER_ENTRY				"_KernalCreateDriver"	//extern "C" CKernalDriver* KernalCreateDriver(void).
#define KERNAL_DRIVER_MEMORY_END		0x070000				//Drivers go in MPU RAM after the kernal, up to the remap table.

//What loading a board's driver came to.
#define KERNAL_LOAD_SUCCESS				0
#define KERNAL_LOAD_NOT_FOUND			1		//No such file.
#define KERNAL_LOAD_BAD_FILE			2		//Not a driver module.
#define KERNAL_LOAD_NO_MEMORY			3		//No room in MPU RAM.
#define KERNAL_LOAD_MISSING_EXPORT		4		//It uses a name the kernal doesn't export.
#define KERNAL_LOAD_BAD_RELOCATION		5		//A field it can't be written in where it was put.
#define KERNAL_LOAD_NO_DRIVER			6		//Its create function gave no driver, or the driver couldn't drive the board.


//A name a loaded driver may use and what it is (KernalExports.asm).
struct SKernalExport
{
	const char*	szName;
	void*		pvAddress;
};


//The table (KernalExports.asm): its first byte and how many exports it has.
extern "C" const uint8			gauiKernalExports;
extern "C" const uint16			guiKernalNumExports;


void*	KernalFindExport(const char* szName);
uint16	KernalLoadDriver(uint16 uiSlot);


#endif // __KERNAL_DRIVER_LOADER_H__
