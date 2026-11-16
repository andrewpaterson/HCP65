#include <string.h>
#include "KernalProcess.h"
#include "VideoBoard.h"


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
// Palette uiPalette's colours: index 0 (a cell's background) the VGA
// colour uiBackground and index 1 (its foreground) uiForeground.
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
		puiRed[(uint32)i * VIDEO_BOARD_GREEN + 1] = gauiVideoColours[uiForeground][i];
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Text in mode uiMode: Width / 8 by Height / 16 cells, all blank in
// light grey on black, and no cursor.  Both frames are cleared, waiting
// for the vertical blank between them.  False without a main memory
// window (the pixels can't be reached).
//
//////////////////////////////////////////////////////////////////////////
bool CKernalVideoBoard::SetTextMode(uint8 uiMode)
{
	uint16	uiPalette;
	uint16	uiByte;
	uint16	uiBit;
	uint32	uiVBlanks;

	if (!muiWindow)
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

	//A glyph row's byte as its eight pixels: 0 (the background) or 1.
	for (uiByte = 0; uiByte < 256; uiByte++)
	{
		for (uiBit = 0; uiBit < 8; uiBit++)
		{
			mauiExpand[uiByte * 8 + uiBit] = (uiByte & (0x80 >> uiBit)) ? 1 : 0;
		}
	}

	//Each palette p is foreground p & 15 on background (p >> 4) & 7, but the cursor's (VIDEO_TEXT_CURSOR_PALETTE), the
	//default colour the other way round (which the interrupt swaps to blink it).
	for (uiPalette = 0; uiPalette < VIDEO_TEXT_CURSOR_PALETTE; uiPalette++)
	{
		SetTextPalette((uint8)uiPalette, (uint8)((uiPalette >> 4) & 7), (uint8)(uiPalette & 15));
	}
	SetTextPalette(VIDEO_TEXT_CURSOR_PALETTE, VIDEO_TEXT_DEFAULT_COLOUR & 15, (VIDEO_TEXT_DEFAULT_COLOUR >> 4) & 7);

	//Both frames blank (pixels 0 in the default colour's palette), the one drawn, then the other once it is.
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
// The frame being drawn's text area, and the lines below it, blank.
//
//////////////////////////////////////////////////////////////////////////
void CKernalVideoBoard::ClearTextFrame(void)
{
	uint32	uiSize;
	uint32	uiDone;
	uint32	uiChunk;

	uiSize = (uint32)muiWidth * (uint32)((muiRows + 1) * VIDEO_TEXT_CELL_HEIGHT);
	for (uiDone = 0; uiDone < uiSize; uiDone += uiChunk)
	{
		uiChunk = ((uiSize - uiDone) < 0x8000) ? (uiSize - uiDone) : 0x8000;
		memset((void*)(muiWindow + uiDone), 0, uiChunk);
	}
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
	uint32			uiRowSize;
	uint16			uiPhysical;
	uint16			uiRow;
	uint16			uiLine;
	uint16			y;

	puiRow = (volatile uint8*)(mpsDevice->uiAddress + VIDEO_ROW_TABLE);
	uiRowSize = (uint32)muiWidth * VIDEO_TEXT_CELL_HEIGHT;
	uiPhysical = uiTop;
	uiRow = 0;
	uiLine = 0;
	for (y = 0; y < muiHeight; y++)
	{
		uiAddress = ((uiRow < muiRows) ? (uint32)uiPhysical * uiRowSize : (uint32)muiRows * uiRowSize) + (uint32)uiLine * muiWidth;
		puiRow[0] = (uint8)uiAddress;
		puiRow[1] = (uint8)(uiAddress >> 8);
		puiRow[2] = (uint8)(uiAddress >> 16);
		puiRow += 4;

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
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The physical cell (in mauiChars) logical cell uiCell is: its row moved
// down by the scroll.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalVideoBoard::PhysicalCell(uint16 uiCell)
{
	uint16	uiRow;
	uint16	uiColumn;

	uiRow = (uint16)(uiCell / muiColumns);
	uiColumn = (uint16)(uiCell - uiRow * muiColumns);
	uiRow = (uint16)(uiRow + muiTop);
	if (uiRow >= muiRows)
	{
		uiRow = (uint16)(uiRow - muiRows);
	}
	return (uint16)(uiRow * muiColumns + uiColumn);
}


//////////////////////////////////////////////////////////////////////////
//
// Physical cell uiCell drawn in the frame being drawn: its character's
// glyph, a byte a pixel, in its colour's palette (the cursor's if it is
// the cursor's cell).
//
//////////////////////////////////////////////////////////////////////////
void CKernalVideoBoard::RenderCell(uint16 uiCell)
{
	uint8*		puiPixel;
	uint8*		puiGlyph;
	uint32*		puiExpand;
	uint16		uiRow;
	uint16		uiColumn;
	uint16		uiLine;

	uiRow = (uint16)(uiCell / muiColumns);
	uiColumn = (uint16)(uiCell - uiRow * muiColumns);
	mpuiRegisters[VIDEO_PALETTE_BYTE] = (uiCell == muiCursor) ? VIDEO_TEXT_CURSOR_PALETTE : mauiColours[uiCell];
	puiPixel = (uint8*)(muiWindow + (uint32)uiRow * muiWidth * VIDEO_TEXT_CELL_HEIGHT + (uint32)uiColumn * VIDEO_TEXT_CELL_WIDTH);
	puiGlyph = &mauiFont[(uint16)mauiChars[uiCell] * VIDEO_TEXT_CELL_HEIGHT];
	for (uiLine = 0; uiLine < VIDEO_TEXT_CELL_HEIGHT; uiLine++)
	{
		puiExpand = (uint32*)&mauiExpand[(uint16)puiGlyph[uiLine] * 8];
		((uint32*)puiPixel)[0] = puiExpand[0];
		((uint32*)puiPixel)[1] = puiExpand[1];
		puiPixel += muiWidth;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Once the frames have flipped, the cells the frame now being drawn is
// behind in (it was shown while they were drawn in the other).
//
//////////////////////////////////////////////////////////////////////////
void CKernalVideoBoard::CatchUp(void)
{
	uint16	i;

	if (!mbFlipped)
	{
		return;
	}
	if (mbPendingAll)
	{
		for (i = 0; i < muiRows * muiColumns; i++)
		{
			RenderCell(i);
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
// Physical cell uiCell, as mauiChars and mauiColours have it, drawn in
// the frame being drawn and owed to the other, which is shown at the next
// vertical blank.
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
// Every cell drawn again, and owed to the other frame.
//
//////////////////////////////////////////////////////////////////////////
void CKernalVideoBoard::DrawAll(void)
{
	uint16	i;

	mbFlipped = false;
	for (i = 0; i < muiRows * muiColumns; i++)
	{
		RenderCell(i);
	}
	muiPending = 0;
	mbPendingAll = true;
	mbFlip = true;
}


//////////////////////////////////////////////////////////////////////////
//
// The running process's uiLength characters at uiUserAddress into the
// text from logical cell uiCell, in the text colour.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalVideoBoard::WriteText(uint32 uiCell, uint32 uiUserAddress, uint32 uiLength, uint32* puiDone)
{
	uint8	auiChars[VIDEO_TEXT_CHUNK];
	uint32	uiChunk;
	uint16	uiPhysical;
	uint16	i;

	if (!mbText || (uiCell >= (uint32)muiRows * muiColumns) || (uiLength > (uint32)muiRows * muiColumns - uiCell))
	{
		return KERNAL_DRIVER_BAD_ARGUMENT;
	}
	while (uiLength > 0)
	{
		uiChunk = (uiLength < VIDEO_TEXT_CHUNK) ? uiLength : VIDEO_TEXT_CHUNK;
		if (!KernalCopyFromUser(auiChars, uiUserAddress, uiChunk))
		{
			return KERNAL_DRIVER_BAD_ARGUMENT;
		}
		for (i = 0; i < uiChunk; i++)
		{
			uiPhysical = PhysicalCell((uint16)uiCell);
			mauiChars[uiPhysical] = auiChars[i];
			mauiColours[uiPhysical] = muiTextColour;
			DrawCell(uiPhysical);
			uiCell++;
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
// blank in the text colour.  The row table changes at the vertical blank
// that shows them.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalVideoBoard::ScrollText(uint32 uiLines)
{
	uint16	uiPhysical;
	uint16	uiLine;
	uint16	i;

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
		for (i = 0; i < muiColumns; i++)
		{
			uiPhysical = PhysicalCell((uint16)((muiRows - 1) * muiColumns + i));
			mauiChars[uiPhysical] = ' ';
			mauiColours[uiPhysical] = muiTextColour;
			DrawCell(uiPhysical);
		}
	}
	return KERNAL_DRIVER_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////
//
// Every cell blank in the text colour.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalVideoBoard::ClearText(void)
{
	if (!mbText)
	{
		return KERNAL_DRIVER_BAD_ARGUMENT;
	}
	CatchUp();
	muiCursor = VIDEO_CURSOR_NONE;
	memset(mauiChars, ' ', sizeof(mauiChars));
	memset(mauiColours, muiTextColour, sizeof(mauiColours));
	DrawAll();
	return KERNAL_DRIVER_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////
//
// The cursor on physical cell uiCell (VIDEO_CURSOR_NONE: hidden): the
// cell it leaves is drawn in its own colour again.
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
			puiCursor[(uint32)i * VIDEO_BOARD_GREEN] = puiCursor[(uint32)i * VIDEO_BOARD_GREEN + 1];
			puiCursor[(uint32)i * VIDEO_BOARD_GREEN + 1] = uiSwap;
		}
	}
}
