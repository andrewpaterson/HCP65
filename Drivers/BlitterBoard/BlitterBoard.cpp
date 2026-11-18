#include "BlitterBoard.h"


//////////////////////////////////////////////////////////////////////////
//
// The board enabled, its count's start 0 (where it stays), and its
// registers the kernal's to give copies to.
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
	gpuiKernalBlitter = mpuiRegisters;
	return uiResult;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CKernalBlitterBoard::Kill(void)
{
	gpuiKernalBlitter = NULL;
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
