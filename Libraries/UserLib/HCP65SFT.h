#ifndef __HCP65_SFT_H__
#define __HCP65_SFT_H__
#include "BaseLib/PrimitiveTypes.h"


//SFT images (CommonLibraries' SupportLib/SFTCommon.h writes them): what a blit draws (HCP65User.h's KernalDeviceBlit).
//A pixel is a byte; every word is low byte first.
//
//An opaque image is an SSFTImage (SFT_TYPE_OPAQUE), then its rows from the top, each its width of pixels.
//
//A transparent image is an SSFTImage (SFT_TYPE_TRANSPARENT), then bytes that say where its pixels are, starting at
//its top left:
//  SFT_RUN_END              the image is done.
//  $01-$7F                  that many pixels follow: they are drawn from here to the right.
//  SFT_RUN_SKIP + (n - 1)   n pixels (1 to SFT_RUN_MAX_COUNT) are skipped to the right.
//  SFT_RUN_ROWS + (n - 1)   n rows (1 to SFT_RUN_MAX_COUNT) down, at the row's left.
//A longer run, skip or move down is several of them.  What isn't drawn stays as it was.
//
//A container is an SSFTContainer, then its uiCount images, each after a word: how many bytes that word is before the
//next image's (0 for the last).
#define SFT_TYPE_OPAQUE			0xE457
#define SFT_TYPE_TRANSPARENT	0xE4FA
#define SFT_TYPE_CONTAINER		0xE4EF

#define SFT_RUN_END				0x00
#define SFT_RUN_MAX_PIXELS		0x7F
#define SFT_RUN_SKIP			0x80
#define SFT_RUN_ROWS			0xC0
#define SFT_RUN_COUNT_MASK		0x3F
#define SFT_RUN_MAX_COUNT		0x40


struct SSFTImage
{
	uint16	uiType;
	uint16	uiWidth;
	uint16	uiHeight;
};


struct SSFTContainer
{
	uint16	uiType;
	uint16	uiCount;
};


//The palette that shows an image's pixels as the colours they are: a pixel's red is its D0-D2, its green D3-D5 and its
//blue D6-D7.  Each of the three is 256 bytes (the Video Board's palettes: HCP65VideoBoard.h).
extern "C" void			SFTColours(uint8* puiRed, uint8* puiGreen, uint8* puiBlue);

//The uiIndex'th image (from 0) of the container at pvContainer, which is uiSize bytes, and in *puiSize how many bytes
//it can be (up to the next image, or the container's end).  NULL if it isn't a container or hasn't that many.
extern "C" const void*	SFTContainedImage(const void* pvContainer, uint32 uiSize, uint16 uiIndex, uint32* puiSize);


#endif // __HCP65_SFT_H__

