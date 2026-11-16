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
		return mpImageDraw->Draw(pCanvas);
	}
	return true;
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
		mpImageDraw->SetCel(OMalloc<CImageCel>(mpImage));
		bSignal = mpcVideo->GetFrame((uint32*)mpImage->GetData(), uiWidth, uiHeight, &uiWidth, &uiHeight);
	}
	muiFrames = uiFrames;
	mbSignal = bSignal;
	return true;
}
