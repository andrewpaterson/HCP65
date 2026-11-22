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


extern "C"
{
	uint8*	gpuiVideoBlitCode;
	uint16	guiVideoBlitEnd;
	uint16	guiVideoBlitWidth;
	uint16	guiVideoBlitHeight;
	uint8*	gpuiVideoBlitRow;
	uint16	guiVideoBlitResult;
	uint8*	gpuiVideoSpanSource;
	uint16	guiVideoSpanSourceStride;
	uint8*	gpuiVideoSpanDest;
	uint16*	gpuiVideoSpans;
	uint16	guiVideoSpanRows;
	uint16	guiVideoSpanLeast;
	uint16	guiVideoSpanMost;
}


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
	uint16		uiResult;

	uiResult = BlitStart(uiUnit, psBlit, aiClip);
	if (uiResult != KERNAL_DRIVER_SUCCESS)
	{
		return uiResult;
	}
	if ((psBlit->uiSize < sizeof(SSFTImage)) || !KernalCopyFromUser((uint8*)&sImage, psBlit->uiImage, sizeof(SSFTImage)) ||
		(sImage.uiMagic != SFT_MAGIC) || (sImage.uiVersion != SFT_VERSION))
	{
		return KERNAL_DRIVER_BAD_ARGUMENT;
	}

	if (sImage.uiType == SFT_TYPE_OPAQUE)
	{
		if (psBlit->uiSize - sizeof(SSFTImage) < (uint32)sImage.uiWidth * sImage.uiHeight)
		{
			return KERNAL_DRIVER_BAD_ARGUMENT;
		}
		return BlitFlat(psBlit, psBlit->uiImage + sizeof(SSFTImage), sImage.uiWidth, sImage.uiHeight, aiClip, KERNAL_COPY_OPAQUE);
	}
	if (sImage.uiType == SFT_TYPE_TRANSPARENT)
	{
		return BlitTransparent(psBlit, sImage.uiWidth, sImage.uiHeight, aiClip);
	}
	return KERNAL_DRIVER_BAD_ARGUMENT;
}


//////////////////////////////////////////////////////////////////////////
//
// An image the kernal has a handle to: its rows as an opaque SFT image's
// are drawn, a transparent one's without its 0s (and without spans).
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalVideoBoard::BlitImage(uint16 uiUnit, const SKernalBlit* psBlit, const SKernalImage* psImage)
{
	int32		aiClip[4];
	uint16		uiResult;

	uiResult = BlitStart(uiUnit, psBlit, aiClip);
	if (uiResult != KERNAL_DRIVER_SUCCESS)
	{
		return uiResult;
	}
	return BlitFlat(psBlit, psImage->uiPixels, psImage->uiWidth, psImage->uiHeight, aiClip,
					(psImage->uiFlags & KERNAL_IMAGE_TRANSPARENT) ? SFT_PIXEL_TRANSPARENT : KERNAL_COPY_OPAQUE);
}


//////////////////////////////////////////////////////////////////////////
//
// Whether a blit can be drawn on uiUnit, and its clip rectangle in
// paiClip: the picture's, and with KERNAL_BLIT_CLIP the blit's inside it.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalVideoBoard::BlitStart(uint16 uiUnit, const SKernalBlit* psBlit, int32* paiClip)
{
	if (uiUnit != VIDEO_BOARD_PIXELS)
	{
		return KERNAL_DRIVER_NOT_SUPPORTED;
	}
	if (!muiWindow || (muiMode == 0xFF))
	{
		return KERNAL_DRIVER_FAILED;
	}

	paiClip[VIDEO_CLIP_LEFT] = 0;
	paiClip[VIDEO_CLIP_TOP] = 0;
	paiClip[VIDEO_CLIP_RIGHT] = muiWidth;
	paiClip[VIDEO_CLIP_BOTTOM] = muiHeight;
	if (psBlit->uiFlags & KERNAL_BLIT_CLIP)
	{
		if (psBlit->iClipLeft > paiClip[VIDEO_CLIP_LEFT])
		{
			paiClip[VIDEO_CLIP_LEFT] = psBlit->iClipLeft;
		}
		if (psBlit->iClipTop > paiClip[VIDEO_CLIP_TOP])
		{
			paiClip[VIDEO_CLIP_TOP] = psBlit->iClipTop;
		}
		if (psBlit->iClipRight < paiClip[VIDEO_CLIP_RIGHT])
		{
			paiClip[VIDEO_CLIP_RIGHT] = psBlit->iClipRight;
		}
		if (psBlit->iClipBottom < paiClip[VIDEO_CLIP_BOTTOM])
		{
			paiClip[VIDEO_CLIP_BOTTOM] = psBlit->iClipBottom;
		}
	}
	return KERNAL_DRIVER_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////
//
// A flat image (an opaque SFT image's rows, or an image the kernal has a
// handle to) of uiWidth by uiHeight whose first pixel is the process's
// uiPixels: the part of each of its rows that is inside the clip
// rectangle, without those that are uiTransparent unless it is
// KERNAL_COPY_OPAQUE.  Spans are only for an opaque one.  A row that
// crosses from one of the process's banks to the next (which needn't be
// next to it in main memory) is a copy from each.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalVideoBoard::BlitFlat(const SKernalBlit* psBlit, uint32 uiPixels, uint16 uiWidth, uint16 uiHeight, const int32* paiClip, uint16 uiTransparent)
{
	uint16*	puiSpans;
	uint16	auiSpan[2];
	uint32	uiSpansInBank;
	uint32	uiSpanRows;
	int32	iSpanLeft;
	int32	iSpanRight;
	int32	iLeft;
	int32	iRight;
	int32	iY;
	uint32	uiSource;
	uint32	uiPixel;
	uint16	uiRow;
	uint16	uiCount;
	uint8*	puiSource;
	uint8*	puiDest;
	uint32	uiInBank;

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
	uiSource = uiPixels + (uint32)uiRow * uiWidth + (uint32)(iLeft - psBlit->iX);
	uiPixel = (uint32)iY * muiWidth + (uint32)iLeft;

	//With spans, each row only from its span's left to its right (inside the clip rectangle's, so inside the picture).
	//They are read straight if they are all in one bank of the process's.
	puiSpans = NULL;
	uiSpanRows = 0;
	if (((psBlit->uiFlags & (KERNAL_BLIT_CLIP | KERNAL_BLIT_SPANS)) == (KERNAL_BLIT_CLIP | KERNAL_BLIT_SPANS)) && (uiTransparent == KERNAL_COPY_OPAQUE))
	{
		if (psBlit->iClipBottom <= psBlit->iClipTop)
		{
			return KERNAL_DRIVER_SUCCESS;
		}
		uiSpanRows = (uint32)(psBlit->iClipBottom - psBlit->iClipTop);
		puiSpans = (uint16*)KernalUserPointer(psBlit->uiSpans, &uiSpansInBank);
		if (puiSpans && (uiSpansInBank < uiSpanRows * 2 * sizeof(uint16)))
		{
			puiSpans = NULL;
		}
		if (!puiSpans && !KernalCopyFromUser((uint8*)auiSpan, psBlit->uiSpans, 2 * sizeof(uint16)))
		{
			return KERNAL_DRIVER_BAD_ARGUMENT;
		}
	}

	//A row is copied straight from the kernal's pointer to it while it is all in the bank the pointer is in, and the
	//pointer is found again only when a row isn't.
	uiCount = (uint16)(iRight - iLeft);
	puiDest = (uint8*)(muiWindow + uiPixel);
	if (puiSpans && !gpuiKernalBlitter && (iLeft == psBlit->iClipLeft))
	{
		return BlitOpaqueSpans(psBlit, uiWidth, uiHeight, iRight, iY, uiRow, uiSource, puiDest, puiSpans,
							   ((int32)psBlit->iY + uiHeight < paiClip[VIDEO_CLIP_BOTTOM]) ? (int32)psBlit->iY + uiHeight : paiClip[VIDEO_CLIP_BOTTOM]);
	}
	puiSource = KernalUserPointer(uiSource, &uiInBank);
	for (; (uiRow < uiHeight) && (iY < paiClip[VIDEO_CLIP_BOTTOM]); uiRow++, iY++)
	{
		if (uiSpanRows > 0)
		{
			if ((uint32)(iY - psBlit->iClipTop) < uiSpanRows)
			{
				if (puiSpans)
				{
					auiSpan[0] = puiSpans[(iY - psBlit->iClipTop) * 2];
					auiSpan[1] = puiSpans[(iY - psBlit->iClipTop) * 2 + 1];
				}
				else if (!KernalCopyFromUser((uint8*)auiSpan, psBlit->uiSpans + (uint32)(iY - psBlit->iClipTop) * 2 * sizeof(uint16), 2 * sizeof(uint16)))
				{
					return KERNAL_DRIVER_BAD_ARGUMENT;
				}
				iSpanLeft = (int32)psBlit->iClipLeft + auiSpan[0];
				iSpanRight = (int32)psBlit->iClipLeft + auiSpan[1];
				if (iSpanLeft < iLeft)
				{
					iSpanLeft = iLeft;
				}
				if (iSpanRight > iRight)
				{
					iSpanRight = iRight;
				}
				if (iSpanLeft < iSpanRight)
				{
					if (puiSource && (uiInBank >= (uint32)(iSpanRight - iLeft)))
					{
						KernalMainCopy(puiDest + (iSpanLeft - iLeft), puiSource + (iSpanLeft - iLeft), (uint16)(iSpanRight - iSpanLeft), uiTransparent);
					}
					else if (!BlitPixels(uiPixel + (uint32)(iSpanLeft - iLeft), uiSource + (uint32)(iSpanLeft - iLeft), (uint32)(iSpanRight - iSpanLeft), uiTransparent))
					{
						return KERNAL_DRIVER_BAD_ARGUMENT;
					}
				}
			}
		}
		else if (puiSource && (uiInBank >= uiCount))
		{
			KernalMainCopy(puiDest, puiSource, uiCount, uiTransparent);
		}
		else if (!BlitPixels(uiPixel, uiSource, uiCount, uiTransparent))
		{
			return KERNAL_DRIVER_BAD_ARGUMENT;
		}
		uiSource += uiWidth;
		uiPixel += muiWidth;
		puiDest += muiWidth;
		if (puiSource && (uiInBank > uiWidth))
		{
			puiSource += uiWidth;
			uiInBank -= uiWidth;
		}
		else
		{
			puiSource = KernalUserPointer(uiSource, &uiInBank);
		}
	}
	return KERNAL_DRIVER_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////
//
// BlitFlat's with spans that are all in one bank (puiSpans, the
// kernal's pointer to them), no Blitter Board and the clip rectangle's
// left inside the image: from screen row iY (the image's uiRow, whose
// pixel at the clip's left is the process's uiSource and goes to puiDest)
// to iBottom.  The rows are drawn by VideoBlitSpans a bank of the image's
// at a time, each its span cut to the image's and the clip's right
// (iRight).  A row that runs past its bank is drawn here.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalVideoBoard::BlitOpaqueSpans(const SKernalBlit* psBlit, uint16 uiWidth, uint16 uiHeight, int32 iRight, int32 iY, uint16 uiRow, uint32 uiSource, uint8* puiDest, uint16* puiSpans, int32 iBottom)
{
	uint8*	puiSource;
	uint32	uiInBank;
	uint32	uiRows;
	uint32	uiLeft;
	uint16	uiMost;
	uint16*	puiSpan;

	uiMost = (uint16)(iRight - psBlit->iClipLeft);
	puiSpan = puiSpans + (iY - psBlit->iClipTop) * 2;
	while (iY < iBottom)
	{
		puiSource = KernalUserPointer(uiSource, &uiInBank);
		if (!puiSource)
		{
			return KERNAL_DRIVER_BAD_ARGUMENT;
		}
		if (uiInBank >= uiMost)
		{
			//Every row that is all in this bank.
			uiRows = (uiInBank - uiMost) / uiWidth + 1;
			uiLeft = (uint32)(iBottom - iY);
			if (uiRows > uiLeft)
			{
				uiRows = uiLeft;
			}
			gpuiVideoSpanSource = puiSource;
			guiVideoSpanSourceStride = uiWidth;
			gpuiVideoSpanDest = puiDest;
			gpuiVideoSpans = puiSpan;
			guiVideoSpanRows = (uint16)uiRows;
			guiVideoSpanLeast = 0;
			guiVideoSpanMost = uiMost;
			VideoBlitSpans();
		}
		else
		{
			//A row past the bank's end, a copy for each bank its span is in.
			uiRows = 1;
			if ((puiSpan[1] > puiSpan[0]) && (puiSpan[0] < uiMost))
			{
				if (!BlitPixels((uint32)(puiDest - (uint8*)muiWindow) + puiSpan[0], uiSource + puiSpan[0], (uint32)(((puiSpan[1] < uiMost) ? puiSpan[1] : uiMost) - puiSpan[0]), KERNAL_COPY_OPAQUE))
				{
					return KERNAL_DRIVER_BAD_ARGUMENT;
				}
			}
		}
		iY += (int32)uiRows;
		uiSource += uiRows * uiWidth;
		puiDest += uiRows * muiWidth;
		puiSpan += uiRows * 2;
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
	uint32	uiInBank;

	//All of it in one bank of the process's and all of it inside the clip rectangle: read straight from the kernal's
	//pointer to it, with nothing to cut off.
	if ((psBlit->iX >= paiClip[VIDEO_CLIP_LEFT]) && ((int32)psBlit->iX + uiWidth <= paiClip[VIDEO_CLIP_RIGHT]) &&
		(psBlit->iY >= paiClip[VIDEO_CLIP_TOP]) && ((int32)psBlit->iY + uiHeight <= paiClip[VIDEO_CLIP_BOTTOM]))
	{
		puiCode = KernalUserPointer(psBlit->uiImage, &uiInBank);
		if (puiCode && (uiInBank >= psBlit->uiSize))
		{
			return BlitTransparentWhole(psBlit, puiCode, uiWidth, uiHeight);
		}
	}

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
// BlitTransparent's for an image that is all in one bank, at puiImage
// (the kernal's pointer to it), and all inside the clip rectangle: its
// codes read straight, each run one copy to its row.  The same checks:
// a run that isn't in the image is KERNAL_DRIVER_BAD_ARGUMENT.  Without a
// Blitter Board (whose copies are quicker for the longer runs) and with
// the image's bytes ending before its bank does, VideoBlitRuns draws it.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalVideoBoard::BlitTransparentWhole(const SKernalBlit* psBlit, const uint8* puiImage, uint16 uiWidth, uint16 uiHeight)
{
	const uint8*	puiCode;
	const uint8*	puiEnd;
	uint8*			puiRow;
	uint8			uiCode;
	uint16			uiCount;
	uint16			uiX;
	uint16			uiRow;

	puiCode = puiImage + sizeof(SSFTImage);
	puiEnd = puiImage + psBlit->uiSize;
	puiRow = (uint8*)(muiWindow + (uint32)psBlit->iY * muiWidth + (uint32)psBlit->iX);
	if (!gpuiKernalBlitter && (((uint32)puiImage & 0xFFFF) + psBlit->uiSize < 0x10000))
	{
		gpuiVideoBlitCode = (uint8*)puiCode;
		guiVideoBlitEnd = (uint16)((uint32)puiEnd & 0xFFFF);
		guiVideoBlitWidth = uiWidth;
		guiVideoBlitHeight = uiHeight;
		gpuiVideoBlitRow = puiRow;
		VideoBlitRuns();
		return (guiVideoBlitResult == 0) ? KERNAL_DRIVER_SUCCESS : KERNAL_DRIVER_BAD_ARGUMENT;
	}
	uiX = 0;
	uiRow = 0;
	while (puiCode < puiEnd)
	{
		uiCode = *puiCode++;
		if (uiCode == SFT_RUN_END)
		{
			return KERNAL_DRIVER_SUCCESS;
		}
		if (uiCode >= SFT_RUN_MIXED)
		{
			uiCount = (uint16)((uiCode & SFT_RUN_MIXED_MASK) + 1);
		}
		else if (uiCode >= SFT_RUN_ROWS)
		{
			uiCount = (uint16)((uiCode & SFT_RUN_ROWS_MASK) + 1);
			uiRow = (uint16)(uiRow + uiCount);
			uiX = 0;
			for (; uiCount > 0; uiCount--)
			{
				puiRow += muiWidth;
			}
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

		//A run of uiCount pixels, all of them drawn or (a mixed run) those that aren't SFT_PIXEL_TRANSPARENT.
		if ((uiRow >= uiHeight) || (uiX + uiCount > uiWidth) || ((uint32)(puiEnd - puiCode) < uiCount))
		{
			return KERNAL_DRIVER_BAD_ARGUMENT;
		}
		KernalMainCopy(puiRow + uiX, puiCode, uiCount, (uiCode >= SFT_RUN_MIXED) ? SFT_PIXEL_TRANSPARENT : KERNAL_COPY_OPAQUE);
		puiCode += uiCount;
		uiX = (uint16)(uiX + uiCount);
	}
	return KERNAL_DRIVER_BAD_ARGUMENT;
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
