#ifndef __SNES_CONTROLLER_DRAW_H__
#define __SNES_CONTROLLER_DRAW_H__
#include "SupportLib/ColourARGB32.h"
#include "SupportLib/Int32Rect.h"
#include "SupportLib/Font.h"
#include "SupportLib/TextLayout.h"
#include "SupportLib/GlyphListFontDraw.h"
#include "WindowLib/ImageCanvasDraw.h"
#include "WindowLib/WindowTheme.h"


class CImageDraw;
class CImageColourARGB;


//Draws a SNES controller from the front, as WindowInputTester draws its keyboard and mouse: anti-aliased shapes, each
//its border colour with its face a pixel inside it, as large as fits in the canvas and centred in it.  The body is a
//rounded bar, round at both ends, with the L and R shoulder buttons showing above its top corners; the D-pad (a cross of
//four arms) on its left, the A, B, X and Y buttons in a diamond on its right (X at the top, A on the right) and the
//Select and Start buttons between them, each with its letters.
//
//The buttons pressed (SetPressed, HCP65_SNES_* bits) are highlighted as a pressed key is; the one being given a key
//(SetFocused) has the pressed border.  The controller is drawn into a cel the size of the canvas, made again only when
//the canvas changes size or what is shown changes, and an image draw puts the cel on the canvas.
class CSNESControllerDraw : public CCanvasDraw
{
CONSTRUCTABLE(CSNESControllerDraw);
DESTRUCTABLE(CSNESControllerDraw);
protected:
	ARGB32					msBackground;
	ARGB32					msBorder;
	ARGB32					msBody;
	ARGB32					msFace;
	ARGB32					msLegend;
	ARGB32					msPressedBorder;
	ARGB32					msPressedFace;
	Ptr<CImageCanvasDraw>	mpCelDraw;
	Ptr<CFont>				mpFont;
	Ptr<CTextLayout>		mpLayout;
	Ptr<CGlyphListFontDraw>	mpGlyphs;
	SInt32Vec2				msCelSize;		//The canvas size the cel was made for, -1, -1 for none yet.
	uint16					muiPressed;
	int						miFocused;		//The button being given a key, -1 for none.
	bool					mbChanged;		//Since the cel was made.

public:
	void		Init(Ptr<CFont> pFont, Ptr<CWindowTheme> pTheme);
	void		Class(void) override;
	void 		Free(void) override;

	bool		Save(CObjectWriter* pcFile) override;
	bool		Load(CObjectReader* pcFile) override;

	bool		Draw(Ptr<CCanvas> pCanvas) override;
	SInt32Vec2	GetDesiredSize(void) override;

	void		SetPressed(uint16 uiPressed);
	void		SetFocused(int iButton);

protected:
	void		MakeCel(Ptr<CCanvas> pCanvas, SInt32Vec2 sSize);
	void		DrawPart(CImageDraw* pcDraw, int iButton, float64 fLeft, float64 fTop, float64 fRight, float64 fBottom, float64 fTopRadius, float64 fBottomRadius, CImageColourARGB* pcBorder, CImageColourARGB* pcFace, CImageColourARGB* pcPressedBorder, CImageColourARGB* pcPressedFace);
	void		DrawTriangle(CImageDraw* pcDraw, float64 fCentreX, float64 fCentreY, float64 fSize, int iButton);
	void		DrawText(CImageDraw* pcDraw, const char* szText, int32 iCentreX, int32 iCentreY);
};


#endif // __SNES_CONTROLLER_DRAW_H__
