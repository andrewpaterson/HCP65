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
// are drawn, a transparent one's without its 0s (and without spans);
// scaled or mirrored, by the Graphics Board (BlitScaled).
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
	if (psBlit->uiFlags & (KERNAL_BLIT_SCALED | KERNAL_BLIT_MIRROR_X | KERNAL_BLIT_MIRROR_Y))
	{
		return BlitScaled(psBlit, psImage, aiClip);
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


//////////////////////////////////////////////////////////////////////////
//
// An image drawn at the blit's size (KERNAL_BLIT_SCALED, else its own)
// and mirrored as its flags say, by the Graphics Board: each drawn row
// that is inside the clip rectangle is one copy (two where its image row
// crosses from one of the process's banks to the next), its pixels read
// through the board's offsets from the image row nearest it.  Each drawn
// pixel is the image's nearest: pixel d of n drawn from s is
// ((2d + 1) x s) / 2n.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalVideoBoard::BlitScaled(const SKernalBlit* psBlit, const SKernalImage* psImage, const int32* paiClip)
{
	uint8*	puiDest;
	uint8*	puiBank;
	uint32	uiBankBytes;
	uint32	uiRowOffset;
	uint32	uiLastRowOffset;
	uint32	uiDenominator;
	uint32	uiRemainder;
	uint32	uiStep;
	int32	iLeft;
	int32	iRight;
	int32	iTop;
	int32	iBottom;
	uint16	uiWidth;
	uint16	uiHeight;
	uint16	uiRow;
	uint16	uiFirst;
	uint16	uiCount;
	uint16	uiTransparent;
	int32	iY;

	if (!gpuiKernalGraphics)
	{
		return KERNAL_DRIVER_NOT_FOUND;
	}
	uiWidth = (psBlit->uiFlags & KERNAL_BLIT_SCALED) ? psBlit->uiWidth : psImage->uiWidth;
	uiHeight = (psBlit->uiFlags & KERNAL_BLIT_SCALED) ? psBlit->uiHeight : psImage->uiHeight;
	if ((uiWidth == 0) || (uiHeight == 0) || (uiWidth >= KERNAL_GRAPHICS_COUNTS) || (uiHeight >= KERNAL_GRAPHICS_COUNTS))
	{
		return KERNAL_DRIVER_BAD_ARGUMENT;
	}
	iLeft = (psBlit->iX > paiClip[VIDEO_CLIP_LEFT]) ? psBlit->iX : paiClip[VIDEO_CLIP_LEFT];
	iRight = ((int32)psBlit->iX + uiWidth < paiClip[VIDEO_CLIP_RIGHT]) ? (int32)psBlit->iX + uiWidth : paiClip[VIDEO_CLIP_RIGHT];
	iTop = (psBlit->iY > paiClip[VIDEO_CLIP_TOP]) ? psBlit->iY : paiClip[VIDEO_CLIP_TOP];
	iBottom = ((int32)psBlit->iY + uiHeight < paiClip[VIDEO_CLIP_BOTTOM]) ? (int32)psBlit->iY + uiHeight : paiClip[VIDEO_CLIP_BOTTOM];
	if ((iLeft >= iRight) || (iTop >= iBottom))
	{
		return KERNAL_DRIVER_SUCCESS;
	}

	WriteOffsets(psImage->uiWidth, uiWidth, (psBlit->uiFlags & KERNAL_BLIT_MIRROR_X) != 0);
	uiTransparent = (psImage->uiFlags & KERNAL_IMAGE_TRANSPARENT) ? SFT_PIXEL_TRANSPARENT : KERNAL_COPY_OPAQUE;
	uiFirst = (uint16)(iLeft - psBlit->iX);
	uiCount = (uint16)(iRight - iLeft);

	//The first drawn row's image row, then each next one's by adding 2 x the image's height to the numerator, its
	//offset in the image stepping with it.  The image's first bank is found once: a row that is all in it is read
	//from there, and only another is found again.
	uiDenominator = 2 * (uint32)uiHeight;
	uiStep = 2 * (uint32)psImage->uiHeight;
	uiRemainder = (2 * (uint32)(iTop - psBlit->iY) + 1) * psImage->uiHeight;
	uiRow = (uint16)(uiRemainder / uiDenominator);
	uiRemainder -= (uint32)uiRow * uiDenominator;
	uiRowOffset = (uint32)uiRow * psImage->uiWidth;
	uiLastRowOffset = (uint32)(psImage->uiHeight - 1) * psImage->uiWidth;
	puiBank = KernalUserPointer(psImage->uiPixels, &uiBankBytes);
	puiDest = (uint8*)(muiWindow + (uint32)iTop * muiWidth + (uint32)iLeft);
	for (iY = iTop; iY < iBottom; iY++)
	{
		if (!BlitScaledRow(psImage, (psBlit->uiFlags & KERNAL_BLIT_MIRROR_Y) ? uiLastRowOffset - uiRowOffset : uiRowOffset, puiBank, uiBankBytes, puiDest, uiFirst, uiCount, uiWidth, (psBlit->uiFlags & KERNAL_BLIT_MIRROR_X) != 0, uiTransparent))
		{
			return KERNAL_DRIVER_BAD_ARGUMENT;
		}
		puiDest += muiWidth;
		uiRemainder += uiStep;
		while (uiRemainder >= uiDenominator)
		{
			uiRemainder -= uiDenominator;
			uiRowOffset += psImage->uiWidth;
		}
	}
	return KERNAL_DRIVER_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////
//
// The offset of drawn pixel uiDrawnX, of uiDrawn drawn from an image row
// of uiSource: the image pixel nearest it, counted from the row's right
// if bMirror.
//
//////////////////////////////////////////////////////////////////////////
static uint16 VideoScaledOffset(uint16 uiDrawnX, uint16 uiSource, uint16 uiDrawn, bool bMirror)
{
	uint16	uiX;

	uiX = (uint16)(((2 * (uint32)uiDrawnX + 1) * uiSource) / (2 * (uint32)uiDrawn));
	return bMirror ? (uint16)(uiSource - 1 - uiX) : uiX;
}


//////////////////////////////////////////////////////////////////////////
//
// Drawn pixels uiFirst to uiFirst + uiCount - 1 of a row of uiDrawn, at
// puiDest, from the image's row at uiRowOffset through the offsets: from
// puiBank (the image's first byte where it really is, with uiBankBytes of
// its bank from there) if the row is all in that bank.  A row that
// crosses into the process's next bank (which needn't be next to it in
// main memory) is two copies: the pixels whose offsets are in the first
// bank, and those in the next, whose source is put so that it plus their
// offsets is there.  False if the process hasn't the row's memory.
//
//////////////////////////////////////////////////////////////////////////
bool CKernalVideoBoard::BlitScaledRow(const SKernalImage* psImage, uint32 uiRowOffset, uint8* puiBank, uint32 uiBankBytes, uint8* puiDest, uint16 uiFirst, uint16 uiCount, uint16 uiDrawn, bool bMirror, uint16 uiTransparent)
{
	uint8*	puiSource;
	uint8*	puiNext;
	uint32	uiAddress;
	uint32	uiInBank;
	uint16	uiLow;
	uint16	uiHigh;
	uint16	uiMiddle;
	uint16	uiSplit;
	bool	bFirstNext;

	if (puiBank && (uiRowOffset + psImage->uiWidth <= uiBankBytes))
	{
		KernalMainScaledCopy(puiDest, puiBank + uiRowOffset, uiFirst, uiCount, uiTransparent);
		return true;
	}
	uiAddress = psImage->uiPixels + uiRowOffset;
	puiSource = KernalUserPointer(uiAddress, &uiInBank);
	if (!puiSource)
	{
		return false;
	}
	if (uiInBank >= psImage->uiWidth)
	{
		KernalMainScaledCopy(puiDest, puiSource, uiFirst, uiCount, uiTransparent);
		return true;
	}

	puiNext = KernalUserPointer(uiAddress + uiInBank, NULL);
	if (!puiNext)
	{
		return false;
	}
	puiNext -= uiInBank;

	//The offsets go one way across the row, so the pixels whose offsets are in the next bank are all at one end: the
	//first pixel that isn't in the same bank as uiFirst's.
	bFirstNext = VideoScaledOffset(uiFirst, psImage->uiWidth, uiDrawn, bMirror) >= uiInBank;
	uiLow = uiFirst;
	uiHigh = (uint16)(uiFirst + uiCount);
	while (uiLow < uiHigh)
	{
		uiMiddle = (uint16)(uiLow + (uiHigh - uiLow) / 2);
		if ((VideoScaledOffset(uiMiddle, psImage->uiWidth, uiDrawn, bMirror) >= uiInBank) == bFirstNext)
		{
			uiLow = (uint16)(uiMiddle + 1);
		}
		else
		{
			uiHigh = uiMiddle;
		}
	}
	uiSplit = uiLow;
	KernalMainScaledCopy(puiDest, bFirstNext ? puiNext : puiSource, uiFirst, (uint16)(uiSplit - uiFirst), uiTransparent);
	KernalMainScaledCopy(puiDest + (uiSplit - uiFirst), bFirstNext ? puiSource : puiNext, uiSplit, (uint16)(uiFirst + uiCount - uiSplit), uiTransparent);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The Graphics Board's offsets for a row of uiDrawn drawn from one of
// uiSource (VideoScaledOffset's, stepped without dividing), unless they
// are already in it.
//
//////////////////////////////////////////////////////////////////////////
void CKernalVideoBoard::WriteOffsets(uint16 uiSource, uint16 uiDrawn, bool bMirror)
{
	volatile uint8*	puiLow;
	volatile uint8*	puiHigh;
	uint32			uiDenominator;
	uint32			uiStep;
	uint32			uiRemainder;
	uint16			uiX;
	uint16			uiOffset;
	uint16			i;

	if ((mpuiOffsetsBoard == gpuiKernalGraphics) && (muiOffsetsSource == uiSource) && (muiOffsetsDrawn == uiDrawn) && (mbOffsetsMirror == bMirror))
	{
		return;
	}
	puiLow = gpuiKernalGraphics + KERNAL_GRAPHICS_OFFSETS_LOW;
	puiHigh = gpuiKernalGraphics + KERNAL_GRAPHICS_OFFSETS_HIGH;
	uiDenominator = 2 * (uint32)uiDrawn;
	uiStep = 2 * (uint32)uiSource;
	uiRemainder = uiSource;
	uiX = 0;
	while (uiRemainder >= uiDenominator)
	{
		uiRemainder -= uiDenominator;
		uiX++;
	}
	for (i = 0; i < uiDrawn; i++)
	{
		uiOffset = bMirror ? (uint16)(uiSource - 1 - uiX) : uiX;
		puiLow[i] = (uint8)uiOffset;
		puiHigh[i] = (uint8)(uiOffset >> 8);
		uiRemainder += uiStep;
		while (uiRemainder >= uiDenominator)
		{
			uiRemainder -= uiDenominator;
			uiX++;
		}
	}
	mpuiOffsetsBoard = gpuiKernalGraphics;
	muiOffsetsSource = uiSource;
	muiOffsetsDrawn = uiDrawn;
	mbOffsetsMirror = bMirror;
}
