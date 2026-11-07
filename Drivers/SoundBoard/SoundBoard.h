#ifndef __SOUND_BOARD_H__
#define __SOUND_BOARD_H__
#include "KernalDriver.h"
#include "UserLib/HCP65SoundBoard.h"


//The HCP65 Sound Board's driver: what a program asks of a board it opens (UserLib/HCP65SoundBoard.h).  Its units are
//the two sample RAMs: Write copies samples into one; Control writes the registers and reads the counter; Wait waits
//for the counter to reach the address uiValue (its interrupt).
class CKernalSoundBoard : public CKernalDriver
{
protected:
	volatile uint8	mbInterrupt;

public:
	uint16	Init(SKernalDevice* psDevice, uint16 uiSlot) override;

	uint16	NumUnits(void) override;
	void	Interrupt(void) override;

	uint16	Write(uint16 uiUnit, uint32 uiOffset, uint32 uiUserAddress, uint32 uiLength, uint32* puiDone) override;
	uint16	Control(uint16 uiUnit, uint16 uiCode, uint32 uiValue, uint32* puiResult) override;
	uint16	Wait(uint16 uiUnit, uint32 uiValue, uint16 uiTicks) override;

protected:
	bool	IsWaitOver(uint16 uiUnit, uint32 uiValue) override;
	uint16	CopyFromUser(uint8* puiDest, uint32 uiAddress, uint16 uiLength);
};


#define SOUND_BOARD_MAX_BOARDS			4		//Sound boards it drives.


extern "C" CKernalDriver*	KernalCreateDriver(uint16 uiIndex);


#endif // __SOUND_BOARD_H__
