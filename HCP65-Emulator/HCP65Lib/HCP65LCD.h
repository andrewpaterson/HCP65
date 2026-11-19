#ifndef __HCP65_LCD_H__
#define __HCP65_LCD_H__
#include "BaseLib/Chars.h"
#include "BaseLib/Mutex.h"
#include "HCP65Device.h"


#define HCP65_LCD_BOARD_NAME			"LCD and SNES Board"
#define HCP65_LCD_BOARD_DRIVER			"LCD.DRV"
#define HCP65_LCD_REGISTERS				0x400

//Registers, from $400.  They repeat every 16 bytes up to $7FF; $40C-$40F decode nothing.
//
//$400-$407 are the display's pins: an access to one (read or write) latches its A0 onto E, A1 onto RS and A2 onto R/~W,
//and a write latches its byte onto DB0-DB7 while R/~W is low.  A read with R/~W high and E high is what the display
//drives; one with R/~W low is the latched byte; otherwise nothing drives the bus.
#define HCP65_LCD_E						0x01
#define HCP65_LCD_RS					0x02
#define HCP65_LCD_READ					0x04
#define HCP65_LCD_COMMAND				0x00	//W: E low, an instruction.
#define HCP65_LCD_COMMAND_E				0x01	//W: E high.
#define HCP65_LCD_DATA					0x02	//W: E low, data.
#define HCP65_LCD_DATA_E				0x03	//W: E high.
#define HCP65_LCD_STATUS				0x04	//R: E low.
#define HCP65_LCD_STATUS_E				0x05	//R: E high, the busy flag (D7) and the address counter.
#define HCP65_LCD_READ_DATA				0x06	//R: E low.
#define HCP65_LCD_READ_DATA_E			0x07	//R: E high, the data at the address counter.
#define HCP65_LCD_SNES_1_LOW			0x08	//R: controller 1's A, X, L and R in D0-D3, D4-D7 high.  0 is pressed.
#define HCP65_LCD_SNES_1_HIGH			0x09	//R: controller 1's B, Y, Select, Start, Up, Down, Left and Right in D0-D7.
#define HCP65_LCD_SNES_2_LOW			0x0A	//R: controller 2's, the same.
#define HCP65_LCD_SNES_2_HIGH			0x0B

//A controller's buttons, set when pressed, in the order it sends them.
#define HCP65_SNES_B					0x0001
#define HCP65_SNES_Y					0x0002
#define HCP65_SNES_SELECT				0x0004
#define HCP65_SNES_START				0x0008
#define HCP65_SNES_UP					0x0010
#define HCP65_SNES_DOWN					0x0020
#define HCP65_SNES_LEFT					0x0040
#define HCP65_SNES_RIGHT				0x0080
#define HCP65_SNES_A					0x0100
#define HCP65_SNES_X					0x0200
#define HCP65_SNES_L					0x0400
#define HCP65_SNES_R					0x0800
#define HCP65_SNES_BUTTONS				0x0FFF
#define HCP65_SNES_CONTROLLERS			2
#define HCP65_SNES_XML_TAG				"Controller"	//A controller's keys in the board's <Board> tag (LoadXML).

#define HCP65_LCD_COLUMNS				16
#define HCP65_LCD_ROWS					2
#define HCP65_LCD_CHARACTER_HEIGHT		8		//Rows of dots in a character, the cursor's the last.
#define HCP65_LCD_CHARACTER_WIDTH		5
#define HCP65_LCD_DDRAM_SIZE			80
#define HCP65_LCD_CGRAM_SIZE			64

//The controller's times, in microseconds, at 270kHz (the datasheet's execution times; its power on reset and blink
//aren't given, so they are an HD44780U's).
#define HCP65_LCD_POWER_ON_US			10000	//Busy with its own reset after power on.
#define HCP65_LCD_CLEAR_US				1530	//Clear display and return home.
#define HCP65_LCD_INSTRUCTION_US		39		//The other instructions.
#define HCP65_LCD_DATA_US				43		//Writing or reading data.
#define HCP65_LCD_BLINK_US				409600	//The blinking cursor's time on, and off.


//What the display shows: each character's eight rows of dots, the leftmost dot in D4.
struct SHCP65LCDPicture
{
	bool	bOn;
	uint8	aauiDots[HCP65_LCD_ROWS][HCP65_LCD_COLUMNS][HCP65_LCD_CHARACTER_HEIGHT];
};


//The display's controller, a KS0066 (or compatible: it works like an HD44780U) with the character ROM in the display's
//datasheet (HCP65/Datasheets/NMTC-S16205DRGHS.pdf), as a 16 by 2 display of 5 by 8 dot characters.  Its time is in the
//HCP65's cycles.
class CHCP65LCDController
{
protected:
	uint8	mauiDDRAM[HCP65_LCD_DDRAM_SIZE];
	uint8	mauiCGRAM[HCP65_LCD_CGRAM_SIZE];
	uint8	muiAddress;				//The address counter.
	bool	mbCGRAM;				//The address counter is a CGRAM address.
	bool	mbIncrement;			//Entry mode I/D.
	bool	mbShiftOnWrite;			//Entry mode S.
	bool	mbDisplayOn;
	bool	mbCursor;
	bool	mbBlink;
	bool	mbEightBit;				//Function set DL.
	bool	mbTwoLines;				//Function set N.
	bool	mbLargeFont;			//Function set F: 5 by 10 dots, drawn as 5 by 8.
	uint8	muiShift;				//How far the display is shifted left, 0 to 39 (79 on one line).
	bool	mbLowNibble;			//Four bit: the next transfer is the low four bits.
	uint8	muiHighNibble;			//Four bit: the high four bits written.
	bool	mbE;
	bool	mbRS;
	bool	mbRead;
	bool	mbDataRead;				//Data was read while E was high: the address counter moves when E goes low.
	uint8	muiDataRegister;		//What a data read gives.
	uint64	muiBusyUntil;
	uint64	muiCyclesPerSecond;
	uint64	muiIgnored;				//Instructions and data written while busy, and dropped.

public:
	void	Init(uint64 uiCyclesPerSecond);
	void	PowerOn(uint64 uiCycle);
	void	SetPins(bool bE, bool bRS, bool bRead, uint8 uiDB, uint64 uiCycle);
	bool	IsDriving(void) const;
	uint8	GetOutput(uint64 uiCycle);
	bool	IsBusy(uint64 uiCycle) const;
	uint8	GetAddress(void) const;
	uint8	GetDDRAM(uint8 uiAddress) const;
	uint8	GetCGRAM(uint8 uiAddress) const;
	uint64	NumIgnored(void) const;
	void	GetPicture(SHCP65LCDPicture* psPicture, uint64 uiCycle) const;

protected:
	void	Transfer(bool bRS, uint8 uiValue, uint64 uiCycle);
	void	Instruction(uint8 uiValue, uint64 uiCycle);
	void	WriteData(uint8 uiValue);
	uint8	ReadData(void) const;
	void	MoveAddress(bool bIncrement);
	void	ShiftDisplay(bool bLeft);
	uint8	DDRAMIndex(uint8 uiAddress) const;
	uint8	Glyph(uint8 uiCode, int iRow) const;
	void	Busy(uint64 uiCycle, uint32 uiMicroseconds);
};


//The HCP65 LCD and SNES Board (LCD sheets, and the SNES sheets for its daughter boards): a 16 by 2 character display
//(Microtips NMTC-S16205DRGHS-07, grey and reflective, no backlight) and two SNES controller ports.  A 32KB device with no
//interrupt; nothing enables it.
//
//The board as it is meant to work (the schematic has faults: Kicad/Computer/LCD/LCD Board/LCD Board Faults.txt):
//  - The display's pins are latched from the registers' addresses ($400-$407).  To write, write the byte to an address
//    with E high, then drop E: the display takes DB on E's fall.  The access that drops E latches its own byte too, at the
//    same moment, so drop it with a read (or write the same byte again): here the display takes the byte from before.
//    To read, set R/~W with E low, then read with E high.
//  - The controller is busy after each instruction (HCP65_LCD_*_US) and ignores what it is given meanwhile.  It isn't
//    reset by the HCP65: only by power.  A data read gives its data register (CHCP65LCDController::GetOutput).
//  - The SNES clock reads both controllers all the time (every 256 device clocks) into latches the registers read.
//    A reset holds the clock and the latches keep what they had.  An unplugged controller reads as nothing pressed.
//
//Which of the host's keys drive each controller is the emulator's (HCP65Emulator's key mappings), kept with the board
//by name: LoadXML reads <Controller Port="n" Keys="name"/> (n 1 or 2) and SaveXML writes one for each controller that
//has a name.  No name is a controller that isn't plugged in.
class CHCP65LCDBoard : public CHCP65Device
{
protected:
	CDeviceROM			mcROM;
	CHCP65LCDController	mcController;
	CMutex				mcMutex;			//The controller, under it while the window reads the picture.
	uint8				muiPins;			//E, RS and R/~W as HCP65_LCD_E, HCP65_LCD_RS and HCP65_LCD_READ.
	uint8				muiData;			//The byte latched for DB.
	uint64				muiCycle;			//The cycle being accessed (CatchUp).
	bool				mbPoweredOn;
	uint16				mauiButtons[HCP65_SNES_CONTROLLERS];	//Pressed, as HCP65_SNES_*.
	uint16				mauiLatched[HCP65_SNES_CONTROLLERS];	//What the latches hold, as the buttons.
	CChars				maszKeys[HCP65_SNES_CONTROLLERS];		//Each controller's key mapping's name, empty for none.

public:
	void	Init(void);
	void	Kill(void) override;
	bool	LoadXML(CMarkupTag* pcTag, const char* szDirectory, CChars* pszError) override;
	void	SaveXML(CXMLWriter* pcWriter) override;

	uint8	Read(uint32 uiOffset) override;
	void	Write(uint32 uiOffset, uint8 uiValue) override;
	void	Reset(void) override;
	void	CatchUp(uint64 uiCycle) override;

	void	SetButtons(int iController, uint16 uiButtons);
	void		SetKeys(int iController, const char* szKeys);
	const char*	GetKeys(int iController);
	void	GetPicture(SHCP65LCDPicture* psPicture, uint64 uiCycle);
	bool	IsBusy(void);
	uint8	GetDDRAM(uint8 uiAddress);
	uint64	NumIgnored(void);

protected:
	uint8	Access(uint32 uiRegister, bool bWrite, uint8 uiValue);
	void	PowerOn(void);
	uint8	ReadController(int iController, bool bHigh);
};


#endif // __HCP65_LCD_H__
