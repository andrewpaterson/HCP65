#include "UserLib/HCP65User.h"
#include "UserLib/HCP65VideoBoard.h"
#include "KernalProcess.h"
#include "KernalConsole.h"
#include "KernalScreen.h"


#define KERNAL_SCREEN_CALL_RESULT	16		//SKernalScreenCall's uiResult.


CKernalScreen	gcKernalScreen;


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CKernalScreen::Init(void)
{
	mpcVideo = NULL;
	muiProcess = 0;
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
// Palette uiPalette's colours from the running process's 256 reds, then
// greens, then blues.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalScreen::SetColours(uint16 uiPalette, uint32 uiUserAddress)
{
	uint32	uiDone;
	uint16	uiResult;

	if (uiPalette >= VIDEO_BOARD_NUM_PALETTES)
	{
		return KERNAL_DEVICE_BAD_ARGUMENT;
	}
	uiDone = 0;
	uiResult = mpcVideo->Write(VIDEO_BOARD_PALETTES, (uint32)uiPalette * 256, uiUserAddress, 256, &uiDone);
	if (uiResult == KERNAL_DRIVER_SUCCESS)
	{
		uiResult = mpcVideo->Write(VIDEO_BOARD_PALETTES, VIDEO_BOARD_GREEN + (uint32)uiPalette * 256, uiUserAddress + 256, 256, &uiDone);
	}
	if (uiResult == KERNAL_DRIVER_SUCCESS)
	{
		uiResult = mpcVideo->Write(VIDEO_BOARD_PALETTES, VIDEO_BOARD_BLUE + (uint32)uiPalette * 256, uiUserAddress + 512, 256, &uiDone);
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

	if (!KernalCopyFromUser((uint8*)&sCall, uiAddress, sizeof(SKernalScreenCall)))
	{
		return KERNAL_DEVICE_BAD_ARGUMENT;
	}
	if (sCall.uiCall == KERNAL_SCREEN_CALL_SET_MODE)
	{
		return SetMode(sCall.uiValue);
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
		return mpcVideo->Blit(VIDEO_BOARD_PIXELS, &sBlit);

	case KERNAL_SCREEN_CALL_FLIP:
		return mpcVideo->Control(VIDEO_BOARD_PIXELS, VIDEO_BOARD_FLIP, 0, NULL);

	case KERNAL_SCREEN_CALL_WAIT_BLANK:
		return mpcVideo->Wait(VIDEO_BOARD_PIXELS, VIDEO_WAIT_VBLANK, sCall.uiValue);

	case KERNAL_SCREEN_CALL_SET_COLOURS:
		return SetColours(sCall.uiValue, sCall.uiAddress);

	case KERNAL_SCREEN_CALL_SET_PALETTE:
		return mpcVideo->Control(VIDEO_BOARD_PIXELS, VIDEO_BOARD_SET_PALETTE, sCall.uiValue, NULL);

	case KERNAL_SCREEN_CALL_WRITE:
		return mpcVideo->Write(VIDEO_BOARD_PIXELS, sCall.uiOffset, sCall.uiAddress, sCall.uiLength, &uiDone);

	case KERNAL_SCREEN_CALL_READ:
		return mpcVideo->Read(VIDEO_BOARD_PIXELS, sCall.uiOffset, sCall.uiAddress, sCall.uiLength, &uiDone);
	}
	return KERNAL_DEVICE_BAD_ARGUMENT;
}


//////////////////////////////////////////////////////////////////////////
//
// uiProcess has ended: if the picture was its, it is the console's again.
//
//////////////////////////////////////////////////////////////////////////
void CKernalScreen::ProcessEnded(uint8 uiProcess)
{
	if (muiProcess && (muiProcess == uiProcess))
	{
		EndGraphics();
	}
}

