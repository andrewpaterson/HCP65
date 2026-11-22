#include <string.h>
#include "UserLib/HCP65User.h"
#include "UserLib/HCP65VideoBoard.h"
#include "UserLib/HCP65SFT.h"
#include "KernalProcess.h"
#include "KernalConsole.h"
#include "KernalScreen.h"


#define KERNAL_SCREEN_CALL_RESULT	16		//SKernalScreenCall's uiResult.


CKernalScreen	gcKernalScreen;
SKernalImage	gasKernalScreenImages[KERNAL_SCREEN_MAX_IMAGES];


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CKernalScreen::Init(void)
{
	mpcVideo = NULL;
	muiProcess = 0;
	muiPalette = 0;
	memset(gasKernalScreenImages, 0, sizeof(gasKernalScreenImages));
}


//////////////////////////////////////////////////////////////////////////
//
// The running process has the picture, in uiMode: the Video Board is
// taken if it isn't already, and the console stops drawing.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalScreen::SetMode(uint16 uiMode)
{
	CKernalDriver*	pcVideo;
	uint16			uiResult;

	if (muiProcess && (muiProcess != KernalCurrentProcess()))
	{
		return KERNAL_DEVICE_IN_USE;
	}
	if (uiMode >= VIDEO_NUM_MODES)
	{
		return KERNAL_DEVICE_BAD_ARGUMENT;
	}

	pcVideo = mpcVideo;
	if (!pcVideo)
	{
		pcVideo = KernalFindDriver(VIDEO_BOARD_MANUFACTURER, VIDEO_BOARD_NAME, 0);
		if (!pcVideo)
		{
			return KERNAL_DEVICE_NOT_FOUND;
		}
		uiResult = pcVideo->Open(VIDEO_BOARD_PIXELS, KERNAL_CONSOLE_OWNER);
		if (uiResult != KERNAL_DRIVER_SUCCESS)
		{
			return uiResult;
		}
	}

	gcKernalConsole.SetGraphics(true);
	uiResult = pcVideo->Control(VIDEO_BOARD_PIXELS, VIDEO_BOARD_SET_MODE, uiMode, NULL);
	if (uiResult == KERNAL_DRIVER_SUCCESS)
	{
		uiResult = pcVideo->Control(VIDEO_BOARD_PIXELS, VIDEO_BOARD_SET_PALETTE, 0, NULL);
	}
	muiPalette = 0;
	mpcVideo = pcVideo;
	muiProcess = KernalCurrentProcess();
	if (uiResult != KERNAL_DRIVER_SUCCESS)
	{
		EndGraphics();
	}
	return uiResult;
}


//////////////////////////////////////////////////////////////////////////
//
// No process has the picture: the console's text is back on it, or the
// board is off if the console isn't on it; and the screen lets it go.
//
//////////////////////////////////////////////////////////////////////////
void CKernalScreen::EndGraphics(void)
{
	if (!gcKernalConsole.IsVideo())
	{
		mpcVideo->Control(VIDEO_BOARD_PIXELS, VIDEO_BOARD_OFF, 0, NULL);
	}
	mpcVideo->Close(VIDEO_BOARD_PIXELS, KERNAL_CONSOLE_OWNER);
	mpcVideo = NULL;
	muiProcess = 0;
	gcKernalConsole.SetGraphics(false);
}


//////////////////////////////////////////////////////////////////////////
//
// Palette uiPalette's colours from the running process's SFT palette
// record: its uiCount reds, then greens, then blues at its uiFirst on.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalScreen::SetColours(uint16 uiPalette, uint32 uiUserAddress)
{
	SSFTPalette	sPalette;
	uint32		uiColours;
	uint32		uiAt;
	uint32		uiDone;
	uint16		uiResult;

	if ((uiPalette >= VIDEO_BOARD_NUM_PALETTES) || !KernalCopyFromUser((uint8*)&sPalette, uiUserAddress, sizeof(SSFTPalette)) ||
		(sPalette.uiMagic != SFT_MAGIC) || (sPalette.uiVersion != SFT_VERSION) || (sPalette.uiType != SFT_TYPE_PALETTE) || (sPalette.uiCount == 0) ||
		((uint32)sPalette.uiFirst + sPalette.uiCount > SFT_PALETTE_COLOURS))
	{
		return KERNAL_DEVICE_BAD_ARGUMENT;
	}
	uiColours = uiUserAddress + sizeof(SSFTPalette);
	uiAt = (uint32)uiPalette * 256 + sPalette.uiFirst;
	uiDone = 0;
	uiResult = mpcVideo->Write(VIDEO_BOARD_PALETTES, uiAt, uiColours, sPalette.uiCount, &uiDone);
	if (uiResult == KERNAL_DRIVER_SUCCESS)
	{
		uiResult = mpcVideo->Write(VIDEO_BOARD_PALETTES, VIDEO_BOARD_GREEN + uiAt, uiColours + sPalette.uiCount, sPalette.uiCount, &uiDone);
	}
	if (uiResult == KERNAL_DRIVER_SUCCESS)
	{
		uiResult = mpcVideo->Write(VIDEO_BOARD_PALETTES, VIDEO_BOARD_BLUE + uiAt, uiColours + 2 * (uint32)sPalette.uiCount, sPalette.uiCount, &uiDone);
	}
	return uiResult;
}


//////////////////////////////////////////////////////////////////////////
//
// A handle to the running process's image of uiSize's width (its low
// word) by height at uiPixels, written to the process's uiUserResult.
// The image must be in memory the process has, every bank of it.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalScreen::AddImage(uint32 uiPixels, uint32 uiSize, uint16 uiFlags, uint16 uiPalette, uint32 uiUserResult)
{
	SKernalImage*	psImage;
	uint32			uiBytes;
	uint32			uiAt;
	uint32			uiInBank;
	uint32			uiHandle;
	uint16			uiWidth;
	uint16			uiHeight;
	uint16			i;

	uiWidth = (uint16)uiSize;
	uiHeight = (uint16)(uiSize >> 16);
	if ((uiWidth == 0) || (uiHeight == 0) || (uiFlags & ~KERNAL_IMAGE_TRANSPARENT) ||
		((uiPalette >= KERNAL_SCREEN_NUM_PALETTES) && (uiPalette != KERNAL_SCREEN_NO_PALETTE)))
	{
		return KERNAL_DEVICE_BAD_ARGUMENT;
	}
	uiBytes = (uint32)uiWidth * uiHeight;
	for (uiAt = uiPixels; uiAt - uiPixels < uiBytes; uiAt += uiInBank)
	{
		if (!KernalUserPointer(uiAt, &uiInBank))
		{
			return KERNAL_DEVICE_BAD_ARGUMENT;
		}
	}

	for (i = 0; i < KERNAL_SCREEN_MAX_IMAGES; i++)
	{
		psImage = &gasKernalScreenImages[i];
		if (psImage->uiProcess == KERNAL_DRIVER_NO_OWNER)
		{
			psImage->uiGeneration = (uint16)((psImage->uiGeneration % KERNAL_SCREEN_MAX_GENERATION) + 1);
			psImage->uiProcess = KernalCurrentProcess();
			psImage->uiPalette = (uint8)uiPalette;
			psImage->uiPixels = uiPixels;
			psImage->uiWidth = uiWidth;
			psImage->uiHeight = uiHeight;
			psImage->uiFlags = uiFlags;
			uiHandle = i | ((uint32)psImage->uiGeneration << KERNAL_SCREEN_IMAGE_BITS);
			KernalCopyToUser(uiUserResult, (uint8*)&uiHandle, sizeof(uint32));
			return KERNAL_DEVICE_SUCCESS;
		}
	}
	return KERNAL_DEVICE_NO_HANDLES;
}


//////////////////////////////////////////////////////////////////////////
//
// The running process's image whose handle is uiImage, NULL if there
// isn't one.
//
//////////////////////////////////////////////////////////////////////////
SKernalImage* CKernalScreen::FindImage(uint32 uiImage)
{
	SKernalImage*	psImage;

	if (uiImage > 0xFFFF)
	{
		return NULL;
	}
	psImage = &gasKernalScreenImages[uiImage & KERNAL_SCREEN_IMAGE_INDEX];
	if ((psImage->uiProcess == KERNAL_DRIVER_NO_OWNER) || (psImage->uiProcess != KernalCurrentProcess()) ||
		(psImage->uiGeneration != (uint16)(uiImage >> KERNAL_SCREEN_IMAGE_BITS)))
	{
		return NULL;
	}
	return psImage;
}


//////////////////////////////////////////////////////////////////////////
//
// The handle uiImage goes, and where its image is is written to the
// process's uiUserResult.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalScreen::RemoveImage(uint16 uiImage, uint32 uiUserResult)
{
	SKernalImage*	psImage;

	psImage = FindImage(uiImage);
	if (!psImage)
	{
		return KERNAL_DEVICE_BAD_HANDLE;
	}
	psImage->uiProcess = KERNAL_DRIVER_NO_OWNER;
	KernalCopyToUser(uiUserResult, (uint8*)&psImage->uiPixels, sizeof(uint32));
	return KERNAL_DEVICE_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////
//
// A blit of an image by its handle, with the image's palette if it has
// one and the palette set put back after.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalScreen::BlitImage(const SKernalBlit* psBlit)
{
	SKernalImage*	psImage;
	uint16			uiResult;
	bool			bPalette;

	psImage = FindImage(psBlit->uiImage);
	if (!psImage)
	{
		return KERNAL_DEVICE_BAD_HANDLE;
	}
	bPalette = (psImage->uiPalette != KERNAL_SCREEN_NO_PALETTE) && (psImage->uiPalette != muiPalette);
	if (bPalette)
	{
		mpcVideo->Control(VIDEO_BOARD_PIXELS, VIDEO_BOARD_SET_PALETTE, psImage->uiPalette, NULL);
	}
	uiResult = mpcVideo->BlitImage(VIDEO_BOARD_PIXELS, psBlit, psImage);
	if (bPalette)
	{
		mpcVideo->Control(VIDEO_BOARD_PIXELS, VIDEO_BOARD_SET_PALETTE, muiPalette, NULL);
	}
	return uiResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The screen call whose block is at uiAddress in the running process's
// memory.  Returns a KERNAL_DEVICE_* result (a driver's are the same).
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalScreen::Call(uint32 uiAddress)
{
	SKernalScreenCall	sCall;
	SKernalBlit			sBlit;
	uint32				uiWidth;
	uint32				uiHeight;
	uint32				uiDone;
	uint16				uiResult;

	if (!KernalCopyFromUser((uint8*)&sCall, uiAddress, sizeof(SKernalScreenCall)))
	{
		return KERNAL_DEVICE_BAD_ARGUMENT;
	}
	if (sCall.uiCall == KERNAL_SCREEN_CALL_SET_MODE)
	{
		return SetMode(sCall.uiValue);
	}

	//A process's images aren't the picture's.
	if (sCall.uiCall == KERNAL_SCREEN_CALL_ADD_IMAGE)
	{
		return AddImage(sCall.uiAddress, sCall.uiOffset, (uint16)sCall.uiLength, sCall.uiValue, uiAddress + KERNAL_SCREEN_CALL_RESULT);
	}
	if (sCall.uiCall == KERNAL_SCREEN_CALL_REMOVE_IMAGE)
	{
		return RemoveImage(sCall.uiValue, uiAddress + KERNAL_SCREEN_CALL_RESULT);
	}

	//The rest are the picture's, for the process that has it.
	if (!muiProcess || (muiProcess != KernalCurrentProcess()))
	{
		return KERNAL_DEVICE_NOT_OPEN;
	}
	uiDone = 0;
	switch (sCall.uiCall)
	{
	case KERNAL_SCREEN_CALL_SET_TEXT:
		EndGraphics();
		return KERNAL_DEVICE_SUCCESS;

	case KERNAL_SCREEN_CALL_GET_SIZE:
		mpcVideo->Control(VIDEO_BOARD_PIXELS, VIDEO_BOARD_GET_WIDTH, 0, &uiWidth);
		mpcVideo->Control(VIDEO_BOARD_PIXELS, VIDEO_BOARD_GET_HEIGHT, 0, &uiHeight);
		sCall.uiResult = uiWidth | (uiHeight << 16);
		KernalCopyToUser(uiAddress + KERNAL_SCREEN_CALL_RESULT, (uint8*)&sCall.uiResult, sizeof(uint32));
		return KERNAL_DEVICE_SUCCESS;

	case KERNAL_SCREEN_CALL_BLIT:
		if (!KernalCopyFromUser((uint8*)&sBlit, sCall.uiAddress, sizeof(SKernalBlit)))
		{
			return KERNAL_DEVICE_BAD_ARGUMENT;
		}
		if (sBlit.uiFlags & KERNAL_BLIT_IMAGE)
		{
			return BlitImage(&sBlit);
		}
		return mpcVideo->Blit(VIDEO_BOARD_PIXELS, &sBlit);

	case KERNAL_SCREEN_CALL_FLIP:
		return mpcVideo->Control(VIDEO_BOARD_PIXELS, VIDEO_BOARD_FLIP, 0, NULL);

	case KERNAL_SCREEN_CALL_WAIT_BLANK:
		return mpcVideo->Wait(VIDEO_BOARD_PIXELS, VIDEO_WAIT_VBLANK, sCall.uiValue);

	case KERNAL_SCREEN_CALL_SET_COLOURS:
		return SetColours(sCall.uiValue, sCall.uiAddress);

	case KERNAL_SCREEN_CALL_SET_PALETTE:
		uiResult = mpcVideo->Control(VIDEO_BOARD_PIXELS, VIDEO_BOARD_SET_PALETTE, sCall.uiValue, NULL);
		if (uiResult == KERNAL_DRIVER_SUCCESS)
		{
			muiPalette = (uint8)sCall.uiValue;
		}
		return uiResult;

	case KERNAL_SCREEN_CALL_WRITE:
		return mpcVideo->Write(VIDEO_BOARD_PIXELS, sCall.uiOffset, sCall.uiAddress, sCall.uiLength, &uiDone);

	case KERNAL_SCREEN_CALL_READ:
		return mpcVideo->Read(VIDEO_BOARD_PIXELS, sCall.uiOffset, sCall.uiAddress, sCall.uiLength, &uiDone);
	}
	return KERNAL_DEVICE_BAD_ARGUMENT;
}


//////////////////////////////////////////////////////////////////////////
//
// uiProcess has ended: if the picture was its, it is the console's again;
// its images' handles go.
//
//////////////////////////////////////////////////////////////////////////
void CKernalScreen::ProcessEnded(uint8 uiProcess)
{
	uint16	i;

	if (muiProcess && (muiProcess == uiProcess))
	{
		EndGraphics();
	}
	for (i = 0; i < KERNAL_SCREEN_MAX_IMAGES; i++)
	{
		if (gasKernalScreenImages[i].uiProcess == uiProcess)
		{
			gasKernalScreenImages[i].uiProcess = KERNAL_DRIVER_NO_OWNER;
		}
	}
}

