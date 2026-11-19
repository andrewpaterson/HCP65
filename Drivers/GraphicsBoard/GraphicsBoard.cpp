#include "GraphicsBoard.h"


//////////////////////////////////////////////////////////////////////////
//
// The board enabled, its count's start and stop 0, and its registers the
// kernal's to give copies to.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalGraphicsBoard::Init(SKernalDevice* psDevice, uint16 uiSlot)
{
	uint16	uiResult;

	uiResult = CKernalDriver::Init(psDevice, uiSlot);
	mpuiRegisters[GRAPHICS_FLAGS] = GRAPHICS_FLAG_ENABLE;
	mpuiRegisters[GRAPHICS_COUNT_START] = 0;
	mpuiRegisters[GRAPHICS_COUNT_START + 1] = 0;
	mpuiRegisters[GRAPHICS_COUNT_STOP] = 0;
	mpuiRegisters[GRAPHICS_COUNT_STOP + 1] = 0;
	gpuiKernalGraphics = mpuiRegisters;
	return uiResult;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CKernalGraphicsBoard::Kill(void)
{
	gpuiKernalGraphics = NULL;
	mpuiRegisters[GRAPHICS_FLAGS] = 0;
	CKernalDriver::Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A process can't have the board.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalGraphicsBoard::OpenUnit(uint16 uiUnit)
{
	return KERNAL_DRIVER_NOT_SUPPORTED;
}


//The driver of the Graphics Board.
static CKernalGraphicsBoard	gacGraphicsBoards[GRAPHICS_BOARD_MAX_BOARDS];


//////////////////////////////////////////////////////////////////////////
//
// The kernal's loader calls it, once the driver is in place and its
// globals are constructed, for each board it drives: the driver of the
// uiIndex'th, NULL if there are too many.
//
//////////////////////////////////////////////////////////////////////////
extern "C" CKernalDriver* KernalCreateDriver(uint16 uiIndex)
{
	return (uiIndex < GRAPHICS_BOARD_MAX_BOARDS) ? &gacGraphicsBoards[uiIndex] : NULL;
}
