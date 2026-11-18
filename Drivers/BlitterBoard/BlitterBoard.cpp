#include "KernalInterrupts.h"
#include "BlitterBoard.h"


//////////////////////////////////////////////////////////////////////////
//
// The kernal's copier's call: pvContext is the board's driver.
//
//////////////////////////////////////////////////////////////////////////
static void BlitterBoardCopy(void* pvContext, uint32 uiDest, uint32 uiSource, uint16 uiCount, uint16 uiTransparent)
{
	((CKernalBlitterBoard*)pvContext)->Copy(uiDest, uiSource, uiCount, uiTransparent);
}


//////////////////////////////////////////////////////////////////////////
//
// The board enabled, opaque, its count from 0 to 0, and the kernal's
// copies its.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalBlitterBoard::Init(SKernalDevice* psDevice, uint16 uiSlot)
{
	uint16	uiResult;

	uiResult = CKernalDriver::Init(psDevice, uiSlot);
	mpuiRegisters[BLITTER_FLAGS] = BLITTER_FLAG_ENABLE;
	mpuiRegisters[BLITTER_COUNT_START] = 0;
	mpuiRegisters[BLITTER_COUNT_START + 1] = 0;
	mpuiRegisters[BLITTER_COUNT_STOP] = 0;
	mpuiRegisters[BLITTER_COUNT_STOP + 1] = 0;
	muiTransparent = KERNAL_COPY_OPAQUE;
	muiStop = 0;
	KernalSetMainCopier(BlitterBoardCopy, this);
	return uiResult;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CKernalBlitterBoard::Kill(void)
{
	KernalSetMainCopier(NULL, NULL);
	mpuiRegisters[BLITTER_FLAGS] = 0;
	CKernalDriver::Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A process can't have the board.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalBlitterBoard::OpenUnit(uint16 uiUnit)
{
	return KERNAL_DRIVER_NOT_SUPPORTED;
}


//////////////////////////////////////////////////////////////////////////
//
// uiCount bytes of main memory from uiSource to uiDest, without those that
// are uiTransparent unless it is KERNAL_COPY_OPAQUE.  The board has main
// memory from Start until its count is the stop: nothing here touches main
// memory meanwhile, and the interrupt handlers that would are put off
// (gbKernalMainBusBusy).
//
//////////////////////////////////////////////////////////////////////////
void CKernalBlitterBoard::Copy(uint32 uiDest, uint32 uiSource, uint16 uiCount, uint16 uiTransparent)
{
	volatile uint8*	puiRegisters;
	uint8			uiLow;
	uint8			uiHigh;

	if (uiCount == 0)
	{
		return;
	}
	puiRegisters = mpuiRegisters;
	if (uiTransparent != muiTransparent)
	{
		muiTransparent = uiTransparent;
		if (uiTransparent == KERNAL_COPY_OPAQUE)
		{
			puiRegisters[BLITTER_FLAGS] = BLITTER_FLAG_ENABLE;
		}
		else
		{
			puiRegisters[BLITTER_TRANSPARENT_COLOUR] = (uint8)uiTransparent;
			puiRegisters[BLITTER_FLAGS] = BLITTER_FLAG_ENABLE | BLITTER_FLAG_TRANSPARENCY;
		}
	}
	uiLow = (uint8)uiCount;
	uiHigh = (uint8)(uiCount >> 8);
	if (uiCount != muiStop)
	{
		muiStop = uiCount;
		puiRegisters[BLITTER_COUNT_STOP] = uiLow;
		puiRegisters[BLITTER_COUNT_STOP + 1] = uiHigh;
	}
	puiRegisters[BLITTER_SOURCE] = (uint8)uiSource;
	puiRegisters[BLITTER_SOURCE + 1] = (uint8)(uiSource >> 8);
	puiRegisters[BLITTER_SOURCE + 2] = (uint8)(uiSource >> 16);
	puiRegisters[BLITTER_DEST] = (uint8)uiDest;
	puiRegisters[BLITTER_DEST + 1] = (uint8)(uiDest >> 8);
	puiRegisters[BLITTER_DEST + 2] = (uint8)(uiDest >> 16);

	gbKernalMainBusBusy = 1;
	puiRegisters[BLITTER_START] = 0;
	while ((puiRegisters[BLITTER_COUNT_START] != uiLow) || (puiRegisters[BLITTER_COUNT_START + 1] != uiHigh))
	{
	}
	gbKernalMainBusBusy = 0;
}


//The driver of the Blitter Board.
static CKernalBlitterBoard	gacBlitterBoards[BLITTER_BOARD_MAX_BOARDS];


//////////////////////////////////////////////////////////////////////////
//
// The kernal's loader calls it, once the driver is in place and its
// globals are constructed, for each board it drives: the driver of the
// uiIndex'th, NULL if there are too many.
//
//////////////////////////////////////////////////////////////////////////
extern "C" CKernalDriver* KernalCreateDriver(uint16 uiIndex)
{
	return (uiIndex < BLITTER_BOARD_MAX_BOARDS) ? &gacBlitterBoards[uiIndex] : NULL;
}
