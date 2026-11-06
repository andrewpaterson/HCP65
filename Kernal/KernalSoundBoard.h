#ifndef __KERNAL_SOUND_BOARD_H__
#define __KERNAL_SOUND_BOARD_H__
#include "KernalDriver.h"


#define KERNAL_SOUND_BOARD_NAME			"Sound Board"

//Units: the sample RAMs.
#define KERNAL_SOUND_LEFT				0		//RAM A.
#define KERNAL_SOUND_RIGHT				1		//RAM B.
#define KERNAL_SOUND_RAM_SIZE			0x8000	//Write's offsets wrap at its end.

//Control codes: 0 to 7 write the register (HCP65_AUDIO_FLAGS ... HCP65_AUDIO_INTERRUPT_HIGH) with uiValue's low byte.
#define KERNAL_SOUND_SET_REGISTER		0
#define KERNAL_SOUND_NUM_REGISTERS		8
#define KERNAL_SOUND_GET_COUNTER		8		//*puiResult: the address of the samples played next.


//The HCP65 Sound Board (HCP65Lib/HCP65Audio.h): two 32KB sample RAMs played together, RAM A left and RAM B right.
//Write copies samples into a RAM; Wait waits for the counter to reach the address uiValue (its interrupt).
class CKernalSoundBoard : public CKernalDriver
{
protected:
	volatile uint8	mbInterrupt;

public:
	uint16	Init(SKernalDevice* psDevice, uint16 uiSlot) override;

	uint16	NumUnits(void) override;
	void	Interrupt(void) override;

	uint16	Write(uint16 uiUnit, uint32 uiOffset, uint32 uiUserAddress, uint32 uiLength) override;
	uint16	Control(uint16 uiUnit, uint16 uiCode, uint32 uiValue, uint32* puiResult) override;
	uint16	Wait(uint16 uiUnit, uint32 uiValue, uint16 uiTicks) override;

protected:
	void	CopyFromUser(uint8* puiDest, uint32 uiAddress, uint16 uiLength);
};


#endif // __KERNAL_SOUND_BOARD_H__
