#ifndef __HCP65_VIDEO_H__
#define __HCP65_VIDEO_H__
#include "BaseLib/Mutex.h"
#include "HCP65Device.h"


#define HCP65_VIDEO_BOARD_NAME			"Video Board"
#define HCP65_VIDEO_DRIVER				"VIDEO.DRV"
#define HCP65_VIDEO_REGISTERS			0x400
#define HCP65_VIDEO_DEVICE_RAM			0x800		//$00800 to $1FFFF: ~Device RAM.
#define HCP65_VIDEO_DECODE				0x1F		//A0-A4; A5-A9 aren't decoded.

//Registers, from $400.  They repeat every 32 bytes; $414, $415 and $418-$41F decode nothing.
#define HCP65_VIDEO_FLAGS				0x00	//R/W: Flags & Interrupt.
#define HCP65_VIDEO_CLEAR_INTERRUPT		0x01	//W.
#define HCP65_VIDEO_ADDRESS_LOW			0x02	//R: the pixel address, bits 0-7.  Reading it takes the address for High and Bank.
#define HCP65_VIDEO_ADDRESS_HIGH		0x03	//R: bits 8-15.
#define HCP65_VIDEO_ADDRESS_BANK		0x04	//R: bits 16-20.
#define HCP65_VIDEO_CLOCK_SELECT		0x05	//W.
#define HCP65_VIDEO_H_FRONT_PORCH		0x06	//W: Horizontal Front Porch Start, in 8 pixels.  R: Current Horizontal, bits 0-7.
#define HCP65_VIDEO_H_SYNC				0x07	//W: Horizontal Sync Start.  R: Current Horizontal, bits 8-11.
#define HCP65_VIDEO_H_BACK_PORCH		0x08	//W: Horizontal Back Porch Start.
#define HCP65_VIDEO_H_END				0x09	//W: Horizontal Back Porch End: the line is 8 times this.
#define HCP65_VIDEO_PATTERN_LOW			0x0A	//W: D0-D3, the pattern's first 4 steps.
#define HCP65_VIDEO_PATTERN_HIGH		0x0B	//W: D0-D7, its other 8.
#define HCP65_VIDEO_V_FRONT_PORCH		0x0C	//W: Vertical Front Porch Start low, then high ($0D, D0-D1).  R: Current Vertical low, then high.
#define HCP65_VIDEO_V_SYNC				0x0E	//W: Vertical Sync Start low, then high.
#define HCP65_VIDEO_V_BACK_PORCH		0x10	//W: Vertical Back Porch Start low, then high.
#define HCP65_VIDEO_V_END				0x12	//W: Vertical Back Porch End low, then high: the frame is this many lines.
#define HCP65_VIDEO_PALETTE_BYTE		0x16	//W.
#define HCP65_VIDEO_SIGNAL_DELAY		0x17	//W.

#define HCP65_VIDEO_FLAG_VSYNC_NEGATIVE	0x01
#define HCP65_VIDEO_FLAG_HSYNC_NEGATIVE	0x02
#define HCP65_VIDEO_FLAG_MEMORY_FRAME	0x04	//0: the processor has frame 0 and frame 1 is shown; 1 the other way round.
#define HCP65_VIDEO_FLAG_PALETTE_WRITE	0x08	//A write to window A also writes the Palette Byte into window B.
#define HCP65_VIDEO_FLAG_ENABLE			0x40
#define HCP65_VIDEO_FLAG_INTERRUPT		0x80	//W: interrupt enable.  R: interrupting.

#define HCP65_VIDEO_CLOCK_RATE			0x07	//Clock Select: HCP65_VIDEO_CLOCK_*.
#define HCP65_VIDEO_CLOCK_ENABLE		0x08
#define HCP65_VIDEO_DELAY				0x03	//Signal Delay: 3 to 4½ clocks.
#define HCP65_VIDEO_DELAY_OFF			0x80	//Signal Delay: 1 takes sync and blank off the DAC.

#define HCP65_VIDEO_NUM_CLOCKS			8
#define HCP65_VIDEO_FRAME_SIZE			0x200000	//Each of the four chips: a frame's indices, or its palette numbers.
#define HCP65_VIDEO_ADDRESS_MASK		0x1FFFFF
#define HCP65_VIDEO_ROW_TABLE			0x04000		//Device RAM: 4 bytes a row (Low, High, Bank, unused).
#define HCP65_VIDEO_ROW_TABLE_SIZE		0x4000
#define HCP65_VIDEO_RED					0x08000		//Device RAM: palette p's index i at 256p + i.
#define HCP65_VIDEO_GREEN				0x10000
#define HCP65_VIDEO_BLUE				0x18000
#define HCP65_VIDEO_PALETTE_SIZE		0x8000
#define HCP65_VIDEO_PALETTE_NUMBER		0x7F		//Of a pixel's palette number byte.
#define HCP65_VIDEO_MAX_ROWS			1024		//The vertical counter's 10 bits.
#define HCP65_VIDEO_PATTERN_STEPS		12
#define HCP65_VIDEO_MAX_WIDTH			(8 * 255)	//The widest active part a line can have.


//The HCP65 Video Board (Video Board - Signals, Video Board - Memory and their boards), as it is meant to work: see their
//Faults files.  A 128KB device: the device ROM below $400, the registers from $400, and device RAM from $800 with the
//row table at $04000 and the Red, Green and Blue palettes at $08000, $10000 and $18000.  Its main memory windows are its
//own memory, two of its four 2MB chips: window A a frame's pixel indices, window B the same frame's palette numbers.
//Memory Frame says which frame the processor has; the other frame is shown.  While Palette Write is set a write to
//window A also writes the Palette Byte at the same offset in window B.
//
//Its time is in pixel clocks (Clock Select's), counted while the clock is enabled.  A line is 8 x Back Porch End pixel
//clocks: the active part, 8 x Front Porch Start, then the front porch, sync (from 8 x Sync Start) and back porch.  A
//frame is Back Porch End lines, the first Front Porch Start of them shown; vertical sync is from Sync Start to Back Porch
//Start.  Each shown row r starts at row table entry r's address, and on each pixel clock of the active part the pixel
//at the address is shown, then the address moves on by one if that step of the 12 step pattern is 1 (the pattern starts
//again at each line).  A pixel's colour is the palettes' entry for its palette number (D0-D6) and index.  The board
//works a line at a time: a row is drawn when its line ends, from what the memory and registers hold then.
//
//The interrupt is set at the start of vertical blank (the line after the last one shown) and cleared by Clear
//Interrupt, or by Enable or the interrupt enable going off.  It is only set while both are on.
//
//Reset stops the pixel clock and takes sync and blank off the DAC, so nothing is shown until the driver has set the
//board up; the other registers are kept.  Held in reset, nothing moves.
//
//The frames drawn are for the host: GetFrame copies the last whole one, under a lock, as the host may take them on
//another thread.  There is no picture while the clock is stopped, the output is off, the timing can't make a frame or
//the board is held in reset.  The signal delay and the sync polarities change nothing in the picture.
class CHCP65VideoBoard : public CHCP65Device
{
protected:
	CDeviceROM	mcROM;
	uint8*		mapuiChips[4];			//Frame 0's indices and palette numbers, then frame 1's.
	uint8*		mpuiRowTable;
	uint8*		mpuiRed;
	uint8*		mpuiGreen;
	uint8*		mpuiBlue;

	uint8		muiFlags;				//D0-D5 and the interrupt enable as written.
	bool		mbEnable;
	bool		mbPending;				//The interrupt has been set since it was last cleared.
	uint8		muiClockSelect;
	uint8		mauiHorizontal[4];		//Front Porch Start, Sync Start, Back Porch Start, Back Porch End.
	uint16		mauiVertical[4];
	uint16		muiPattern;				//Step n is bit n.
	uint8		muiPaletteByte;
	uint8		muiSignalDelay;
	uint32		muiAddressSnapshot;

	uint64		muiCycle;				//Caught up to.
	uint64		muiTick;				//Pixel clocks counted.
	uint64		muiRateCycle;			//The cycle and tick the clock last started or changed at.
	uint64		muiRateTick;
	uint16		muiLine;				//The line of the frame, from 0.
	uint64		muiLineStart;			//Its first tick.

	uint32*		mpuiDrawing;			//The frame being drawn, HCP65_VIDEO_MAX_WIDTH x HCP65_VIDEO_MAX_ROWS.
	uint32*		mpuiShown;				//The last whole frame, under mcFrameMutex with the three below.
	uint32		muiShownWidth;
	uint32		muiShownHeight;
	uint64		muiFrames;
	CMutex		mcFrameMutex;

public:
	void		Init(void);
	void		Kill(void) override;

	uint8		Read(uint32 uiOffset) override;
	void		Write(uint32 uiOffset, uint8 uiValue) override;
	void		Reset(void) override;
	void		SetHeldInReset(bool bHeld) override;
	void		CatchUp(uint64 uiCycle) override;
	uint64		NextEvent(void) override;
	bool		IsInterrupting(void) override;
	uint8*		GetMemoryChip(int iChip) override;
	bool		IsMemoryChipWatched(int iChip) override;
	void		WriteMemoryChip(int iChip, uint32 uiOffset, uint8 uiValue) override;

	bool		GetFrame(uint32* puiDest, uint32 uiMaxWidth, uint32 uiMaxHeight, uint32* puiWidth, uint32* puiHeight);
	uint64		NumFrames(void);
	bool		IsSignal(void);
	uint64		GetPixelClockRate(void);
	uint32		GetLineLength(void);
	uint32		GetActiveWidth(void);
	uint32		GetFrameLines(void);
	uint32		GetActiveLines(void);
	uint16		GetLine(void);
	uint32		GetHorizontal(void);
	uint32		GetPixelAddress(void);
	uint8*		GetChip(int iFrame, int iKind);
	uint8*		GetDeviceRAM(uint32 uiOffset);

	static uint64	GetClockRate(int iSelect);

protected:
	uint8		ReadRegister(uint32 uiRegister);
	void		WriteRegister(uint32 uiRegister, uint8 uiValue);
	void		WriteFlags(uint8 uiValue);
	void		SetClock(uint8 uiClockSelect);
	bool		IsClockRunning(void);
	uint64		TickAt(uint64 uiCycle);
	void		Rebase(void);
	bool		IsTiming(void);
	void		EndLine(void);
	void		EndFrame(void);
	void		DrawRow(uint16 uiRow, uint32 uiWidth);
	uint32		RowStart(uint16 uiRow);
	uint32		Advances(uint32 uiTicks);
	int			ShownFrame(void);
	int			ProcessorFrame(void);
};


#endif // __HCP65_VIDEO_H__

