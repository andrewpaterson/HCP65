#ifndef __VIDEO_BOARD_H__
#define __VIDEO_BOARD_H__
#include "KernalDriver.h"
#include "KernalMemory.h"
#include "UserLib/HCP65User.h"
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
#define VIDEO_TEXT_MAX_PENDING			1024		//Cells owed to the other frame before it is all drawn again.
#define VIDEO_TEXT_GLYPHS				256
#define VIDEO_TEXT_PIXEL				0xFF		//The palette entry that is a cell's foreground: what a glyph's pixels are.
#define VIDEO_TEXT_NO_CLEAR				0xFF		//mauiRowClears: the row isn't owed a clear.
#define VIDEO_CLEAR_SIZE				1024		//gpuiVideoClear: the widest mode's line.
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


//What the driver's assembly (VideoBoardRender.asm) works on with the driver: the one board's.
//
//Text.  A cell's physical row is its row moved down by the scroll, muiTop (the row table shows physical row muiTop
//first); a cell is its physical row (D8-D15) and column (D0-D7).  What each cell is (gauiVideoChars, gauiVideoCellColours,
//at gauiVideoRowCells[row] + column) is drawn in the frame being drawn, and what the other frame is owed is kept to
//draw in it once the frames have flipped (mbFlipped): rows to clear (mauiRowClears, the colour), then cells
//(gauiVideoPending, or all of them).
//
//The font is an SFT container of 256 transparent images of 8 by 16 (UserLib/HCP65SFT.h), which SetTextMode checks:
//gauiVideoGlyphs is where each one's runs start in it, 0 for one without a pixel.  A glyph's runs draw only its pixels,
//so a cell is cleared before its glyph is drawn.
extern "C"
{
	extern uint8	guiVideoFlags;			//What Flags was last written.
	extern uint16	guiVideoWidth;			//The pixels in a line of the mode set (muiWidth), for the assembly: text's and the blits'.
	extern uint32	guiVideoPalette;		//The Palette Byte register's address.
	extern uint8*	gpuiVideoFont;			//VIDEO_FONT_SIZE bytes of the kernal's main memory heap in one bank, NULL if there were none.
	extern uint8*	gpuiVideoClear;			//VIDEO_CLEAR_SIZE zeros there too: the line a Blitter Board clears a row with.  NULL if there were none.
	extern uint16	guiVideoColumns;
	extern uint16	guiVideoRows;
	extern uint8	guiVideoTextColour;
	extern uint16	guiVideoCursor;			//Its physical cell, VIDEO_CURSOR_NONE if none.
	extern uint16	guiVideoPending;
	extern uint8	gbVideoPendingAll;
	extern uint16	gauiVideoRowCells[VIDEO_TEXT_MAX_ROWS + 1];	//Each row's first cell in gauiVideoChars: row x columns.
	extern uint32	gauiVideoRowAddress[VIDEO_TEXT_MAX_ROWS + 1];	//Each row's first pixel's address in the window.
	extern uint8	gauiVideoChars[VIDEO_TEXT_MAX_CELLS];
	extern uint8	gauiVideoCellColours[VIDEO_TEXT_MAX_CELLS];
	extern uint16	gauiVideoPending[VIDEO_TEXT_MAX_PENDING];
	extern uint16	gauiVideoGlyphs[VIDEO_TEXT_GLYPHS];

	extern uint16	guiVideoCell;			//VideoRenderCell's cell; VideoWriteCells' first, then the one after its last.
	extern uint32	guiVideoSource;			//VideoWriteCells' characters' address,
	extern uint16	guiVideoCount;			//how many,
	extern uint16	guiVideoDrew;			//and whether any was drawn.
	extern uint32	guiVideoFillAddress;	//VideoFillZero: the first byte
	extern uint16	guiVideoFillGroups;		//and how many sixteens of them.
	extern uint16	guiVideoFillLines;		//VideoClearLines: how many lines from guiVideoFillAddress.
	extern uint8*	gpuiVideoBlitCode;		//VideoBlitRuns (VideoBoardBlit.asm): the image's first code (the kernal's pointer),
	extern uint16	guiVideoBlitEnd;		//just past its bytes in the same bank,
	extern uint16	guiVideoBlitWidth;		//its size,
	extern uint16	guiVideoBlitHeight;
	extern uint8*	gpuiVideoBlitRow;		//where its top left goes,
	extern uint16	guiVideoBlitResult;		//and 0 if it was all drawn, 1 if a run wasn't in it.
	extern uint8*	gpuiVideoSpanSource;	//VideoBlitSpans (VideoBoardBlit.asm): the first row's pixel at the spans' left,
	extern uint16	guiVideoSpanSourceStride;	//a row's pixels,
	extern uint8*	gpuiVideoSpanDest;		//where the pixel goes,
	extern uint16*	gpuiVideoSpans;			//the first row's span,
	extern uint16	guiVideoSpanRows;		//how many rows,
	extern uint16	guiVideoSpanLeast;		//and what the spans are cut to.
	extern uint16	guiVideoSpanMost;

	void	VideoRenderCell(void);
	void	VideoRenderPending(void);
	void	VideoRenderAll(void);
	void	VideoWriteCells(void);
	void	VideoBlitRuns(void);
	void	VideoBlitSpans(void);
	void	VideoFillZero(void);
	void	VideoClearLines(void);
}


//The HCP65 Video Board's driver: its units are the board's memories (VIDEO_BOARD_PIXELS ...) and its text
//(VIDEO_BOARD_TEXT, VIDEO_BOARD_FONT: VideoBoardText.cpp), and a program has the whole board.  The pixels and their
//palette numbers are the board's main memory window (the frame the processor has); the palettes and the row table its
//device RAM.  A blit draws an SFT image on the pixels (VideoBoardBlit.cpp).  The interrupt (trusted: only the board and the driver's own memory) is
//vertical blank's: it counts them and makes a flip asked for since the last.  Without a window from the kernal (or with
//window 0, whose pixels aren't reachable in kernel mode) the pixels can't be reached: their reads and writes fail.
class CKernalVideoBoard : public CKernalDriver
{
protected:
	uint8			muiMode;		//VIDEO_MODE_*, 0xFF if none.
	uint16			muiWidth;
	uint16			muiHeight;
	volatile bool	mbFlip;			//A flip is due at the next vertical blank.
	volatile uint32	muiVBlanks;
	uint32			muiWindow;		//The main memory window's first address, 0 if it can't be reached.

	//Text (the cells themselves are the globals above).
	bool			mbText;
	uint16			muiTop;
	uint16			muiShownTop;	//The row table's.
	uint8			muiBlink;
	volatile bool	mbFlipped;
	uint32			mauiRowPixels[VIDEO_TEXT_MAX_ROWS + 1];	//Each row's first pixel in a frame: row x 16 x width.
	uint8			mauiRowClears[VIDEO_TEXT_MAX_ROWS];
	uint16			muiFontSize;	//The bytes of the font written.

public:
	uint16	Init(SKernalDevice* psDevice, uint16 uiSlot) override;
	void	Kill(void) override;

	uint16	NumUnits(void) const override;
	void	Interrupt(void) override;

	uint16	Read(uint16 uiUnit, uint32 uiOffset, uint32 uiUserAddress, uint32 uiLength, uint32* puiDone) override;
	uint16	Write(uint16 uiUnit, uint32 uiOffset, uint32 uiUserAddress, uint32 uiLength, uint32* puiDone) override;
	uint16	Control(uint16 uiUnit, uint16 uiCode, uint32 uiValue, uint32* puiResult) override;
	uint16	Wait(uint16 uiUnit, uint32 uiValue, uint16 uiTicks) override;
	uint16	Blit(uint16 uiUnit, const SKernalBlit* psBlit) override;

protected:
	bool	IsWaitOver(uint16 uiUnit, uint32 uiValue) override;

	uint8*	UnitMemory(uint16 uiUnit, uint32 uiOffset, uint32 uiLength);
	void	SetMode(uint8 uiMode);
	void	Off(void);
	void	WriteFlags(uint8 uiFlags);

	bool	SetTextMode(uint8 uiMode);
	void	SetTextPalette(uint8 uiPalette, uint8 uiBackground, uint8 uiForeground);
	void	ClearTextFrame(void);
	bool	ReadFont(void);
	void	WriteTextRows(uint16 uiTop);
	uint16	PhysicalCell(uint32 uiCell);
	uint16	PhysicalRow(uint16 uiRow);
	void	RenderRowClear(uint16 uiRow, uint8 uiColour);
	void	CatchUp(void);
	void	DrawCell(uint16 uiCell);
	void	ClearRow(uint16 uiRow);
	uint16	BlitOpaque(const SKernalBlit* psBlit, uint16 uiWidth, uint16 uiHeight, const int32* paiClip);
	uint16	BlitTransparent(const SKernalBlit* psBlit, uint16 uiWidth, uint16 uiHeight, const int32* paiClip);
	uint16	BlitTransparentWhole(const SKernalBlit* psBlit, const uint8* puiImage, uint16 uiWidth, uint16 uiHeight);
	uint16	BlitOpaqueSpans(const SKernalBlit* psBlit, uint16 uiWidth, uint16 uiHeight, int32 iRight, int32 iY, uint16 uiRow, uint32 uiSource, uint8* puiDest, uint16* puiSpans, int32 iBottom);
	bool	BlitPixels(uint32 uiPixel, uint32 uiUserAddress, uint32 uiLength, uint16 uiTransparent);

	uint16	WriteText(uint32 uiCell, uint32 uiUserAddress, uint32 uiLength, uint32* puiDone);
	uint16	ScrollText(uint32 uiLines);
	uint16	ClearText(void);
	void	SetCursorCell(uint16 uiCell);
	void	TextBlank(bool bFlipped);
};


#define VIDEO_BOARD_MAX_BOARDS			1		//Video boards it drives.


extern "C" CKernalDriver*	KernalCreateDriver(uint16 uiIndex);


#endif // __VIDEO_BOARD_H__
