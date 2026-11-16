#include <string.h>
#include "StandardLib/ClassDefines.h"
#include "StandardLib/Objects.h"
#include "SupportLib/ImageCel.h"
#include "WindowLib/Canvas.h"
#include "HCP65Lib/HCP65Video.h"
#include "HCP65ScreenDraw.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65ScreenDraw::Init(ARGB32 sBackground)
{
	PreInit();

	CCanvasDraw::Init();
	mpImageDraw = OMalloc<CImageCanvasDraw>(Ptr<CImageCel>(), false);
	mpImageDraw->SetCelAlignment(CSH_Center, CSV_Center);
	msBackground = sBackground;
	mpcVideo = NULL;
	muiFrames = 0;
	mbSignal = false;
	muiScale = 0;
	mbScaledStale = false;

	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65ScreenDraw::Class(void)
{
	CCanvasDraw::Class();
	M_Pointer(mpImageDraw);
	M_Pointer(mpImage);
	M_Pointer(mpScaled);
	U_UInt32(muiScale);
	U_Bool(mbScaledStale);
	U_UInt32(msBackground);
	U_Pointer(mpcVideo);
	U_UInt64(muiFrames);
	U_Bool(mbSignal);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65ScreenDraw::Free(void)
{
	CCanvasDraw::Free();
}


bool CHCP65ScreenDraw::Save(CObjectWriter* pcFile) { return false; }
bool CHCP65ScreenDraw::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// The background, then the frame over it if there is a picture.
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65ScreenDraw::Draw(Ptr<CCanvas> pCanvas)
{
	CInt32Rect	cRect;
	SInt32Vec2	sSize;

	if (!pCanvas->IsValid())
	{
		return true;
	}

	//DrawBox includes the right and bottom edges of its rectangle.
	sSize = pCanvas->GetActualSize();
	sSize.x--;
	sSize.y--;
	cRect.Init(sSize);
	pCanvas->DrawBox(&cRect, true, msBackground);
	if (mbSignal && mpImage.IsNotNull())
	{
		Scale(pCanvas->GetActualSize());
		return mpImageDraw->Draw(pCanvas);
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The cel is the frame as many times as big as fits sSize (at least once),
// each pixel a square of them.  Made again when the frame or the scale
// changes.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65ScreenDraw::Scale(SInt32Vec2 sSize)
{
	uint32			uiWidth;
	uint32			uiHeight;
	uint32			uiScale;
	uint32			uiScaledWidth;
	uint32*			puiSource;
	uint32*			puiRow;
	uint32*			puiDest;
	uint32			x;
	uint32			y;
	uint32			i;

	uiWidth = (uint32)mpImage->GetWidth();
	uiHeight = (uint32)mpImage->GetHeight();
	uiScale = 1;
	if ((sSize.x > 0) && (sSize.y > 0))
	{
		uiScale = (uint32)sSize.x / uiWidth;
		if ((uint32)sSize.y / uiHeight < uiScale)
		{
			uiScale = (uint32)sSize.y / uiHeight;
		}
		if (uiScale < 1)
		{
			uiScale = 1;
		}
	}

	if (uiScale != muiScale)
	{
		muiScale = uiScale;
		if (uiScale == 1)
		{
			mpScaled = NULL;
			mpImageDraw->SetCel(OMalloc<CImageCel>(mpImage));
		}
		else
		{
			mpScaled = OMalloc<CImage>((int)(uiWidth * uiScale), (int)(uiHeight * uiScale), PT_uint8, IMAGE_DIFFUSE_BLUE, IMAGE_DIFFUSE_GREEN, IMAGE_DIFFUSE_RED, IMAGE_IGNORED, CHANNEL_STOP);
			mpImageDraw->SetCel(OMalloc<CImageCel>(mpScaled));
		}
		mbScaledStale = true;
	}
	if (!mbScaledStale || mpScaled.IsNull())
	{
		mbScaledStale = false;
		return;
	}

	uiScaledWidth = uiWidth * uiScale;
	puiSource = (uint32*)mpImage->GetData();
	puiDest = (uint32*)mpScaled->GetData();
	for (y = 0; y < uiHeight; y++)
	{
		puiRow = puiDest;
		for (x = 0; x < uiWidth; x++)
		{
			for (i = 0; i < uiScale; i++)
			{
				*puiDest++ = puiSource[x];
			}
		}
		for (i = 1; i < uiScale; i++)
		{
			memcpy(puiDest, puiRow, uiScaledWidth * sizeof(uint32));
			puiDest += uiScaledWidth;
		}
		puiSource += uiWidth;
	}
	mbScaledStale = false;
}


//////////////////////////////////////////////////////////////////////////
//
// The Video Board whose frames are shown, NULL for none.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65ScreenDraw::SetVideo(CHCP65VideoBoard* pcVideo)
{
	mpcVideo = pcVideo;
	muiFrames = 0;
	mbSignal = false;
}


//////////////////////////////////////////////////////////////////////////
//
// The board's new frame into the image (made again at the frame's size
// if that has changed).  True if what is shown has changed.
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65ScreenDraw::Update(void)
{
	uint32	uiWidth;
	uint32	uiHeight;
	uint32	uiImageWidth;
	uint32	uiImageHeight;
	uint64	uiFrames;
	bool	bSignal;

	if (!mpcVideo)
	{
		bSignal = mbSignal;
		mbSignal = false;
		return bSignal;
	}
	uiFrames = mpcVideo->NumFrames();
	if ((uiFrames == muiFrames) && (mbSignal == mpcVideo->IsSignal()))
	{
		return false;
	}

	uiImageWidth = mpImage.IsNotNull() ? (uint32)mpImage->GetWidth() : 0;
	uiImageHeight = mpImage.IsNotNull() ? (uint32)mpImage->GetHeight() : 0;
	bSignal = mpcVideo->GetFrame(mpImage.IsNotNull() ? (uint32*)mpImage->GetData() : NULL, uiImageWidth, uiImageHeight, &uiWidth, &uiHeight);
	if (bSignal && ((uiWidth != uiImageWidth) || (uiHeight != uiImageHeight)))
	{
		mpImage = OMalloc<CImage>((int)uiWidth, (int)uiHeight, PT_uint8, IMAGE_DIFFUSE_BLUE, IMAGE_DIFFUSE_GREEN, IMAGE_DIFFUSE_RED, IMAGE_IGNORED, CHANNEL_STOP);
		mpScaled = NULL;
		muiScale = 0;
		bSignal = mpcVideo->GetFrame((uint32*)mpImage->GetData(), uiWidth, uiHeight, &uiWidth, &uiHeight);
	}
	muiFrames = uiFrames;
	mbSignal = bSignal;
	mbScaledStale = true;
	return true;
}
