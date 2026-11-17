#include <string.h>
#include "HCP65User.h"


//////////////////////////////////////////////////////////////////////////
//
// The screen call uiCall with uiValue, the program's memory at pvAddress
// and, for pixels, uiOffset and uiLength.
//
//////////////////////////////////////////////////////////////////////////
static uint16 UserScreenCall(uint16 uiCall, uint16 uiValue, const void* pvAddress, uint32 uiOffset, uint32 uiLength, uint32* puiResult)
{
	SKernalScreenCall	sCall;
	uint16				uiResult;

	sCall.uiCall = uiCall;
	sCall.uiValue = uiValue;
	sCall.uiOffset = uiOffset;
	sCall.uiAddress = (uint32)pvAddress;
	sCall.uiLength = uiLength;
	sCall.uiResult = 0;
	uiResult = KernalScreenCall(&sCall);
	if (puiResult)
	{
		*puiResult = sCall.uiResult;
	}
	return uiResult;
}


extern "C" uint16 KernalScreenSetMode(uint16 uiMode)
{
	return UserScreenCall(KERNAL_SCREEN_CALL_SET_MODE, uiMode, NULL, 0, 0, NULL);
}


extern "C" uint16 KernalScreenSetText(void)
{
	return UserScreenCall(KERNAL_SCREEN_CALL_SET_TEXT, 0, NULL, 0, 0, NULL);
}


extern "C" uint16 KernalScreenGetSize(uint16* puiWidth, uint16* puiHeight)
{
	uint32	uiSize;
	uint16	uiResult;

	uiResult = UserScreenCall(KERNAL_SCREEN_CALL_GET_SIZE, 0, NULL, 0, 0, &uiSize);
	*puiWidth = (uint16)uiSize;
	*puiHeight = (uint16)(uiSize >> 16);
	return uiResult;
}


extern "C" uint16 KernalScreenBlit(const SKernalBlit* psBlit)
{
	return UserScreenCall(KERNAL_SCREEN_CALL_BLIT, 0, psBlit, 0, 0, NULL);
}


extern "C" uint16 KernalScreenFlip(void)
{
	return UserScreenCall(KERNAL_SCREEN_CALL_FLIP, 0, NULL, 0, 0, NULL);
}


extern "C" uint16 KernalScreenWaitBlank(uint16 uiTicks)
{
	return UserScreenCall(KERNAL_SCREEN_CALL_WAIT_BLANK, uiTicks, NULL, 0, 0, NULL);
}


extern "C" uint16 KernalScreenSetColours(uint16 uiPalette, const uint8* puiColours)
{
	return UserScreenCall(KERNAL_SCREEN_CALL_SET_COLOURS, uiPalette, puiColours, 0, 0, NULL);
}


extern "C" uint16 KernalScreenSetPalette(uint16 uiPalette)
{
	return UserScreenCall(KERNAL_SCREEN_CALL_SET_PALETTE, uiPalette, NULL, 0, 0, NULL);
}


extern "C" uint16 KernalScreenWrite(uint32 uiOffset, const void* pvSource, uint32 uiLength)
{
	return UserScreenCall(KERNAL_SCREEN_CALL_WRITE, 0, pvSource, uiOffset, uiLength, NULL);
}


extern "C" uint16 KernalScreenRead(uint32 uiOffset, void* pvDest, uint32 uiLength)
{
	return UserScreenCall(KERNAL_SCREEN_CALL_READ, 0, pvDest, uiOffset, uiLength, NULL);
}

