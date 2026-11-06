#ifndef __KERNAL_SOUND_BOARD_H__
#define __KERNAL_SOUND_BOARD_H__
#include "KernalDriver.h"


//The sound board's driver (HCP65\Drivers\SoundBoard, loaded from \OS\Drivers\SOUND.DRV): what the kernal's audio
//calls ask of it.
#define KERNAL_SOUND_BOARD_NAME			"Sound Board"

//Units: the sample RAMs.
#define KERNAL_SOUND_LEFT				0		//RAM A.
#define KERNAL_SOUND_RIGHT				1		//RAM B.
#define KERNAL_SOUND_RAM_SIZE			0x8000	//Write's offsets wrap at its end.

//Control codes: 0 to 7 write the register (HCP65_AUDIO_FLAGS ... HCP65_AUDIO_INTERRUPT_HIGH) with uiValue's low byte.
#define KERNAL_SOUND_SET_REGISTER		0
#define KERNAL_SOUND_NUM_REGISTERS		8
#define KERNAL_SOUND_GET_COUNTER		8		//*puiResult: the address of the samples played next.


#endif // __KERNAL_SOUND_BOARD_H__
