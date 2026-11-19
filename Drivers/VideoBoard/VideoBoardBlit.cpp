#include <string.h>
#include "KernalProcess.h"
#include "UserLib/HCP65SFT.h"
#include "VideoBoard.h"


//The clip rectangle: what is drawn is from its left and top to just before its right and bottom.
#define VIDEO_CLIP_LEFT		0
#define VIDEO_CLIP_TOP		1
#define VIDEO_CLIP_RIGHT	2
#define VIDEO_CLIP_BOTTOM	3

#define VIDEO_BLIT_MOST		0x8000		//The most bytes in one of a blit's copies.


//////////////////////////////////////////////////////////////////////////
//
// The blit: an SFT image in the running process's memory drawn on the
// frame being drawn, where the mode's picture and the blit's clip
// rectangle have it.  Each pixel written gets the palette number set
// (VIDEO_BOARD_SET_PALETTE), as any pixel write does.  The program flips
// the frames to show it.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalVideoBoard::Blit(uint16 uiUnit, const SKernalBlit* psBlit)
{
	SSFTImage	sImage;
	int32		aiClip[4];

	if (uiUnit != VIDEO_BOARD_PIXELS)
	{
		return KERNAL_DRIVER_NOT_SUPPORTED;
	}
	if (!muiWindow || (muiMode == 0xFF))
	{
		return KERNAL_DRIVER_FAILED;
	}
	if ((psBlit->uiSize < sizeof(SSFTImage)) || !KernalCopyFromUser((uint8*)&sImage, psBlit->uiImage, sizeof(SSFTImage)))
	{
		return KERNAL_DRIVER_BAD_ARGUMENT;
	}

	aiClip[VIDEO_CLIP_LEFT] = 0;
	aiClip[VIDEO_CLIP_TOP] = 0;
	aiClip[VIDEO_CLIP_RIGHT] = muiWidth;
	aiClip[VIDEO_CLIP_BOTTOM] = muiHeight;
	if (psBlit->uiFlags & KERNAL_BLIT_CLIP)
	{
		if (psBlit->iClipLeft > aiClip[VIDEO_CLIP_LEFT])
		{
			aiClip[VIDEO_CLIP_LEFT] = psBlit->iClipLeft;
		}
		if (psBlit->iClipTop > aiClip[VIDEO_CLIP_TOP])
		{
			aiClip[VIDEO_CLIP_TOP] = psBlit->iClipTop;
		}
		if (psBlit->iClipRight < aiClip[VIDEO_CLIP_RIGHT])
		{
			aiClip[VIDEO_CLIP_RIGHT] = psBlit->iClipRight;
		}
		if (psBlit->iClipBottom < aiClip[VIDEO_CLIP_BOTTOM])
		{
			aiClip[VIDEO_CLIP_BOTTOM] = psBlit->iClipBottom;
		}
	}

	if (sImage.uiType == SFT_TYPE_OPAQUE)
	{
		return BlitOpaque(psBlit, sImage.uiWidth, sImage.uiHeight, aiClip);
	}
	if (sImage.uiType == SFT_TYPE_TRANSPARENT)
	{
		return BlitTransparent(psBlit, sImage.uiWidth, sImage.uiHeight, aiClip);
	}
	return KERNAL_DRIVER_BAD_ARGUMENT;
}


//////////////////////////////////////////////////////////////////////////
//
// An opaque image: the part of each of its rows that is inside the clip
// rectangle.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalVideoBoard::BlitOpaque(const SKernalBlit* psBlit, uint16 uiWidth, uint16 uiHeight, const int32* paiClip)
{
	int32	iLeft;
	int32	iRight;
	int32	iY;
	uint32	uiSource;
	uint32	uiPixel;
	uint16	uiRow;

	if (psBlit->uiSize - sizeof(SSFTImage) < (uint32)uiWidth * uiHeight)
	{
		return KERNAL_DRIVER_BAD_ARGUMENT;
	}
	iLeft = (psBlit->iX > paiClip[VIDEO_CLIP_LEFT]) ? psBlit->iX : paiClip[VIDEO_CLIP_LEFT];
	iRight = ((int32)psBlit->iX + uiWidth < paiClip[VIDEO_CLIP_RIGHT]) ? (int32)psBlit->iX + uiWidth : paiClip[VIDEO_CLIP_RIGHT];
	if (iLeft >= iRight)
	{
		return KERNAL_DRIVER_SUCCESS;
	}

	//The first row inside the rectangle, then each after it a row's pixels on.
	iY = psBlit->iY;
	uiRow = 0;
	if (iY < paiClip[VIDEO_CLIP_TOP])
	{
		if (paiClip[VIDEO_CLIP_TOP] - iY >= (int32)uiHeight)
		{
			return KERNAL_DRIVER_SUCCESS;
		}
		uiRow = (uint16)(paiClip[VIDEO_CLIP_TOP] - iY);
		iY = paiClip[VIDEO_CLIP_TOP];
	}
	uiSource = psBlit->uiImage + sizeof(SSFTImage) + (uint32)uiRow * uiWidth + (uint32)(iLeft - psBlit->iX);
	uiPixel = (uint32)iY * muiWidth + (uint32)iLeft;
	for (; (uiRow < uiHeight) && (iY < paiClip[VIDEO_CLIP_BOTTOM]); uiRow++, iY++)
	{
		if (!BlitPixels(uiPixel, uiSource, (uint32)(iRight - iLeft), KERNAL_COPY_OPAQUE))
		{
			return KERNAL_DRIVER_BAD_ARGUMENT;
		}
		uiSource += uiWidth;
		uiPixel += muiWidth;
	}
	return KERNAL_DRIVER_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////
//
// A transparent image: the part of each of its runs that is inside the
// clip rectangle (of a mixed run, the pixels that aren't transparent).  A run that isn't in the image (past its right, its
// last row or its size) is KERNAL_DRIVER_BAD_ARGUMENT.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalVideoBoard::BlitTransparent(const SKernalBlit* psBlit, uint16 uiWidth, uint16 uiHeight, const int32* paiClip)
{
	uint8*	puiCode;
	uint8	uiCode;
	uint16	uiCount;
	bool	bMixed;
	uint32	uiOffset;
	uint32	uiX;
	uint32	uiRow;
	int32	iY;
	int32	iRowPixel;
	int32	iLeft;
	int32	iRight;

	uiOffset = sizeof(SSFTImage);
	uiX = 0;
	uiRow = 0;
	iY = psBlit->iY;
	iRowPixel = iY * (int32)muiWidth;
	for (;;)
	{
		puiCode = (uiOffset < psBlit->uiSize) ? KernalUserPointer(psBlit->uiImage + uiOffset, NULL) : NULL;
		if (!puiCode)
		{
			return KERNAL_DRIVER_BAD_ARGUMENT;
		}
		uiCode = *puiCode;
		uiOffset++;
		if (uiCode == SFT_RUN_END)
		{
			return KERNAL_DRIVER_SUCCESS;
		}

		bMixed = (uiCode >= SFT_RUN_MIXED);
		if (bMixed)
		{
			uiCode = (uint8)((uiCode & SFT_RUN_MIXED_MASK) + 1);
		}
		else if (uiCode >= SFT_RUN_SKIP)
		{
			uiCount = (uint16)((uiCode & SFT_RUN_COUNT_MASK) + 1);
			if (uiCode >= SFT_RUN_ROWS)
			{
				uiCount = (uint16)((uiCode & SFT_RUN_ROWS_MASK) + 1);
				uiRow += uiCount;
				iY += uiCount;
				uiX = 0;
				for (; uiCount > 0; uiCount--)
				{
					iRowPixel += muiWidth;
				}
			}
			else
			{
				uiX += uiCount;
			}
			continue;
		}

		//A run of uiCode pixels, all of them drawn or (a mixed run) those that aren't SFT_PIXEL_TRANSPARENT.
		if ((uiRow >= uiHeight) || (uiX + uiCode > uiWidth) || (psBlit->uiSize - uiOffset < uiCode))
		{
			return KERNAL_DRIVER_BAD_ARGUMENT;
		}
		if ((iY >= paiClip[VIDEO_CLIP_TOP]) && (iY < paiClip[VIDEO_CLIP_BOTTOM]))
		{
			iLeft = psBlit->iX + (int32)uiX;
			iRight = iLeft + uiCode;
			if (iRight > paiClip[VIDEO_CLIP_RIGHT])
			{
				iRight = paiClip[VIDEO_CLIP_RIGHT];
			}
			if (iLeft < paiClip[VIDEO_CLIP_LEFT])
			{
				iLeft = paiClip[VIDEO_CLIP_LEFT];
			}
			if (iLeft < iRight)
			{
				if (!BlitPixels((uint32)(iRowPixel + iLeft), psBlit->uiImage + uiOffset + (uint32)(iLeft - psBlit->iX - (int32)uiX), (uint32)(iRight - iLeft),
						bMixed ? SFT_PIXEL_TRANSPARENT : KERNAL_COPY_OPAQUE))
				{
					return KERNAL_DRIVER_BAD_ARGUMENT;
				}
			}
		}
		uiOffset += uiCode;
		uiX += uiCode;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// uiLength of the running process's bytes at uiUserAddress to the frame
// being drawn's pixels from uiPixel (which the caller has inside the
// picture), without those that are uiTransparent unless it is
// KERNAL_COPY_OPAQUE.  They are copies in main memory (KernalMainCopy: the
// Blitter Board's if there is one), one for each bank of the process's
// that the bytes are in.  False if the process hasn't that memory.
//
//////////////////////////////////////////////////////////////////////////
bool CKernalVideoBoard::BlitPixels(uint32 uiPixel, uint32 uiUserAddress, uint32 uiLength, uint16 uiTransparent)
{
	uint8*	puiSource;
	uint32	uiInBank;

	while (uiLength > 0)
	{
		puiSource = KernalUserPointer(uiUserAddress, &uiInBank);
		if (!puiSource)
		{
			return false;
		}
		uiInBank = (uiInBank < uiLength) ? uiInBank : uiLength;
		if (uiInBank > VIDEO_BLIT_MOST)
		{
			uiInBank = VIDEO_BLIT_MOST;
		}
		KernalMainCopy((uint8*)(muiWindow + uiPixel), puiSource, (uint16)uiInBank, uiTransparent);
		uiUserAddress += uiInBank;
		uiPixel += uiInBank;
		uiLength -= uiInBank;
	}
	return true;
}
