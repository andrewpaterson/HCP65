#include "KernalInterrupts.h"
#include "KernalProcess.h"
#include "VideoBoard.h"


//The modes' timings (VESA): 640x480, 800x600 and 1024x768, all at 60Hz.
static const SVideoMode	gasVideoModes[VIDEO_NUM_MODES] =
{
	{ VIDEO_CLOCK_ENABLE | 2, VIDEO_FLAG_VSYNC_NEGATIVE | VIDEO_FLAG_HSYNC_NEGATIVE, { 80, 82, 94, 100 }, { 480, 490, 492, 525 } },
	{ VIDEO_CLOCK_ENABLE | 5, 0, { 100, 105, 121, 132 }, { 600, 601, 605, 628 } },
	{ VIDEO_CLOCK_ENABLE | 6, VIDEO_FLAG_VSYNC_NEGATIVE | VIDEO_FLAG_HSYNC_NEGATIVE, { 128, 131, 148, 168 }, { 768, 771, 777, 806 } },
};


//////////////////////////////////////////////////////////////////////////
//
// Enabled with its interrupt on and Palette Write on (palette 0), but no
// mode: the clock stopped and the output off.  The interrupt is trusted.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalVideoBoard::Init(SKernalDevice* psDevice, uint16 uiSlot)
{
	uint16	uiResult;

	uiResult = CKernalDriver::Init(psDevice, uiSlot);
	mbSeparateUnits = false;
	muiMode = 0xFF;
	muiWidth = 0;
	muiHeight = 0;
	mbFlip = false;
	muiVBlanks = 0;
	mbText = false;
	muiColumns = 0;
	muiRows = 0;
	muiWindow = ((psDevice->uiWindow != KERNAL_NO_WINDOW) && (psDevice->uiWindow != 0)) ? (uint32)psDevice->uiWindow * VIDEO_WINDOW_SIZE : 0;

	Off();
	mpuiRegisters[VIDEO_PALETTE_BYTE] = 0;
	SetInterrupt(&mpuiRegisters[VIDEO_FLAGS], true);
	WriteFlags(VIDEO_FLAG_ENABLE | VIDEO_FLAG_INTERRUPT | VIDEO_FLAG_PALETTE_WRITE);
	return uiResult;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CKernalVideoBoard::Kill(void)
{
	Off();
	WriteFlags(0);
	CKernalDriver::Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalVideoBoard::NumUnits(void)
{
	return VIDEO_BOARD_FONT + 1;
}


//////////////////////////////////////////////////////////////////////////
//
// Vertical blank: cleared, counted, and a flip made.  Trusted: only the
// board and the driver's own memory.
//
//////////////////////////////////////////////////////////////////////////
void CKernalVideoBoard::Interrupt(void)
{
	bool	bFlipped;

	mpuiRegisters[VIDEO_CLEAR_INTERRUPT] = 0;
	muiVBlanks++;
	bFlipped = mbFlip;
	if (mbFlip)
	{
		mbFlip = false;
		WriteFlags((uint8)(muiFlags ^ VIDEO_FLAG_MEMORY_FRAME));
	}
	TextBlank(bFlipped);
}


//////////////////////////////////////////////////////////////////////////
//
// uiLength bytes of the unit from uiOffset to the running process's
// memory at uiUserAddress.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalVideoBoard::Read(uint16 uiUnit, uint32 uiOffset, uint32 uiUserAddress, uint32 uiLength, uint32* puiDone)
{
	uint8*	puiMemory;

	puiMemory = UnitMemory(uiUnit, uiOffset, uiLength);
	if (!puiMemory || !KernalCopyToUser(uiUserAddress, puiMemory, uiLength))
	{
		return KERNAL_DRIVER_BAD_ARGUMENT;
	}
	*puiDone += uiLength;
	return KERNAL_DRIVER_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////
//
// uiLength bytes from the running process's memory at uiUserAddress to
// the unit from uiOffset.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalVideoBoard::Write(uint16 uiUnit, uint32 uiOffset, uint32 uiUserAddress, uint32 uiLength, uint32* puiDone)
{
	uint8*	puiMemory;

	if (uiUnit == VIDEO_BOARD_TEXT)
	{
		return WriteText(uiOffset, uiUserAddress, uiLength, puiDone);
	}
	if (uiUnit == VIDEO_BOARD_FONT)
	{
		if ((uiOffset > VIDEO_FONT_SIZE) || (uiLength > VIDEO_FONT_SIZE - uiOffset) || !KernalCopyFromUser(&mauiFont[uiOffset], uiUserAddress, uiLength))
		{
			return KERNAL_DRIVER_BAD_ARGUMENT;
		}
		*puiDone += uiLength;
		return KERNAL_DRIVER_SUCCESS;
	}

	puiMemory = UnitMemory(uiUnit, uiOffset, uiLength);
	if (!puiMemory || !KernalCopyFromUser(puiMemory, uiUserAddress, uiLength))
	{
		return KERNAL_DRIVER_BAD_ARGUMENT;
	}
	*puiDone += uiLength;
	return KERNAL_DRIVER_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////
//
// Where uiLength bytes of the unit from uiOffset are: in the main memory
// window, or the device RAM.  NULL if they aren't all in the unit.
//
//////////////////////////////////////////////////////////////////////////
uint8* CKernalVideoBoard::UnitMemory(uint16 uiUnit, uint32 uiOffset, uint32 uiLength)
{
	uint32	uiSize;
	uint32	uiAddress;

	switch (uiUnit)
	{
	case VIDEO_BOARD_PIXELS:
		uiSize = VIDEO_BOARD_FRAME_SIZE;
		uiAddress = muiWindow;
		break;

	case VIDEO_BOARD_PALETTE_NUMBERS:
		uiSize = VIDEO_BOARD_FRAME_SIZE;
		uiAddress = muiWindow ? muiWindow + VIDEO_BOARD_FRAME_SIZE : 0;
		break;

	case VIDEO_BOARD_PALETTES:
		uiSize = VIDEO_BOARD_PALETTES_SIZE;
		uiAddress = mpsDevice->uiAddress + VIDEO_PALETTES;
		break;

	case VIDEO_BOARD_TEXT:
	case VIDEO_BOARD_FONT:
		return NULL;

	default:
		uiSize = VIDEO_BOARD_ROWS_SIZE;
		uiAddress = mpsDevice->uiAddress + VIDEO_ROW_TABLE;
		break;
	}
	if ((uiAddress == 0) || (uiOffset >= uiSize) || (uiLength > uiSize - uiOffset))
	{
		return NULL;
	}
	return (uint8*)(uiAddress + uiOffset);
}


//////////////////////////////////////////////////////////////////////////
//
// HCP65VideoBoard.h's VIDEO_BOARD_* codes.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalVideoBoard::Control(uint16 uiUnit, uint16 uiCode, uint32 uiValue, uint32* puiResult)
{
	switch (uiCode)
	{
	case VIDEO_BOARD_SET_MODE:
		if (uiValue >= VIDEO_NUM_MODES)
		{
			return KERNAL_DRIVER_BAD_ARGUMENT;
		}
		mbText = false;
		SetMode((uint8)uiValue);
		return KERNAL_DRIVER_SUCCESS;

	case VIDEO_BOARD_GET_WIDTH:
		*puiResult = muiWidth;
		return KERNAL_DRIVER_SUCCESS;

	case VIDEO_BOARD_GET_HEIGHT:
		*puiResult = muiHeight;
		return KERNAL_DRIVER_SUCCESS;

	case VIDEO_BOARD_SET_PALETTE:
		if (uiValue == VIDEO_PALETTE_NONE)
		{
			WriteFlags((uint8)(muiFlags & ~VIDEO_FLAG_PALETTE_WRITE));
			return KERNAL_DRIVER_SUCCESS;
		}
		if (uiValue >= VIDEO_BOARD_NUM_PALETTES)
		{
			return KERNAL_DRIVER_BAD_ARGUMENT;
		}
		mpuiRegisters[VIDEO_PALETTE_BYTE] = (uint8)uiValue;
		WriteFlags((uint8)(muiFlags | VIDEO_FLAG_PALETTE_WRITE));
		return KERNAL_DRIVER_SUCCESS;

	case VIDEO_BOARD_FLIP:
		mbFlip = true;
		return KERNAL_DRIVER_SUCCESS;

	case VIDEO_BOARD_GET_SHOWN:
		*puiResult = (muiFlags & VIDEO_FLAG_MEMORY_FRAME) ? 0 : 1;
		return KERNAL_DRIVER_SUCCESS;

	case VIDEO_BOARD_GET_VBLANKS:
		*puiResult = muiVBlanks;
		return KERNAL_DRIVER_SUCCESS;

	case VIDEO_BOARD_OFF:
		Off();
		return KERNAL_DRIVER_SUCCESS;

	case VIDEO_BOARD_SET_TEXT_MODE:
		if (uiValue >= VIDEO_NUM_MODES)
		{
			return KERNAL_DRIVER_BAD_ARGUMENT;
		}
		return SetTextMode((uint8)uiValue) ? KERNAL_DRIVER_SUCCESS : KERNAL_DRIVER_FAILED;

	case VIDEO_BOARD_GET_COLUMNS:
		*puiResult = mbText ? muiColumns : 0;
		return KERNAL_DRIVER_SUCCESS;

	case VIDEO_BOARD_GET_ROWS:
		*puiResult = mbText ? muiRows : 0;
		return KERNAL_DRIVER_SUCCESS;

	case VIDEO_BOARD_SET_TEXT_COLOUR:
		if (!mbText || (uiValue >= VIDEO_TEXT_CURSOR_PALETTE))
		{
			return KERNAL_DRIVER_BAD_ARGUMENT;
		}
		muiTextColour = (uint8)uiValue;
		return KERNAL_DRIVER_SUCCESS;

	case VIDEO_BOARD_SCROLL_TEXT:
		return ScrollText(uiValue);

	case VIDEO_BOARD_CLEAR_TEXT:
		return ClearText();

	case VIDEO_BOARD_SET_CURSOR:
		if (!mbText || ((uiValue != VIDEO_CURSOR_NONE) && (uiValue >= (uint32)muiRows * muiColumns)))
		{
			return KERNAL_DRIVER_BAD_ARGUMENT;
		}
		SetCursorCell((uiValue == VIDEO_CURSOR_NONE) ? VIDEO_CURSOR_NONE : PhysicalCell((uint16)uiValue));
		return KERNAL_DRIVER_SUCCESS;

	default:
		return KERNAL_DRIVER_NOT_SUPPORTED;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Sleeps up to uiTicks for the next vertical blank.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalVideoBoard::Wait(uint16 uiUnit, uint32 uiValue, uint16 uiTicks)
{
	if (uiValue != VIDEO_WAIT_VBLANK)
	{
		return KERNAL_DRIVER_BAD_ARGUMENT;
	}
	return WaitFor(uiUnit, muiVBlanks, uiTicks);
}


//////////////////////////////////////////////////////////////////////////
//
// uiValue is the vertical blanks counted when the wait started.
//
//////////////////////////////////////////////////////////////////////////
bool CKernalVideoBoard::IsWaitOver(uint16 uiUnit, uint32 uiValue)
{
	return muiVBlanks != uiValue;
}


//////////////////////////////////////////////////////////////////////////
//
// The output off and the clock stopped while the timing changes; a row
// after another in the row table, the pattern a pixel a clock and the
// sync polarities; then the clock and the output on.
//
//////////////////////////////////////////////////////////////////////////
void CKernalVideoBoard::SetMode(uint8 uiMode)
{
	const SVideoMode*	psMode;
	volatile uint8*		puiRow;
	uint32				uiStart;
	uint16				i;

	psMode = &gasVideoModes[uiMode];
	Off();
	for (i = 0; i < 4; i++)
	{
		mpuiRegisters[VIDEO_HORIZONTAL + i] = psMode->auiHorizontal[i];
		mpuiRegisters[VIDEO_VERTICAL + i * 2] = (uint8)psMode->auiVertical[i];
		mpuiRegisters[VIDEO_VERTICAL + i * 2 + 1] = (uint8)(psMode->auiVertical[i] >> 8);
	}
	mpuiRegisters[VIDEO_PATTERN_LOW] = 0x0F;
	mpuiRegisters[VIDEO_PATTERN_HIGH] = 0xFF;

	muiWidth = (uint16)(psMode->auiHorizontal[0] * 8);
	muiHeight = psMode->auiVertical[0];
	puiRow = (volatile uint8*)(mpsDevice->uiAddress + VIDEO_ROW_TABLE);
	uiStart = 0;
	for (i = 0; i < muiHeight; i++)
	{
		puiRow[0] = (uint8)uiStart;
		puiRow[1] = (uint8)(uiStart >> 8);
		puiRow[2] = (uint8)(uiStart >> 16);
		puiRow[3] = 0;
		puiRow += 4;
		uiStart += muiWidth;
	}

	WriteFlags((uint8)((muiFlags & ~(VIDEO_FLAG_VSYNC_NEGATIVE | VIDEO_FLAG_HSYNC_NEGATIVE)) | psMode->uiSyncFlags));
	mpuiRegisters[VIDEO_CLOCK_SELECT] = psMode->uiClockSelect;
	mpuiRegisters[VIDEO_SIGNAL_DELAY] = 0;
	muiMode = uiMode;
}


//////////////////////////////////////////////////////////////////////////
//
// Nothing to the monitor: the output off and the pixel clock stopped.
//
//////////////////////////////////////////////////////////////////////////
void CKernalVideoBoard::Off(void)
{
	mbText = false;
	mpuiRegisters[VIDEO_SIGNAL_DELAY] = VIDEO_DELAY_OFF;
	mpuiRegisters[VIDEO_CLOCK_SELECT] = 0;
	muiMode = 0xFF;
	muiWidth = 0;
	muiHeight = 0;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CKernalVideoBoard::WriteFlags(uint8 uiFlags)
{
	muiFlags = uiFlags;
	mpuiRegisters[VIDEO_FLAGS] = uiFlags;
}


//The driver of the video board.
static CKernalVideoBoard	gacVideoBoards[VIDEO_BOARD_MAX_BOARDS];


//////////////////////////////////////////////////////////////////////////
//
// The kernal's loader calls it, once the driver is in place and its
// globals are constructed, for each board it drives: the driver of the
// uiIndex'th, NULL if there are too many.
//
//////////////////////////////////////////////////////////////////////////
extern "C" CKernalDriver* KernalCreateDriver(uint16 uiIndex)
{
	return (uiIndex < VIDEO_BOARD_MAX_BOARDS) ? &gacVideoBoards[uiIndex] : NULL;
}
