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

//Text.
#define VIDEO_TEXT_CELL_WIDTH			8
#define VIDEO_TEXT_CELL_HEIGHT			16
#define VIDEO_TEXT_MAX_COLUMNS			128			//1024x768's.
#define VIDEO_TEXT_MAX_ROWS				48
#define VIDEO_TEXT_MAX_CELLS			(VIDEO_TEXT_MAX_COLUMNS * VIDEO_TEXT_MAX_ROWS)
#define VIDEO_TEXT_MAX_PENDING			2048		//Cells owed to the other frame before it is all drawn again.
#define VIDEO_TEXT_GLYPH_SIZE			(VIDEO_TEXT_CELL_WIDTH * VIDEO_TEXT_CELL_HEIGHT)	//A glyph expanded: a byte a pixel.
#define VIDEO_TEXT_FONT_OFFSET			0x1F0000	//Where the expanded font is in each frame: its last 64KB, far above the picture.
#define VIDEO_TEXT_NO_CLEAR				0xFF		//mauiRowClears: the row isn't owed a clear.
#define VIDEO_TEXT_DEFAULT_COLOUR		0x07		//Light grey on black.
#define VIDEO_TEXT_BLINK_FRAMES			16			//The cursor's blink: about a quarter of a second on, then off.
#define VIDEO_TEXT_FLIP_TICKS			16			//The longest a mode change waits for a vertical blank (KERNAL_TICKS_PER_SECOND).
#define VIDEO_TEXT_CHUNK				64			//Characters WriteText takes from the process at a time.


//A mode's timing: in 8 pixels across and lines down, each Front Porch Start (the width or height), Sync Start, Back
//Porch Start and Back Porch End (the whole).
struct SVideoMode
{
	uint8	uiClockSelect;
	uint8	uiSyncFlags;
	uint8	auiHorizontal[4];
	uint16	auiVertical[4];
};


//The HCP65 Video Board's driver: its units are the board's memories (VIDEO_BOARD_PIXELS ...) and its text
//(VIDEO_BOARD_TEXT, VIDEO_BOARD_FONT: VideoBoardText.cpp), and a program has the whole board.  The pixels and their
//palette numbers are the board's main memory window (the frame the processor has); the palettes and the row table its
//device RAM.  The interrupt (trusted: only the board and the driver's own memory) is
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

	//Text.  A cell's physical row is its row moved down by the scroll, muiTop (the row table shows physical row muiTop
	//first); a cell is its physical row (D8-D15) and column (D0-D7).  What each cell is (mauiChars, mauiColours, at
	//mauiRowCells[row] + column) is drawn in the frame being drawn, and what the other frame is owed is kept to draw in
	//it once the frames have flipped (mbFlipped): rows to clear (mauiRowClears, the colour), then cells (mauiPending,
	//or all of them).  A cell is drawn from its glyph expanded to a byte a pixel, which is in each frame's own memory
	//at VIDEO_TEXT_FONT_OFFSET (the driver must fit a bank), by VideoBoardRender.asm.
	bool			mbText;
	uint16			muiColumns;
	uint16			muiRows;
	uint16			muiTop;
	uint16			muiShownTop;	//The row table's.
	uint8			muiTextColour;
	uint16			muiCursor;		//Its physical cell, VIDEO_CURSOR_NONE if none.
	uint8			muiBlink;
	uint16			muiPending;
	bool			mbPendingAll;
	volatile bool	mbFlipped;
	uint16			mauiRowCells[VIDEO_TEXT_MAX_ROWS + 1];		//Each row's first cell in mauiChars: row x columns.
	uint32			mauiRowPixels[VIDEO_TEXT_MAX_ROWS + 1];	//Each row's first pixel in a frame: row x 16 x width.
	uint8			mauiRowClears[VIDEO_TEXT_MAX_ROWS];
	uint8			mauiChars[VIDEO_TEXT_MAX_CELLS];
	uint8			mauiColours[VIDEO_TEXT_MAX_CELLS];
	uint16			mauiPending[VIDEO_TEXT_MAX_PENDING];
	uint8			mauiFont[VIDEO_FONT_SIZE];

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

	bool	SetTextMode(uint8 uiMode);
	void	SetTextPalette(uint8 uiPalette, uint8 uiBackground, uint8 uiForeground);
	void	ClearTextFrame(void);
	void	ExpandFont(void);
	void	WriteTextRows(uint16 uiTop);
	uint16	PhysicalCell(uint32 uiCell);
	uint16	PhysicalRow(uint16 uiRow);
	void	RenderCell(uint16 uiCell);
	void	RenderRowClear(uint16 uiRow, uint8 uiColour);
	void	CatchUp(void);
	void	DrawCell(uint16 uiCell);
	void	ClearRow(uint16 uiRow);
	uint16	WriteText(uint32 uiCell, uint32 uiUserAddress, uint32 uiLength, uint32* puiDone);
	uint16	ScrollText(uint32 uiLines);
	uint16	ClearText(void);
	void	SetCursorCell(uint16 uiCell);
	void	TextBlank(bool bFlipped);
};


#define VIDEO_BOARD_MAX_BOARDS			1		//Video boards it drives.


extern "C" CKernalDriver*	KernalCreateDriver(uint16 uiIndex);


#endif // __VIDEO_BOARD_H__
