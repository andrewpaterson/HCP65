#include <string.h>
#include "KernalProcess.h"
#include "UserLib/HCP65SFT.h"
#include "VideoBoard.h"


//What VideoBoardRender.asm's routines draw.
extern "C"
{
	uint32	guiVideoCellPixel;		//VideoDrawRuns and VideoClearCell: the cell's first pixel,
	uint32	guiVideoCellGlyph;		//its glyph's first run
	uint16	guiVideoWidth;			//and the pixels in a line.
	uint32	guiVideoFillAddress;	//VideoFillZero: the first byte
	uint16	guiVideoFillGroups;		//and how many sixteens of them.

	void	VideoDrawRuns(void);
	void	VideoClearCell(void);
	void	VideoFillZero(void);
}


#define VIDEO_CELL(row, column)		((uint16)(((row) << 8) | (column)))
#define VIDEO_CELL_ROW(cell)		((uint16)((cell) >> 8))
#define VIDEO_CELL_COLUMN(cell)		((uint16)((cell) & 0xFF))


//A cell's bit in its row's byte of mauiDirty.
static const uint8	gauiVideoCellBits[8] = { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };


//A VGA's sixteen colours, as red, green and blue.
static const uint8	gauiVideoColours[16][3] =
{
	{ 0x00, 0x00, 0x00 }, { 0x00, 0x00, 0xAA }, { 0x00, 0xAA, 0x00 }, { 0x00, 0xAA, 0xAA },
	{ 0xAA, 0x00, 0x00 }, { 0xAA, 0x00, 0xAA }, { 0xAA, 0x55, 0x00 }, { 0xAA, 0xAA, 0xAA },
	{ 0x55, 0x55, 0x55 }, { 0x55, 0x55, 0xFF }, { 0x55, 0xFF, 0x55 }, { 0x55, 0xFF, 0xFF },
	{ 0xFF, 0x55, 0x55 }, { 0xFF, 0x55, 0xFF }, { 0xFF, 0xFF, 0x55 }, { 0xFF, 0xFF, 0xFF },
};


//////////////////////////////////////////////////////////////////////////
//
// Palette uiPalette's colours: entry 0 (a cell's background) the VGA
// colour uiBackground and VIDEO_TEXT_PIXEL (its foreground) uiForeground.
//
//////////////////////////////////////////////////////////////////////////
void CKernalVideoBoard::SetTextPalette(uint8 uiPalette, uint8 uiBackground, uint8 uiForeground)
{
	volatile uint8*	puiRed;
	uint16			i;

	puiRed = (volatile uint8*)(mpsDevice->uiAddress + VIDEO_PALETTES + (uint32)uiPalette * 256);
	for (i = 0; i < 3; i++)
	{
		puiRed[(uint32)i * VIDEO_BOARD_GREEN] = gauiVideoColours[uiBackground][i];
		puiRed[(uint32)i * VIDEO_BOARD_GREEN + VIDEO_TEXT_PIXEL] = gauiVideoColours[uiForeground][i];
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Text in mode uiMode: Width / 8 by Height / 16 cells, all blank in
// light grey on black, and no cursor, drawn with the font the driver has
// been given.  Both frames are cleared, waiting for the vertical blank
// between them.  False without a main memory window (the pixels can't be
// reached) or a font that is what it should be.
//
//////////////////////////////////////////////////////////////////////////
bool CKernalVideoBoard::SetTextMode(uint8 uiMode)
{
	uint16	uiPalette;
	uint16	uiRow;
	uint16	uiCell;
	uint32	uiPixel;
	uint32	uiVBlanks;

	if (!muiWindow || !ReadFont())
	{
		return false;
	}
	mbText = false;
	SetMode(uiMode);
	muiColumns = (uint16)(muiWidth / VIDEO_TEXT_CELL_WIDTH);
	muiRows = (uint16)(muiHeight / VIDEO_TEXT_CELL_HEIGHT);
	muiTop = 0;
	muiShownTop = 0;
	muiTextColour = VIDEO_TEXT_DEFAULT_COLOUR;
	muiCursor = VIDEO_CURSOR_NONE;
	muiBlink = 0;
	muiPending = 0;
	mbPendingAll = false;
	mbFlipped = false;
	memset(mauiChars, ' ', sizeof(mauiChars));
	memset(mauiColours, VIDEO_TEXT_DEFAULT_COLOUR, sizeof(mauiColours));
	memset(mauiRowClears, VIDEO_TEXT_NO_CLEAR, sizeof(mauiRowClears));

	//Where each row's cells and pixels start, so that nothing drawn need multiply or divide.
	uiCell = 0;
	uiPixel = 0;
	for (uiRow = 0; uiRow <= muiRows; uiRow++)
	{
		mauiRowCells[uiRow] = uiCell;
		mauiRowPixels[uiRow] = uiPixel;
		uiCell = (uint16)(uiCell + muiColumns);
		uiPixel += (uint32)muiWidth * VIDEO_TEXT_CELL_HEIGHT;
	}
	guiVideoWidth = muiWidth;

	//Each palette p is foreground p & 15 on background (p >> 4) & 7, but the cursor's (VIDEO_TEXT_CURSOR_PALETTE), the
	//default colour the other way round (which the interrupt swaps to blink it).
	for (uiPalette = 0; uiPalette < VIDEO_TEXT_CURSOR_PALETTE; uiPalette++)
	{
		SetTextPalette((uint8)uiPalette, (uint8)((uiPalette >> 4) & 7), (uint8)(uiPalette & 15));
	}
	SetTextPalette(VIDEO_TEXT_CURSOR_PALETTE, VIDEO_TEXT_DEFAULT_COLOUR & 15, (VIDEO_TEXT_DEFAULT_COLOUR >> 4) & 7);

	//Both frames blank (pixels 0 in the default colour's palette): the one drawn, then the other once it is.
	mpuiRegisters[VIDEO_PALETTE_BYTE] = VIDEO_TEXT_DEFAULT_COLOUR;
	WriteFlags((uint8)(muiFlags | VIDEO_FLAG_PALETTE_WRITE));
	ClearTextFrame();
	WriteTextRows(0);
	uiVBlanks = muiVBlanks;
	mbFlip = true;
	WaitFor(VIDEO_BOARD_TEXT, uiVBlanks, VIDEO_TEXT_FLIP_TICKS);
	ClearTextFrame();
	mbFlipped = false;
	mbText = true;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The frame being drawn's text area, and the lines below it, blank in
// the default colour.
//
//////////////////////////////////////////////////////////////////////////
void CKernalVideoBoard::ClearTextFrame(void)
{
	uint32	uiSize;
	uint32	uiDone;
	uint32	uiChunk;
	uint16	uiFrame;

	uiFrame = (muiFlags & VIDEO_FLAG_MEMORY_FRAME) ? 1 : 0;
	memset(&mauiDirty[uiFrame ? VIDEO_TEXT_FRAME_BYTES : 0], 0, VIDEO_TEXT_FRAME_BYTES);
	memset(&mauiRowGround[uiFrame ? VIDEO_TEXT_MAX_ROWS : 0], VIDEO_TEXT_DEFAULT_COLOUR >> 4, VIDEO_TEXT_MAX_ROWS);

	uiSize = mauiRowPixels[muiRows] + (uint32)muiWidth * VIDEO_TEXT_CELL_HEIGHT;
	for (uiDone = 0; uiDone < uiSize; uiDone += uiChunk)
	{
		uiChunk = ((uiSize - uiDone) < 0x8000) ? (uiSize - uiDone) : 0x8000;
		memset((void*)(muiWindow + uiDone), 0, uiChunk);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The font that was written is an SFT container whose first 256 images
// are transparent and 8 by 16, with every run inside its image: mauiGlyphs
// is where each one's runs start.  VideoDrawRuns draws them without
// checking again.
//
//////////////////////////////////////////////////////////////////////////
bool CKernalVideoBoard::ReadFont(void)
{
	const SSFTContainer*	psContainer;
	const SSFTImage*		psImage;
	uint16					uiGlyph;
	uint16					uiPosition;
	uint16					uiNext;
	uint16					uiCode;
	uint16					uiFirst;
	uint16					x;
	uint16					y;
	uint8					c;

	psContainer = (const SSFTContainer*)mauiFont;
	if ((muiFontSize < sizeof(SSFTContainer)) || (psContainer->uiType != SFT_TYPE_CONTAINER) || (psContainer->uiCount < VIDEO_TEXT_GLYPHS))
	{
		return false;
	}

	uiPosition = sizeof(SSFTContainer);
	for (uiGlyph = 0; uiGlyph < VIDEO_TEXT_GLYPHS; uiGlyph++)
	{
		if ((uint16)(muiFontSize - uiPosition) < sizeof(uint16) + sizeof(SSFTImage) + 1)
		{
			return false;
		}
		uiNext = *((const uint16*)&mauiFont[uiPosition]);
		psImage = (const SSFTImage*)&mauiFont[uiPosition + sizeof(uint16)];
		if ((psImage->uiType != SFT_TYPE_TRANSPARENT) || (psImage->uiWidth != VIDEO_TEXT_CELL_WIDTH) || (psImage->uiHeight != VIDEO_TEXT_CELL_HEIGHT))
		{
			return false;
		}

		uiFirst = (uint16)(uiPosition + sizeof(uint16) + sizeof(SSFTImage));
		uiCode = uiFirst;
		x = 0;
		y = 0;
		for (;;)
		{
			if (uiCode >= muiFontSize)
			{
				return false;
			}
			c = mauiFont[uiCode++];
			if (c == SFT_RUN_END)
			{
				break;
			}
			if (c >= SFT_RUN_ROWS)
			{
				y = (uint16)(y + (c & SFT_RUN_COUNT_MASK) + 1);
				x = 0;
			}
			else if (c >= SFT_RUN_SKIP)
			{
				x = (uint16)(x + (c & SFT_RUN_COUNT_MASK) + 1);
			}
			else
			{
				if ((y >= VIDEO_TEXT_CELL_HEIGHT) || (x + c > VIDEO_TEXT_CELL_WIDTH) || ((uint16)(muiFontSize - uiCode) < c))
				{
					return false;
				}
				uiCode = (uint16)(uiCode + c);
				x = (uint16)(x + c);
			}
		}
		mauiGlyphs[uiGlyph] = (mauiFont[uiFirst] == SFT_RUN_END) ? 0 : uiFirst;

		if (uiGlyph < VIDEO_TEXT_GLYPHS - 1)
		{
			if ((uiNext == 0) || (uiNext > (uint16)(muiFontSize - uiPosition)))
			{
				return false;
			}
			uiPosition = (uint16)(uiPosition + uiNext);
		}
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The row table for logical row 0 being the physical row uiTop: each
// line of a logical row the matching line of the row of cells it is.  The
// lines below the last row (800x600's last 8) show the blank row after
// them.
//
//////////////////////////////////////////////////////////////////////////
void CKernalVideoBoard::WriteTextRows(uint16 uiTop)
{
	volatile uint8*	puiRow;
	uint32			uiAddress;
	uint16			uiPhysical;
	uint16			uiRow;
	uint16			uiLine;
	uint16			y;

	puiRow = (volatile uint8*)(mpsDevice->uiAddress + VIDEO_ROW_TABLE);
	uiPhysical = uiTop;
	uiRow = 0;
	uiLine = 0;
	uiAddress = mauiRowPixels[uiPhysical];
	for (y = 0; y < muiHeight; y++)
	{
		puiRow[0] = (uint8)uiAddress;
		puiRow[1] = (uint8)(uiAddress >> 8);
		puiRow[2] = (uint8)(uiAddress >> 16);
		puiRow += 4;

		uiAddress += muiWidth;
		uiLine++;
		if (uiLine == VIDEO_TEXT_CELL_HEIGHT)
		{
			uiLine = 0;
			uiRow++;
			uiPhysical++;
			if (uiPhysical == muiRows)
			{
				uiPhysical = 0;
			}
			uiAddress = mauiRowPixels[(uiRow < muiRows) ? uiPhysical : muiRows];
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The physical row logical row uiRow is: moved down by the scroll.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalVideoBoard::PhysicalRow(uint16 uiRow)
{
	uiRow = (uint16)(uiRow + muiTop);
	if (uiRow >= muiRows)
	{
		uiRow = (uint16)(uiRow - muiRows);
	}
	return uiRow;
}


//////////////////////////////////////////////////////////////////////////
//
// The cell (its physical row and column) the logical cell at row x
// columns + column is.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalVideoBoard::PhysicalCell(uint32 uiCell)
{
	uint16	uiRow;

	uiRow = (uint16)(uiCell / muiColumns);
	return VIDEO_CELL(PhysicalRow(uiRow), (uint16)(uiCell - (uint32)uiRow * muiColumns));
}


//////////////////////////////////////////////////////////////////////////
//
// The cell drawn in the frame being drawn: its character's glyph in its
// colour's palette (the cursor's if it is the cursor's cell).  The glyph's
// runs are only its pixels, so a cell that has pixels in this frame, or
// whose background isn't what its row was cleared to, is cleared first.
//
//////////////////////////////////////////////////////////////////////////
void CKernalVideoBoard::RenderCell(uint16 uiCell)
{
	uint16	uiRow;
	uint16	uiColumn;
	uint16	uiIndex;
	uint16	uiDirty;
	uint16	uiGlyph;
	uint8	uiColour;
	uint8	uiBit;
	uint8	uiGround;

	uiRow = VIDEO_CELL_ROW(uiCell);
	uiColumn = VIDEO_CELL_COLUMN(uiCell);
	uiIndex = (uint16)(mauiRowCells[uiRow] + uiColumn);
	uiGlyph = mauiGlyphs[mauiChars[uiIndex]];
	uiBit = gauiVideoCellBits[uiColumn & 7];
	if (muiFlags & VIDEO_FLAG_MEMORY_FRAME)
	{
		uiDirty = (uint16)(VIDEO_TEXT_FRAME_BYTES + (uiRow << 4) + (uiColumn >> 3));
		uiGround = mauiRowGround[VIDEO_TEXT_MAX_ROWS + uiRow];
	}
	else
	{
		uiDirty = (uint16)((uiRow << 4) + (uiColumn >> 3));
		uiGround = mauiRowGround[uiRow];
	}
	guiVideoCellPixel = muiWindow + mauiRowPixels[uiRow] + (uint32)(uiColumn << 3);

	if (uiCell == muiCursor)
	{
		//The cursor's cell is all drawn, and isn't in its row's background after.
		mpuiRegisters[VIDEO_PALETTE_BYTE] = VIDEO_TEXT_CURSOR_PALETTE;
		VideoClearCell();
		mauiDirty[uiDirty] |= uiBit;
	}
	else
	{
		uiColour = mauiColours[uiIndex];
		mpuiRegisters[VIDEO_PALETTE_BYTE] = uiColour;
		if ((uint8)(uiColour >> 4) != uiGround)
		{
			VideoClearCell();
			mauiDirty[uiDirty] |= uiBit;
		}
		else if (mauiDirty[uiDirty] & uiBit)
		{
			VideoClearCell();
			if (uiGlyph == 0)
			{
				mauiDirty[uiDirty] &= (uint8)~uiBit;
				return;
			}
		}
		else if (uiGlyph == 0)
		{
			return;
		}
		else
		{
			mauiDirty[uiDirty] |= uiBit;
		}
	}
	if (uiGlyph != 0)
	{
		guiVideoCellGlyph = (uint32)&mauiFont[uiGlyph];
		VideoDrawRuns();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Physical row uiRow's pixels all background in the frame being drawn, in
// uiColour's palette.
//
//////////////////////////////////////////////////////////////////////////
void CKernalVideoBoard::RenderRowClear(uint16 uiRow, uint8 uiColour)
{
	uint16	uiFrame;

	uiFrame = (muiFlags & VIDEO_FLAG_MEMORY_FRAME) ? 1 : 0;
	memset(&mauiDirty[(uiFrame ? VIDEO_TEXT_FRAME_BYTES : 0) + (uiRow << 4)], 0, VIDEO_TEXT_ROW_BYTES);
	mauiRowGround[(uiFrame ? VIDEO_TEXT_MAX_ROWS : 0) + uiRow] = (uint8)(uiColour >> 4);
	mpuiRegisters[VIDEO_PALETTE_BYTE] = uiColour;
	guiVideoFillAddress = muiWindow + mauiRowPixels[uiRow];
	guiVideoFillGroups = muiWidth;  //16 lines of the width, in sixteens.
	VideoFillZero();
}


//////////////////////////////////////////////////////////////////////////
//
// Once the frames have flipped, what the frame now being drawn is behind
// in (it was shown while the other was drawn): the rows cleared, then the
// cells.
//
//////////////////////////////////////////////////////////////////////////
void CKernalVideoBoard::CatchUp(void)
{
	uint16	uiRow;
	uint16	uiColumn;
	uint16	i;

	if (!mbFlipped)
	{
		return;
	}
	for (uiRow = 0; uiRow < muiRows; uiRow++)
	{
		if (mauiRowClears[uiRow] != VIDEO_TEXT_NO_CLEAR)
		{
			RenderRowClear(uiRow, mauiRowClears[uiRow]);
			mauiRowClears[uiRow] = VIDEO_TEXT_NO_CLEAR;
		}
	}
	if (mbPendingAll)
	{
		for (uiRow = 0; uiRow < muiRows; uiRow++)
		{
			for (uiColumn = 0; uiColumn < muiColumns; uiColumn++)
			{
				RenderCell(VIDEO_CELL(uiRow, uiColumn));
			}
		}
	}
	else
	{
		for (i = 0; i < muiPending; i++)
		{
			RenderCell(mauiPending[i]);
		}
	}
	muiPending = 0;
	mbPendingAll = false;
	mbFlipped = false;
}


//////////////////////////////////////////////////////////////////////////
//
// The cell, as mauiChars and mauiColours have it, drawn in the frame being
// drawn and owed to the other, which is shown at the next vertical blank.
//
//////////////////////////////////////////////////////////////////////////
void CKernalVideoBoard::DrawCell(uint16 uiCell)
{
	CatchUp();
	RenderCell(uiCell);
	if (!mbPendingAll)
	{
		if (muiPending < VIDEO_TEXT_MAX_PENDING)
		{
			mauiPending[muiPending++] = uiCell;
		}
		else
		{
			mbPendingAll = true;
		}
	}
	mbFlip = true;
}


//////////////////////////////////////////////////////////////////////////
//
// Physical row uiRow's cells all blank in the text colour: cleared in the
// frame being drawn, and owed to the other.
//
//////////////////////////////////////////////////////////////////////////
void CKernalVideoBoard::ClearRow(uint16 uiRow)
{
	CatchUp();
	memset(&mauiChars[mauiRowCells[uiRow]], ' ', muiColumns);
	memset(&mauiColours[mauiRowCells[uiRow]], muiTextColour, muiColumns);
	RenderRowClear(uiRow, muiTextColour);
	mauiRowClears[uiRow] = muiTextColour;
	mbFlip = true;
}


//////////////////////////////////////////////////////////////////////////
//
// The running process's uiLength characters at uiUserAddress into the
// text from logical cell uiCell, in the text colour.  The cursor's cell
// written over is no longer the cursor's.  Only cells that change are
// drawn.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalVideoBoard::WriteText(uint32 uiCell, uint32 uiUserAddress, uint32 uiLength, uint32* puiDone)
{
	uint8	auiChars[VIDEO_TEXT_CHUNK];
	uint32	uiChunk;
	uint16	uiPhysical;
	uint16	uiIndex;
	uint16	i;

	if (!mbText || (uiCell >= (uint32)mauiRowCells[muiRows]) || (uiLength > (uint32)mauiRowCells[muiRows] - uiCell))
	{
		return KERNAL_DRIVER_BAD_ARGUMENT;
	}
	uiPhysical = PhysicalCell(uiCell);
	while (uiLength > 0)
	{
		uiChunk = (uiLength < VIDEO_TEXT_CHUNK) ? uiLength : VIDEO_TEXT_CHUNK;
		if (!KernalCopyFromUser(auiChars, uiUserAddress, uiChunk))
		{
			return KERNAL_DRIVER_BAD_ARGUMENT;
		}
		for (i = 0; i < uiChunk; i++)
		{
			//A cell that is already this (a space where the line is blank) isn't drawn again.
			uiIndex = (uint16)(mauiRowCells[VIDEO_CELL_ROW(uiPhysical)] + VIDEO_CELL_COLUMN(uiPhysical));
			if ((mauiChars[uiIndex] != auiChars[i]) || (mauiColours[uiIndex] != muiTextColour) || (uiPhysical == muiCursor))
			{
				mauiChars[uiIndex] = auiChars[i];
				mauiColours[uiIndex] = muiTextColour;
				if (uiPhysical == muiCursor)
				{
					muiCursor = VIDEO_CURSOR_NONE;
				}
				DrawCell(uiPhysical);
			}

			//The next cell: along the row, then the start of the next row down (the first after the last).
			uiPhysical++;
			if (VIDEO_CELL_COLUMN(uiPhysical) == muiColumns)
			{
				uiPhysical = (uint16)(VIDEO_CELL_ROW(uiPhysical) + 1);
				uiPhysical = VIDEO_CELL((uiPhysical == muiRows) ? 0 : uiPhysical, 0);
			}
		}
		*puiDone += uiChunk;
		uiUserAddress += uiChunk;
		uiLength -= uiChunk;
	}
	return KERNAL_DRIVER_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////
//
// The text moved up uiLines rows: the rows that come in at the bottom are
// blank in the text colour, and the cursor is hidden.  The row table
// changes at the vertical blank that shows them.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalVideoBoard::ScrollText(uint32 uiLines)
{
	uint16	uiLine;

	if (!mbText || (uiLines == 0) || (uiLines > muiRows))
	{
		return KERNAL_DRIVER_BAD_ARGUMENT;
	}
	SetCursorCell(VIDEO_CURSOR_NONE);
	for (uiLine = 0; uiLine < uiLines; uiLine++)
	{
		muiTop++;
		if (muiTop == muiRows)
		{
			muiTop = 0;
		}
		ClearRow(PhysicalRow((uint16)(muiRows - 1)));
	}
	return KERNAL_DRIVER_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////
//
// Every cell blank in the text colour, and no cursor.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalVideoBoard::ClearText(void)
{
	uint16	uiRow;

	if (!mbText)
	{
		return KERNAL_DRIVER_BAD_ARGUMENT;
	}
	muiCursor = VIDEO_CURSOR_NONE;
	for (uiRow = 0; uiRow < muiRows; uiRow++)
	{
		ClearRow(uiRow);
	}
	return KERNAL_DRIVER_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////
//
// The cursor on the cell uiCell (VIDEO_CURSOR_NONE: hidden): the cell it
// leaves is drawn in its own colour again.
//
//////////////////////////////////////////////////////////////////////////
void CKernalVideoBoard::SetCursorCell(uint16 uiCell)
{
	uint16	uiOld;

	uiOld = muiCursor;
	if (uiOld == uiCell)
	{
		return;
	}
	muiCursor = uiCell;
	if (uiOld != VIDEO_CURSOR_NONE)
	{
		DrawCell(uiOld);
	}
	if (uiCell != VIDEO_CURSOR_NONE)
	{
		DrawCell(uiCell);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The vertical blank's part in text: the row table for the scroll the
// frame now shown was drawn for, and the cursor's blink.  Only device RAM:
// the interrupt is trusted.
//
//////////////////////////////////////////////////////////////////////////
void CKernalVideoBoard::TextBlank(bool bFlipped)
{
	volatile uint8*	puiCursor;
	uint8			uiSwap;
	uint16			i;

	if (!mbText)
	{
		return;
	}
	if (bFlipped)
	{
		mbFlipped = true;
		if (muiShownTop != muiTop)
		{
			WriteTextRows(muiTop);
			muiShownTop = muiTop;
		}
	}
	muiBlink++;
	if (muiBlink == VIDEO_TEXT_BLINK_FRAMES)
	{
		muiBlink = 0;
		puiCursor = (volatile uint8*)(mpsDevice->uiAddress + VIDEO_PALETTES + (uint32)VIDEO_TEXT_CURSOR_PALETTE * 256);
		for (i = 0; i < 3; i++)
		{
			uiSwap = puiCursor[(uint32)i * VIDEO_BOARD_GREEN];
			puiCursor[(uint32)i * VIDEO_BOARD_GREEN] = puiCursor[(uint32)i * VIDEO_BOARD_GREEN + VIDEO_TEXT_PIXEL];
			puiCursor[(uint32)i * VIDEO_BOARD_GREEN + VIDEO_TEXT_PIXEL] = uiSwap;
		}
	}
}
