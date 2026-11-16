#ifndef __VIA_BOARD_H__
#define __VIA_BOARD_H__
#include "KernalDriver.h"
#include "UserLib/HCP65VIABoard.h"


#define VIA_BOARD_REGISTERS_SIZE	0x10	//Each VIA's, from the slot's device registers ($400): A6-A4 pick the VIA.
#define VIA_BOARD_INTERRUPTS		0x80	//The interrupt register: bit n is 1 where VIA n is interrupting.


//The HCP65 VIA Board's driver: its units are the eight VIAs (VIA_BOARD_AB to VIA_BOARD_OP), each opened by its own
//process (mbSeparateUnits).  Init finds which are fitted: a VIA that isn't reads $FF.  IER is the driver's: Wait turns
//on the interrupts of the flags it waits for, and the interrupt, which is trusted (it only touches the VIAs), turns off
//those of each interrupting VIA's flags that are set, leaving the flags for the process.
class CKernalVIABoard : public CKernalDriver
{
protected:
	uint8	muiFitted;		//Bit n where VIA n is.

public:
	uint16	Init(SKernalDevice* psDevice, uint16 uiSlot) override;
	void	Kill(void) override;

	uint16	NumUnits(void) override;
	void	Interrupt(void) override;

	uint16	Read(uint16 uiUnit, uint32 uiOffset, uint32 uiUserAddress, uint32 uiLength, uint32* puiDone) override;
	uint16	Write(uint16 uiUnit, uint32 uiOffset, uint32 uiUserAddress, uint32 uiLength, uint32* puiDone) override;
	uint16	Control(uint16 uiUnit, uint16 uiCode, uint32 uiValue, uint32* puiResult) override;
	uint16	Wait(uint16 uiUnit, uint32 uiValue, uint16 uiTicks) override;

protected:
	uint16			OpenUnit(uint16 uiUnit) override;
	void			CloseUnit(uint16 uiUnit) override;
	bool			IsWaitOver(uint16 uiUnit, uint32 uiValue) override;
	volatile uint8*	GetVIA(uint16 uiUnit);
	bool			IsFitted(uint16 uiUnit);
	void			Quieten(uint16 uiUnit);
	bool			IsRange(uint32 uiOffset, uint32 uiLength);
};


#define VIA_BOARD_MAX_BOARDS		2		//VIA boards it drives.


extern "C" CKernalDriver*	KernalCreateDriver(uint16 uiIndex);


#endif // __VIA_BOARD_H__
