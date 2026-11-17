#include "HCP65SFT.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
extern "C" const void* SFTContainedImage(const void* pvContainer, uint32 uiSize, uint16 uiIndex, uint32* puiSize)
{
	const uint8*			puiContainer;
	const SSFTContainer*	psContainer;
	uint32					uiPosition;
	uint16					uiNext;
	uint16					i;

	puiContainer = (const uint8*)pvContainer;
	psContainer = (const SSFTContainer*)pvContainer;
	if ((uiSize < sizeof(SSFTContainer)) || (psContainer->uiType != SFT_TYPE_CONTAINER) || (uiIndex >= psContainer->uiCount))
	{
		return NULL;
	}

	uiPosition = sizeof(SSFTContainer);
	for (i = 0; ; i++)
	{
		if (uiPosition + sizeof(uint16) > uiSize)
		{
			return NULL;
		}
		uiNext = *((const uint16*)&puiContainer[uiPosition]);
		if (i == uiIndex)
		{
			break;
		}
		if (uiNext == 0)
		{
			return NULL;
		}
		uiPosition += uiNext;
	}
	*puiSize = ((uiNext != 0) && (uiPosition + uiNext <= uiSize)) ? (uint32)(uiNext - sizeof(uint16)) : (uiSize - uiPosition - sizeof(uint16));
	return &puiContainer[uiPosition + sizeof(uint16)];
}


//////////////////////////////////////////////////////////////////////////
//
// Three bits and two bits repeated to fill eight, as SupportLib's
// SFTReader makes them.
//
//////////////////////////////////////////////////////////////////////////
extern "C" void SFTColours(uint8* puiRed, uint8* puiGreen, uint8* puiBlue)
{
	uint16	i;
	uint8	uiRed;
	uint8	uiGreen;
	uint8	uiBlue;

	for (i = 0; i < 256; i++)
	{
		uiRed = (uint8)(i & 7);
		uiGreen = (uint8)((i >> 3) & 7);
		uiBlue = (uint8)((i >> 6) & 3);
		puiRed[i] = (uint8)((uiRed << 5) | (uiRed << 2) | (uiRed >> 1));
		puiGreen[i] = (uint8)((uiGreen << 5) | (uiGreen << 2) | (uiGreen >> 1));
		puiBlue[i] = (uint8)((uiBlue << 6) | (uiBlue << 4) | (uiBlue << 2) | uiBlue);
	}
}

