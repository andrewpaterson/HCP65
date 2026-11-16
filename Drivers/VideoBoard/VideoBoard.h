#ifndef __VIDEO_BOARD_H__
#define __VIDEO_BOARD_H__
#include "KernalDriver.h"
#include "UserLib/HCP65VideoBoard.h"


//The board's registers, from its first ($400 in the slot).
#define VIDEO_FLAGS						0x00
#define VIDEO_CLEAR_INTERRUPT			0x01
#define VIDEO_CLOCK_SELECT				0x05
#define VIDEO_HORIZONTAL				0x06	//Front Porch Start, Sync Start, Back Porch Start, Back Porch End: in 8 pixels.
#define VIDEO_PATTERN_LOW				0x0A
#define VIDEO_PATTERN_HIGH				0x0B
#define VIDEO_VERTICAL					0x0C	//The same four, low then high byte each.
#define VIDEO_PALETTE_BYTE				0x16
#define VIDEO_SIGNAL_DELAY				0x17

#define VIDEO_FLAG_VSYNC_NEGATIVE		0x01
#define VIDEO_FLAG_HSYNC_NEGATIVE		0x02
#define VIDEO_FLAG_MEMORY_FRAME			0x04	//0: the processor has frame 0 and frame 1 is shown.
#define VIDEO_FLAG_PALETTE_WRITE		0x08
#define VIDEO_FLAG_ENABLE				0x40
#define VIDEO_FLAG_INTERRUPT			0x80

#define VIDEO_CLOCK_ENABLE				0x08
#define VIDEO_DELAY_OFF					0x80

//The board's device RAM, from the slot's first address.
#define VIDEO_ROW_TABLE					0x04000
#define VIDEO_PALETTES					0x08000

#define VIDEO_WINDOW_SIZE				0x400000	//A main memory window: chip A (the indices), then chip B (the palette numbers).


//A mode's timing: in 8 pixels across and lines down, each Front Porch Start (the width or height), Sync Start, Back
//Porch Start and Back Porch End (the whole).
struct SVideoMode
{
	uint8	uiClockSelect;
	uint8	uiSyncFlags;
	uint8	auiHorizontal[4];
	uint16	auiVertical[4];
};


//The HCP65 Video Board's driver: its units are the board's memories (VIDEO_BOARD_PIXELS ...), and a program has the
//whole board.  The pixels and their palette numbers are the board's main memory window (the frame the processor has);
//the palettes and the row table its device RAM.  The interrupt (trusted: only the board and the driver's own memory) is
//vertical blank's: it counts them and makes a flip asked for since the last.  Without a window from the kernal (or with
//window 0, whose pixels aren't reachable in kernel mode) the pixels can't be reached: their reads and writes fail.
class CKernalVideoBoard : public CKernalDriver
{
protected:
	uint8			muiFlags;		//What Flags was last written.
	uint8			muiMode;		//VIDEO_MODE_*, 0xFF if none.
	uint16			muiWidth;
	uint16			muiHeight;
	volatile bool	mbFlip;			//A flip is due at the next vertical blank.
	volatile uint32	muiVBlanks;
	uint32			muiWindow;		//The main memory window's first address, 0 if it can't be reached.

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
	bool	IsWaitOver(uint16 uiUnit, uint32 uiValue) override;

	uint8*	UnitMemory(uint16 uiUnit, uint32 uiOffset, uint32 uiLength);
	void	SetMode(uint8 uiMode);
	void	Off(void);
	void	WriteFlags(uint8 uiFlags);
};


#define VIDEO_BOARD_MAX_BOARDS			1		//Video boards it drives.


extern "C" CKernalDriver*	KernalCreateDriver(uint16 uiIndex);


#endif // __VIDEO_BOARD_H__
