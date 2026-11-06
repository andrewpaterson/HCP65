#ifndef __SOUND_BOARD_H__
#define __SOUND_BOARD_H__
#include "KernalSoundBoard.h"


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


#define SOUND_BOARD_MAX_BOARDS			4		//Sound boards it drives.


extern "C" CKernalDriver*	KernalCreateDriver(uint16 uiIndex);


#endif // __SOUND_BOARD_H__
