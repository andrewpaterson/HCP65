#include "StandardLib/ClassDefines.h"
#include "SupportLib/ColourARGB32.h"
#include "WindowLib/Canvas.h"
#include "HCP65Lib/HCP65.h"
#include "HCP65LCDDraw.h"


//The module's look, in pixels.
#define HCP65_LCD_DRAW_DOT			3		//A dot is a square this big,
#define HCP65_LCD_DRAW_DOT_GAP		1		//with this between dots,
#define HCP65_LCD_DRAW_CHAR_GAP		4		//this more between characters,
#define HCP65_LCD_DRAW_LINE_GAP		6		//and this more between the lines.
#define HCP65_LCD_DRAW_GLASS_X		14		//The glass around the characters,
#define HCP65_LCD_DRAW_GLASS_Y		12
#define HCP65_LCD_DRAW_BEZEL		10		//the bezel around the glass,
#define HCP65_LCD_DRAW_MARGIN		8		//and the background above and below the bezel.

#define HCP65_LCD_DRAW_DOT_PITCH	(HCP65_LCD_DRAW_DOT + HCP65_LCD_DRAW_DOT_GAP)
#define HCP65_LCD_DRAW_CHAR_WIDTH	(HCP65_LCD_CHARACTER_WIDTH * HCP65_LCD_DRAW_DOT_PITCH - HCP65_LCD_DRAW_DOT_GAP)
#define HCP65_LCD_DRAW_CHAR_HEIGHT	(HCP65_LCD_CHARACTER_HEIGHT * HCP65_LCD_DRAW_DOT_PITCH - HCP65_LCD_DRAW_DOT_GAP)
#define HCP65_LCD_DRAW_TEXT_WIDTH	(HCP65_LCD_COLUMNS * (HCP65_LCD_DRAW_CHAR_WIDTH + HCP65_LCD_DRAW_CHAR_GAP) - HCP65_LCD_DRAW_CHAR_GAP)
#define HCP65_LCD_DRAW_TEXT_HEIGHT	(HCP65_LCD_ROWS * (HCP65_LCD_DRAW_CHAR_HEIGHT + HCP65_LCD_DRAW_LINE_GAP) - HCP65_LCD_DRAW_LINE_GAP)
#define HCP65_LCD_DRAW_WIDTH		(HCP65_LCD_DRAW_TEXT_WIDTH + 2 * (HCP65_LCD_DRAW_GLASS_X + HCP65_LCD_DRAW_BEZEL))
#define HCP65_LCD_DRAW_HEIGHT		(HCP65_LCD_DRAW_TEXT_HEIGHT + 2 * (HCP65_LCD_DRAW_GLASS_Y + HCP65_LCD_DRAW_BEZEL + HCP65_LCD_DRAW_MARGIN))


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65LCDDraw::Init(ARGB32 sBackground, CHCP65* pcHCP65)
{
	PreInit();

	CCanvasDraw::Init();
	msBackground = sBackground;
	mpcHCP65 = pcHCP65;
	mpcLCD = NULL;

	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65LCDDraw::Class(void)
{
	CCanvasDraw::Class();
	U_UInt32(msBackground);
	U_Pointer(mpcLCD);
	U_Pointer(mpcHCP65);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65LCDDraw::Free(void)
{
	CCanvasDraw::Free();
}


bool CHCP65LCDDraw::Save(CObjectWriter* pcFile) { return false; }
bool CHCP65LCDDraw::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// As big as the module and its margins; nothing without a board.
//
//////////////////////////////////////////////////////////////////////////
SInt32Vec2 CHCP65LCDDraw::GetDesiredSize(void)
{
	SInt32Vec2	sSize;

	sSize.x = mpcLCD ? HCP65_LCD_DRAW_WIDTH : 0;
	sSize.y = mpcLCD ? HCP65_LCD_DRAW_HEIGHT : 0;
	return sSize;
}


//////////////////////////////////////////////////////////////////////////
//
// The background, the bezel and glass in the middle, and every dot: dark
// if it's on, a shade darker than the glass if it's off (as the glass
// shows them).  A display that is off shows only the glass.
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65LCDDraw::Draw(Ptr<CCanvas> pCanvas)
{
	CInt32Rect			cRect;
	SInt32Vec2			sSize;
	SHCP65LCDPicture	sPicture;
	int					iLeft;
	int					iTop;
	int					iRow;
	int					iColumn;
	int					iDotRow;
	int					iDot;
	int					x;
	int					y;
	bool				bOn;
	ARGB32				sBezel;
	ARGB32				sGlass;
	ARGB32				sDotOff;
	ARGB32				sDotOn;

	if (!pCanvas->IsValid())
	{
		return true;
	}

	//DrawBox includes the right and bottom edges of its rectangle.
	sSize = pCanvas->GetActualSize();
	cRect.Init(0, 0, sSize.x - 1, sSize.y - 1);
	pCanvas->DrawBox(&cRect, true, msBackground);
	if (!mpcLCD)
	{
		return true;
	}

	sBezel = Set32BitColour(24u, 24u, 24u);
	sGlass = Set32BitColour(184u, 190u, 178u);
	sDotOff = Set32BitColour(174u, 180u, 168u);
	sDotOn = Set32BitColour(36u, 40u, 36u);

	iLeft = (sSize.x - HCP65_LCD_DRAW_WIDTH) / 2;
	iTop = (sSize.y - HCP65_LCD_DRAW_HEIGHT) / 2 + HCP65_LCD_DRAW_MARGIN;
	cRect.Init(iLeft, iTop, iLeft + HCP65_LCD_DRAW_WIDTH - 1, iTop + HCP65_LCD_DRAW_HEIGHT - 2 * HCP65_LCD_DRAW_MARGIN - 1);
	pCanvas->DrawBox(&cRect, true, sBezel);
	iLeft += HCP65_LCD_DRAW_BEZEL;
	iTop += HCP65_LCD_DRAW_BEZEL;
	cRect.Init(iLeft, iTop, iLeft + HCP65_LCD_DRAW_TEXT_WIDTH + 2 * HCP65_LCD_DRAW_GLASS_X - 1, iTop + HCP65_LCD_DRAW_TEXT_HEIGHT + 2 * HCP65_LCD_DRAW_GLASS_Y - 1);
	pCanvas->DrawBox(&cRect, true, sGlass);
	iLeft += HCP65_LCD_DRAW_GLASS_X;
	iTop += HCP65_LCD_DRAW_GLASS_Y;

	mpcLCD->GetPicture(&sPicture, mpcHCP65 ? mpcHCP65->NumCycles() : 0);
	for (iRow = 0; iRow < HCP65_LCD_ROWS; iRow++)
	{
		for (iColumn = 0; iColumn < HCP65_LCD_COLUMNS; iColumn++)
		{
			for (iDotRow = 0; iDotRow < HCP65_LCD_CHARACTER_HEIGHT; iDotRow++)
			{
				for (iDot = 0; iDot < HCP65_LCD_CHARACTER_WIDTH; iDot++)
				{
					bOn = sPicture.bOn && ((sPicture.aauiDots[iRow][iColumn][iDotRow] >> (HCP65_LCD_CHARACTER_WIDTH - 1 - iDot)) & 1);
					x = iLeft + iColumn * (HCP65_LCD_DRAW_CHAR_WIDTH + HCP65_LCD_DRAW_CHAR_GAP) + iDot * HCP65_LCD_DRAW_DOT_PITCH;
					y = iTop + iRow * (HCP65_LCD_DRAW_CHAR_HEIGHT + HCP65_LCD_DRAW_LINE_GAP) + iDotRow * HCP65_LCD_DRAW_DOT_PITCH;
					cRect.Init(x, y, x + HCP65_LCD_DRAW_DOT - 1, y + HCP65_LCD_DRAW_DOT - 1);
					pCanvas->DrawBox(&cRect, true, bOn ? sDotOn : sDotOff);
				}
			}
		}
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The board whose display is drawn, NULL for none.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65LCDDraw::SetLCD(CHCP65LCDBoard* pcLCD)
{
	mpcLCD = pcLCD;
}


bool CHCP65LCDDraw::HasLCD(void) const { return mpcLCD != NULL; }
