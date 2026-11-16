#ifndef __KERNAL_START_UP_H__
#define __KERNAL_START_UP_H__
#include "BaseLib/FatVolume.h"
#include "KernalSDCard.h"


//What the boot loader and the kernal both do as they start (KernalStartUp.cpp): clear the status, find the devices,
//take main memory, start the interrupts and the SD Card Boards' drivers (the ROM's, built into both), and mount the
//first card with a FAT volume.
extern CKernalSDCardBoard	gacKernalSDCardBoards[KERNAL_SD_MAX_BOARDS];
extern CFatVolume			gcKernalVolume;


bool	KernalStartUp(void);
void	KernalStopDrivers(void);


#endif // __KERNAL_START_UP_H__
