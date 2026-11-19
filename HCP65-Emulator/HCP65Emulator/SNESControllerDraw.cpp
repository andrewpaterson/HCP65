#include "BaseLib/UTF8.h"
#include "StandardLib/ClassDefines.h"
#include "StandardLib/Objects.h"
#include "SupportLib/ImageDraw.h"
#include "SupportLib/ImageColourConverter.h"
#include "SupportLib/SimpleTextReader.h"
#include "WindowLib/Canvas.h"
#include "WindowLib/Window.h"
#include "HCP65Lib/HCP65LCD.h"
#include "HCP65SNESKeys.h"
#include "SNESControllerDraw.h"


//The controller's shape, in parts of its width (the units here): the body is SNES_PAD_BODY_HEIGHT tall with the
//shoulders SNES_PAD_SHOULDER_RISE above it.
#define SNES_PAD_MARGIN				12		//Pixels between the controller and the canvas's sides.
#define SNES_PAD_DESIRED_WIDTH		640		//Pixels, as the dialog would have it.
#define SNES_PAD_BODY_HEIGHT		0.42
#define SNES_PAD_SHOULDER_RISE		0.06
#define SNES_PAD_HEIGHT				(SNES_PAD_BODY_HEIGHT + SNES_PAD_SHOULDER_RISE)
#define SNES_PAD_SHOULDER_LEFT		0.06	//L's left, and R's right from the right.
#define SNES_PAD_SHOULDER_WIDTH		0.30
#define SNES_PAD_SHOULDER_DEPTH		0.10	//How far down the body the shoulders go behind it.
#define SNES_PAD_SHOULDER_RADIUS	0.05
#define SNES_PAD_DPAD_X				0.21	//The D-pad's centre, from the left and from the body's top.
#define SNES_PAD_DPAD_Y				0.22
#define SNES_PAD_DPAD_ARM			0.105	//From the centre to an arm's end.
#define SNES_PAD_DPAD_HALF_WIDTH	0.036	//Half an arm's width.
#define SNES_PAD_FACE_X				0.79	//The diamond's centre.
#define SNES_PAD_FACE_Y				0.22
#define SNES_PAD_FACE_SPREAD		0.088	//From the diamond's centre to a button's.
#define SNES_PAD_FACE_RADIUS		0.046
#define SNES_PAD_MIDDLE_X			0.072	//Select's and Start's centres from the middle.
#define SNES_PAD_MIDDLE_Y			0.27
#define SNES_PAD_MIDDLE_WIDTH		0.08
#define SNES_PAD_MIDDLE_HEIGHT		0.032
#define SNES_PAD_MIDDLE_LABEL		0.045	//Below the buttons' centres to their labels'.
#define SNES_PAD_ARROW				0.032	//The D-pad's arrows across.


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
static void SetPadColour(CImageColourARGB* pcColour, ARGB32 sColour)
{
	pcColour->Init(1.0f, Get8BitRedColour(sColour) / 255.0f, Get8BitGreenColour(sColour) / 255.0f, Get8BitBlueColour(sColour) / 255.0f);
}


//////////////////////////////////////////////////////////////////////////
//
// The font is the colour the text is drawn in.
//
//////////////////////////////////////////////////////////////////////////
void CSNESControllerDraw::Init(Ptr<CFont> pFont, Ptr<CWindowTheme> pTheme)
{
	PreInit();

	msBackground = pTheme->GetColour(TC_Editor);
	msBorder = pTheme->GetColour(TC_Border);
	msBody = pTheme->GetColour(TC_Panel);
	msFace = pTheme->GetColour(TC_Editor);
	msLegend = pTheme->GetColour(TC_Text);
	msPressedBorder = pTheme->GetColour(TC_Accent);
	msPressedFace = pTheme->GetColour(TC_ListSelected);

	CCanvasDraw::Init();
	mpCelDraw = OMalloc<CImageCanvasDraw>(Ptr<CImageCel>(), false);
	mpFont = pFont;
	mpLayout = OMalloc<CTextLayout>();
	mpGlyphs = OMalloc<CGlyphListFontDraw>();
	msCelSize.Init(-1, -1);
	muiPressed = 0;
	miFocused = -1;
	mbChanged = true;

	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CSNESControllerDraw::Class(void)
{
	U_UInt32(msBackground);
	U_UInt32(msBorder);
	U_UInt32(msBody);
	U_UInt32(msFace);
	U_UInt32(msLegend);
	U_UInt32(msPressedBorder);
	U_UInt32(msPressedFace);
	CCanvasDraw::Class();
	M_Pointer(mpCelDraw);
	M_Pointer(mpFont);
	M_Pointer(mpLayout);
	M_Pointer(mpGlyphs);
	U_2Int32(msCelSize);
	U_UInt16(muiPressed);
	U_Int32(miFocused);
	U_Bool(mbChanged);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CSNESControllerDraw::Free(void)
{
	CCanvasDraw::Free();
}


bool CSNESControllerDraw::Save(CObjectWriter* pcFile) { return false; }
bool CSNESControllerDraw::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// As wide as the dialog would have it, and as tall as the controller is
// then.
//
//////////////////////////////////////////////////////////////////////////
SInt32Vec2 CSNESControllerDraw::GetDesiredSize(void)
{
	SInt32Vec2	sSize;

	sSize.x = SNES_PAD_DESIRED_WIDTH;
	sSize.y = (int32)((SNES_PAD_DESIRED_WIDTH - 2 * SNES_PAD_MARGIN) * SNES_PAD_HEIGHT) + 2 * SNES_PAD_MARGIN;
	return sSize;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CSNESControllerDraw::SetPressed(uint16 uiPressed)
{
	if (uiPressed != muiPressed)
	{
		muiPressed = uiPressed;
		mbChanged = true;
	}
}


void CSNESControllerDraw::SetFocused(int iButton)
{
	if (iButton != miFocused)
	{
		miFocused = iButton;
		mbChanged = true;
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CSNESControllerDraw::Draw(Ptr<CCanvas> pCanvas)
{
	SInt32Vec2	sSize;

	sSize = pCanvas->GetActualSize();
	if ((sSize.x <= 0) || (sSize.y <= 0))
	{
		return true;
	}
	if ((sSize.x != msCelSize.x) || (sSize.y != msCelSize.y) || mbChanged || mpCelDraw->GetCel().IsNull())
	{
		MakeCel(pCanvas, sSize);
	}
	return mpCelDraw->Draw(pCanvas);
}


//////////////////////////////////////////////////////////////////////////
//
// The parts are drawn in RGBA, as the anti-aliased shapes need opacity,
// then the image is put in the canvas's colour order and the legends are
// drawn onto it, as the text's glyphs are in the font's order.  Edges are
// on whole pixels so that only the curves are blended.
//
//////////////////////////////////////////////////////////////////////////
void CSNESControllerDraw::MakeCel(Ptr<CCanvas> pCanvas, SInt32Vec2 sSize)
{
	Ptr<CImage>			pImage;
	CImageDraw			cDraw;
	CImageColourARGB	cBackground;
	CImageColourARGB	cBorder;
	CImageColourARGB	cBody;
	CImageColourARGB	cFace;
	CImageColourARGB	cLegend;
	CImageColourARGB	cPressedBorder;
	CImageColourARGB	cPressedFace;
	float64				fUnit;
	float64				fLeft;
	float64				fTop;
	float64				fBodyTop;
	float64				fBodyBottom;
	float64				fRight;
	float64				fShoulderWidth;
	float64				fDPadX;
	float64				fDPadY;
	float64				fArm;
	float64				fHalf;
	float64				fFaceX;
	float64				fFaceY;
	float64				fSpread;
	float64				fRadius;
	float64				fMiddleY;
	float64				fMiddleWidth;
	float64				fMiddleHeight;
	float64				fX;
	int					i;

	static const int	aiFaceButtons[4] = { 9, 8, 0, 1 };		//X, A, B and Y: top, right, bottom and left.
	static const float64	afFaceX[4] = { 0, 1, 0, -1 };
	static const float64	afFaceY[4] = { -1, 0, 1, 0 };
	static const char*	aszFaceLegends[4] = { "X", "A", "B", "Y" };

	fUnit = (float64)(sSize.x - 2 * SNES_PAD_MARGIN);
	if ((float64)(sSize.y - 2 * SNES_PAD_MARGIN) / SNES_PAD_HEIGHT < fUnit)
	{
		fUnit = (float64)(sSize.y - 2 * SNES_PAD_MARGIN) / SNES_PAD_HEIGHT;
	}
	fLeft = (int32)((sSize.x - fUnit) / 2);
	fTop = (int32)((sSize.y - fUnit * SNES_PAD_HEIGHT) / 2);
	fRight = (int32)(fLeft + fUnit);
	fBodyTop = (int32)(fTop + fUnit * SNES_PAD_SHOULDER_RISE);
	fBodyBottom = (int32)(fBodyTop + fUnit * SNES_PAD_BODY_HEIGHT);

	pImage = OMalloc<CImage>(sSize.x, sSize.y, PT_uint8, IMAGE_DIFFUSE_RED, IMAGE_DIFFUSE_GREEN, IMAGE_DIFFUSE_BLUE, IMAGE_OPACITY, CHANNEL_STOP);
	pImage->Black();
	SetPadColour(&cBackground, msBackground);
	SetPadColour(&cBorder, msBorder);
	SetPadColour(&cBody, msBody);
	SetPadColour(&cFace, msFace);
	SetPadColour(&cPressedBorder, msPressedBorder);
	SetPadColour(&cPressedFace, msPressedFace);
	cDraw.Init(&pImage);
	cDraw.SetColour(&cBackground);
	cDraw.DrawRectangle(0, 0, sSize.x, sSize.y);

	if (fUnit > 64)
	{
		//The shoulders, behind the body's top corners.
		fShoulderWidth = (int32)(fUnit * SNES_PAD_SHOULDER_WIDTH);
		fX = (int32)(fLeft + fUnit * SNES_PAD_SHOULDER_LEFT);
		DrawPart(&cDraw, 10, fX, fTop, fX + fShoulderWidth, (int32)(fBodyTop + fUnit * SNES_PAD_SHOULDER_DEPTH), fUnit * SNES_PAD_SHOULDER_RADIUS, 0, &cBorder, &cBody, &cPressedBorder, &cPressedFace);
		fX = (int32)(fRight - fUnit * SNES_PAD_SHOULDER_LEFT);
		DrawPart(&cDraw, 11, fX - fShoulderWidth, fTop, fX, (int32)(fBodyTop + fUnit * SNES_PAD_SHOULDER_DEPTH), fUnit * SNES_PAD_SHOULDER_RADIUS, 0, &cBorder, &cBody, &cPressedBorder, &cPressedFace);

		//The body, round at both ends.
		fRadius = (fBodyBottom - fBodyTop) / 2;
		cDraw.SetColour(&cBorder);
		cDraw.DrawRoundedRectangle(fLeft, fBodyTop, fRight, fBodyBottom, fRadius);
		cDraw.SetColour(&cBody);
		cDraw.DrawRoundedRectangle(fLeft + 1, fBodyTop + 1, fRight - 1, fBodyBottom - 1, fRadius - 1);

		//The D-pad: its middle, then its arms (up, down, left and right are buttons 4 to 7) over it.
		fDPadX = (int32)(fLeft + fUnit * SNES_PAD_DPAD_X);
		fDPadY = (int32)(fBodyTop + fUnit * SNES_PAD_DPAD_Y);
		fArm = (int32)(fUnit * SNES_PAD_DPAD_ARM);
		fHalf = (int32)(fUnit * SNES_PAD_DPAD_HALF_WIDTH);
		cDraw.SetColour(&cBorder);
		cDraw.DrawRectangle(fDPadX - fHalf, fDPadY - fHalf, fDPadX + fHalf, fDPadY + fHalf);
		cDraw.SetColour(&cFace);
		cDraw.DrawRectangle(fDPadX - fHalf + 1, fDPadY - fHalf + 1, fDPadX + fHalf - 1, fDPadY + fHalf - 1);
		DrawPart(&cDraw, 4, fDPadX - fHalf, fDPadY - fArm, fDPadX + fHalf, fDPadY - fHalf + 1, 2, 0, &cBorder, &cFace, &cPressedBorder, &cPressedFace);
		DrawPart(&cDraw, 5, fDPadX - fHalf, fDPadY + fHalf - 1, fDPadX + fHalf, fDPadY + fArm, 0, 2, &cBorder, &cFace, &cPressedBorder, &cPressedFace);
		DrawPart(&cDraw, 6, fDPadX - fArm, fDPadY - fHalf, fDPadX - fHalf + 1, fDPadY + fHalf, 2, 2, &cBorder, &cFace, &cPressedBorder, &cPressedFace);
		DrawPart(&cDraw, 7, fDPadX + fHalf - 1, fDPadY - fHalf, fDPadX + fArm, fDPadY + fHalf, 2, 2, &cBorder, &cFace, &cPressedBorder, &cPressedFace);
		SetPadColour(&cLegend, msLegend);
		cDraw.SetColour(&cLegend);
		DrawTriangle(&cDraw, fDPadX, fDPadY - (fArm + fHalf) / 2, fUnit * SNES_PAD_ARROW, 4);
		DrawTriangle(&cDraw, fDPadX, fDPadY + (fArm + fHalf) / 2, fUnit * SNES_PAD_ARROW, 5);
		DrawTriangle(&cDraw, fDPadX - (fArm + fHalf) / 2, fDPadY, fUnit * SNES_PAD_ARROW, 6);
		DrawTriangle(&cDraw, fDPadX + (fArm + fHalf) / 2, fDPadY, fUnit * SNES_PAD_ARROW, 7);
		cLegend.Kill();

		//X, A, B and Y in a diamond.
		fFaceX = (int32)(fLeft + fUnit * SNES_PAD_FACE_X);
		fFaceY = (int32)(fBodyTop + fUnit * SNES_PAD_FACE_Y);
		fSpread = fUnit * SNES_PAD_FACE_SPREAD;
		fRadius = (int32)(fUnit * SNES_PAD_FACE_RADIUS);
		for (i = 0; i < 4; i++)
		{
			fX = (int32)(fFaceX + afFaceX[i] * fSpread);
			fMiddleY = (int32)(fFaceY + afFaceY[i] * fSpread);
			DrawPart(&cDraw, aiFaceButtons[i], fX - fRadius, fMiddleY - fRadius, fX + fRadius, fMiddleY + fRadius, fRadius, fRadius, &cBorder, &cFace, &cPressedBorder, &cPressedFace);
		}

		//Select and Start, round ended, either side of the middle.
		fMiddleY = (int32)(fBodyTop + fUnit * SNES_PAD_MIDDLE_Y);
		fMiddleWidth = (int32)(fUnit * SNES_PAD_MIDDLE_WIDTH);
		fMiddleHeight = (int32)(fUnit * SNES_PAD_MIDDLE_HEIGHT);
		for (i = 0; i < 2; i++)
		{
			fX = (int32)(fLeft + fUnit / 2 + (i == 0 ? -1 : 1) * fUnit * SNES_PAD_MIDDLE_X);
			DrawPart(&cDraw, 2 + i, fX - fMiddleWidth / 2, fMiddleY - fMiddleHeight / 2, fX + fMiddleWidth / 2, fMiddleY + fMiddleHeight / 2, fMiddleHeight / 2, fMiddleHeight / 2, &cBorder, &cFace, &cPressedBorder, &cPressedFace);
		}
	}
	cDraw.Kill();
	cPressedFace.Kill();
	cPressedBorder.Kill();
	cFace.Kill();
	cBody.Kill();
	cBorder.Kill();
	cBackground.Kill();
	pImage = CImageColourConverter::ConvertColour(pImage, pCanvas->GetColourOrder());

	//The legends.
	if (fUnit > 64)
	{
		cDraw.Init(&pImage);
		DrawText(&cDraw, "L", (int32)(fLeft + fUnit * (SNES_PAD_SHOULDER_LEFT + SNES_PAD_SHOULDER_WIDTH / 2)), (int32)((fTop + fBodyTop) / 2) + 1);
		DrawText(&cDraw, "R", (int32)(fRight - fUnit * (SNES_PAD_SHOULDER_LEFT + SNES_PAD_SHOULDER_WIDTH / 2)), (int32)((fTop + fBodyTop) / 2) + 1);
		for (i = 0; i < 4; i++)
		{
			DrawText(&cDraw, aszFaceLegends[i], (int32)(fFaceX + afFaceX[i] * fSpread), (int32)(fFaceY + afFaceY[i] * fSpread));
		}
		DrawText(&cDraw, "SELECT", (int32)(fLeft + fUnit / 2 - fUnit * SNES_PAD_MIDDLE_X), (int32)(fMiddleY + fUnit * SNES_PAD_MIDDLE_LABEL));
		DrawText(&cDraw, "START", (int32)(fLeft + fUnit / 2 + fUnit * SNES_PAD_MIDDLE_X), (int32)(fMiddleY + fUnit * SNES_PAD_MIDDLE_LABEL));
		cDraw.Kill();
	}

	mpCelDraw->SetCel(OMalloc<CImageCel>(pImage));
	msCelSize = sSize;
	mbChanged = false;
}


//////////////////////////////////////////////////////////////////////////
//
// Button iButton's shape: its border colour (the pressed one if it is
// pressed or being given a key) with its face a pixel inside it, the
// pressed face if it is pressed.
//
//////////////////////////////////////////////////////////////////////////
void CSNESControllerDraw::DrawPart(CImageDraw* pcDraw, int iButton, float64 fLeft, float64 fTop, float64 fRight, float64 fBottom, float64 fTopRadius, float64 fBottomRadius, CImageColourARGB* pcBorder, CImageColourARGB* pcFace, CImageColourARGB* pcPressedBorder, CImageColourARGB* pcPressedFace)
{
	bool	bPressed;

	bPressed = (muiPressed & (1 << iButton)) != 0;
	pcDraw->SetColour((bPressed || (iButton == miFocused)) ? pcPressedBorder : pcBorder);
	pcDraw->DrawRoundedRectangle(fLeft, fTop, fRight, fBottom, fTopRadius, fBottomRadius);
	pcDraw->SetColour(bPressed ? pcPressedFace : pcFace);
	pcDraw->DrawRoundedRectangle(fLeft + 1, fTop + 1, fRight - 1, fBottom - 1, (fTopRadius > 1) ? fTopRadius - 1 : 0, (fBottomRadius > 1) ? fBottomRadius - 1 : 0);
}


//////////////////////////////////////////////////////////////////////////
//
// A filled triangle fSize across, centred on the point, pointing the way
// the D-pad's button iButton (4 up to 7 right) is.
//
//////////////////////////////////////////////////////////////////////////
void CSNESControllerDraw::DrawTriangle(CImageDraw* pcDraw, float64 fCentreX, float64 fCentreY, float64 fSize, int iButton)
{
	SFloat64Vec2	asPoints[3];
	float64			fHalf;
	float64			fDepth;

	fHalf = fSize / 2;
	fDepth = fSize * 0.45;
	switch (iButton)
	{
	case 4:
		asPoints[0].Init(fCentreX, fCentreY - fDepth);
		asPoints[1].Init(fCentreX + fHalf, fCentreY + fDepth);
		asPoints[2].Init(fCentreX - fHalf, fCentreY + fDepth);
		break;
	case 5:
		asPoints[0].Init(fCentreX, fCentreY + fDepth);
		asPoints[1].Init(fCentreX - fHalf, fCentreY - fDepth);
		asPoints[2].Init(fCentreX + fHalf, fCentreY - fDepth);
		break;
	case 6:
		asPoints[0].Init(fCentreX - fDepth, fCentreY);
		asPoints[1].Init(fCentreX + fDepth, fCentreY - fHalf);
		asPoints[2].Init(fCentreX + fDepth, fCentreY + fHalf);
		break;
	default:
		asPoints[0].Init(fCentreX + fDepth, fCentreY);
		asPoints[1].Init(fCentreX - fDepth, fCentreY + fHalf);
		asPoints[2].Init(fCentreX - fDepth, fCentreY - fHalf);
		break;
	}
	pcDraw->DrawPolygon(asPoints, 3);
}


//////////////////////////////////////////////////////////////////////////
//
// The text centred on the point, in the font's colour.
//
//////////////////////////////////////////////////////////////////////////
void CSNESControllerDraw::DrawText(CImageDraw* pcDraw, const char* szText, int32 iCentreX, int32 iCentreY)
{
	CSimpleTextReader		cReader;
	CUTF8					cUTF8;
	Ptr<CText>				pText;
	SInt32Vec2				sTextSize;
	CArrayGlyphPosition*	pasGlyphs;
	SGlyphPosition*			psGlyph;
	size					i;
	int32					x;
	int32					y;

	cUTF8.Init((char*)szText);
	cReader.Init(&mpFont);
	pText = cReader.Read(&cUTF8, false);
	cReader.Kill();
	cUTF8.Kill();
	if (pText.IsNull())
	{
		return;
	}
	mpGlyphs->Begin();
	mpLayout->Layout(mpGlyphs, pText);
	mpGlyphs->End();
	sTextSize = mpLayout->GetSize();
	x = iCentreX - sTextSize.x / 2;
	y = iCentreY - sTextSize.y / 2;
	pasGlyphs = mpGlyphs->GetGlyphs();
	for (i = 0; i < pasGlyphs->NumElements(); i++)
	{
		psGlyph = pasGlyphs->Get(i);
		pcDraw->DrawCel(x + psGlyph->sPosition.x, y + psGlyph->sPosition.y, psGlyph->pcGlyph->GetCel(), NULL);
	}
}
