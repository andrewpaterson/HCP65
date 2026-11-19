#ifndef __HCP65_LCD_DRAW_H__
#define __HCP65_LCD_DRAW_H__
#include "WindowLib/CanvasDraw.h"
#include "HCP65Lib/HCP65LCD.h"


class CHCP65;


//The LCD and SNES Board's 16 by 2 character display, drawn as the module looks (grey and reflective: dark dots on light
//grey glass in a black bezel) in the middle of the canvas, on the background.  It wants no height while there's no board,
//so its row of the window goes.  It reads the board's picture as it draws (the board is driven on the HCP65's thread and
//gives the picture under its lock), at the HCP65's cycle then for the blinking cursor.
class CHCP65LCDDraw : public CCanvasDraw
{
CONSTRUCTABLE(CHCP65LCDDraw);
DESTRUCTABLE(CHCP65LCDDraw);
protected:
	ARGB32				msBackground;
	CHCP65LCDBoard*		mpcLCD;
	CHCP65*				mpcHCP65;

public:
	void		Init(ARGB32 sBackground, CHCP65* pcHCP65);
	void		Class(void) override;
	void		Free(void) override;

	bool		Save(CObjectWriter* pcFile) override;
	bool		Load(CObjectReader* pcFile) override;

	bool		Draw(Ptr<CCanvas> pCanvas) override;
	SInt32Vec2	GetDesiredSize(void) override;

	void		SetLCD(CHCP65LCDBoard* pcLCD);
	bool		HasLCD(void) const;
};


#endif // __HCP65_LCD_DRAW_H__
