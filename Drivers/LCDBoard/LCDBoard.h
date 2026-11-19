#ifndef __LCD_BOARD_H__
#define __LCD_BOARD_H__
#include "KernalDriver.h"
#include "UserLib/HCP65LCDBoard.h"


//The board's registers, from its first ($400 in the slot).  An access to $00-$07, read or write, sets the display's
//pins: E from A0, RS from A1 and R/~W from A2; a write also latches its byte for the display's data pins.
#define LCD_COMMAND					0x00	//E low, an instruction.
#define LCD_COMMAND_E				0x01	//E high.
#define LCD_DATA					0x02	//E low, data.
#define LCD_DATA_E					0x03
#define LCD_STATUS					0x04	//R/~W high, E low: the busy flag and address counter.
#define LCD_STATUS_E				0x05
#define LCD_READ_DATA				0x06	//R/~W high, E low: data.
#define LCD_READ_DATA_E				0x07
#define LCD_SNES_1_LOW				0x08	//R: controller 1's A, X, L and R in D0-D3, D4-D7 high.  0 is pressed.
#define LCD_SNES_1_HIGH				0x09	//R: its B, Y, Select, Start, Up, Down, Left and Right.
#define LCD_SNES_SIZE				2		//Each controller's registers: controller 2's follow.

#define LCD_BUSY					0x80	//The status's busy flag.

//The display's instructions.
#define LCD_INSTRUCTION_CLEAR		0x01
#define LCD_INSTRUCTION_ENTRY		0x06	//Entry mode: increment, no shift.
#define LCD_INSTRUCTION_DISPLAY_ON	0x0C	//On, no cursor, no blink.
#define LCD_INSTRUCTION_FUNCTION	0x38	//Eight bit, two lines, 5 by 8 dots.
#define LCD_INSTRUCTION_ADDRESS		0x80	//Set the DDRAM address: or'd with it.
#define LCD_LINE_2					0x40	//The second line's first DDRAM address.

#define LCD_MAX_POLLS				20000	//The busy flag is read this many times before the display is given up on:
											//well past its 10ms reset after power on.
#define LCD_BOARD_MAX_BOARDS		2		//LCD boards it drives.


//The HCP65 LCD and SNES Board's driver.  Its units are the display and the two SNES controllers.  Init sets the display
//up by instruction (eight bit, two lines, on, cleared); if the display never stops being busy, it is given up on and
//can't be opened.  A controller's buttons are its two latches, the bits turned round so that pressed is 1.
//
//The display is driven as the board's notes say (Kicad/Computer/LCD/LCD Board/LCD Board Faults.txt): RS and R/~W are
//set with E low first, E is dropped with a read so the latched byte doesn't change as it falls, and a byte read with E
//high is read twice, the second time after the display has had a whole access to put it out.  Each transfer waits for
//the busy flag, spinning: the longest, a clear, is 1.53ms.
class CKernalLCDBoard : public CKernalDriver
{
protected:
	bool	mbReady;		//The display answered when it was set up.
	uint8	muiTouched;		//What the last read for the pins it sets (Touch) gave.

public:
	uint16	Init(SKernalDevice* psDevice, uint16 uiSlot) override;
	uint16	NumUnits(void) const override;

	uint16	Read(uint16 uiUnit, uint32 uiOffset, uint32 uiUserAddress, uint32 uiLength, uint32* puiDone) override;
	uint16	Write(uint16 uiUnit, uint32 uiOffset, uint32 uiUserAddress, uint32 uiLength, uint32* puiDone) override;
	uint16	Control(uint16 uiUnit, uint16 uiCode, uint32 uiValue, uint32* puiResult) override;

protected:
	uint16	OpenUnit(uint16 uiUnit) override;
	void	Touch(uint8 uiRegister);
	bool	WaitReady(void);
	bool	Send(uint8 uiRegister, uint8 uiValue);
	bool	ReadByte(uint8* puiValue);
	bool	SetAddress(uint32 uiPosition);
	uint16	ReadController(uint16 uiUnit, uint32 uiOffset, uint32 uiUserAddress, uint32 uiLength, uint32* puiDone);
	bool	IsRange(uint32 uiOffset, uint32 uiLength);
};


extern "C" CKernalDriver*	KernalCreateDriver(uint16 uiIndex);


#endif // __LCD_BOARD_H__
