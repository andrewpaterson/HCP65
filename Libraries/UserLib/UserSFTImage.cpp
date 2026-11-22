#include <stdlib.h>
#include <string.h>
#include "HCP65User.h"
#include "HCP65SFT.h"


//////////////////////////////////////////////////////////////////////////
//
// A transparent SFT image's runs (after its header) drawn into the
// uiWidth by uiHeight pixels at puiPixels, which are 0 first.  False if a
// run isn't inside the image or the uiSize bytes end before SFT_RUN_END.
//
//////////////////////////////////////////////////////////////////////////
static bool SFTUnpackTransparent(const uint8* puiImage, uint32 uiSize, uint16 uiWidth, uint16 uiHeight, uint8* puiPixels)
{
	uint8*	puiRow;
	uint32	uiAt;
	uint16	uiX;
	uint16	uiRow;
	uint16	uiCount;
	uint8	uiCode;
	bool	bMixed;

	memset32(puiPixels, SFT_PIXEL_TRANSPARENT, (uint32)uiWidth * uiHeight);
	puiRow = puiPixels;
	uiX = 0;
	uiRow = 0;
	for (uiAt = sizeof(SSFTImage); uiAt < uiSize; )
	{
		uiCode = puiImage[uiAt++];
		if (uiCode == SFT_RUN_END)
		{
			return true;
		}
		bMixed = (uiCode >= SFT_RUN_MIXED);
		if (bMixed)
		{
			uiCount = (uint16)((uiCode & SFT_RUN_MIXED_MASK) + 1);
		}
		else if (uiCode >= SFT_RUN_ROWS)
		{
			uiCount = (uint16)((uiCode & SFT_RUN_ROWS_MASK) + 1);
			uiRow = (uint16)(uiRow + uiCount);
			puiRow += (uint32)uiCount * uiWidth;
			uiX = 0;
			continue;
		}
		else if (uiCode >= SFT_RUN_SKIP)
		{
			uiX = (uint16)(uiX + (uiCode & SFT_RUN_COUNT_MASK) + 1);
			continue;
		}
		else
		{
			uiCount = uiCode;
		}

		//A run's pixels: a mixed run's 0s are already 0.
		if ((uiRow >= uiHeight) || ((uint32)uiX + uiCount > uiWidth) || (uiSize - uiAt < uiCount))
		{
			return false;
		}
		memcpy(puiRow + uiX, &puiImage[uiAt], uiCount);
		uiAt += uiCount;
		uiX = (uint16)(uiX + uiCount);
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
extern "C" bool SFTImageUnpack(const void* pvSFT, uint32 uiSize, void* pvPixels)
{
	const SSFTImage*	psImage;
	uint32				uiBytes;

	psImage = (const SSFTImage*)pvSFT;
	if (uiSize < sizeof(SSFTImage))
	{
		return false;
	}
	uiBytes = (uint32)psImage->uiWidth * psImage->uiHeight;
	if (SFTIsRecord(pvSFT, uiSize, SFT_TYPE_OPAQUE))
	{
		if (uiSize - sizeof(SSFTImage) < uiBytes)
		{
			return false;
		}
		memcpy32(pvPixels, (const uint8*)pvSFT + sizeof(SSFTImage), uiBytes);
		return true;
	}
	if (SFTIsRecord(pvSFT, uiSize, SFT_TYPE_TRANSPARENT))
	{
		return SFTUnpackTransparent((const uint8*)pvSFT, uiSize, psImage->uiWidth, psImage->uiHeight, (uint8*)pvPixels);
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
extern "C" uint16 SFTImageMake(const void* pvSFT, uint32 uiSize, uint16 uiPalette, uint16* puiImage)
{
	const SSFTImage*	psImage;
	void*				pvPixels;
	uint16				uiResult;

	*puiImage = 0;
	psImage = (const SSFTImage*)pvSFT;
	if (!SFTIsRecord(pvSFT, uiSize, SFT_TYPE_OPAQUE) && !SFTIsRecord(pvSFT, uiSize, SFT_TYPE_TRANSPARENT))
	{
		return KERNAL_DEVICE_BAD_ARGUMENT;
	}
	if ((psImage->uiWidth == 0) || (psImage->uiHeight == 0))
	{
		return KERNAL_DEVICE_BAD_ARGUMENT;
	}
	pvPixels = malloc32((uint32)psImage->uiWidth * psImage->uiHeight);
	if (!pvPixels)
	{
		return KERNAL_DEVICE_NO_MEMORY;
	}
	if (!SFTImageUnpack(pvSFT, uiSize, pvPixels))
	{
		free(pvPixels);
		return KERNAL_DEVICE_BAD_ARGUMENT;
	}
	uiResult = KernalScreenAddImage(pvPixels, psImage->uiWidth, psImage->uiHeight, (psImage->uiType == SFT_TYPE_TRANSPARENT) ? KERNAL_IMAGE_TRANSPARENT : 0, uiPalette, puiImage);
	if (uiResult != KERNAL_DEVICE_SUCCESS)
	{
		free(pvPixels);
	}
	return uiResult;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
extern "C" uint16 SFTImageFree(uint16 uiImage)
{
	void*	pvPixels;
	uint16	uiResult;

	uiResult = KernalScreenRemoveImage(uiImage, &pvPixels);
	if (uiResult == KERNAL_DEVICE_SUCCESS)
	{
		free(pvPixels);
	}
	return uiResult;
}
