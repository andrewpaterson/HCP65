#include "KernalProcess.h"
#include "LCDBoard.h"


//////////////////////////////////////////////////////////////////////////
//
// The display set up by instruction once its own reset is over: eight
// bit, two lines, on without a cursor, cleared, and incrementing.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalLCDBoard::Init(SKernalDevice* psDevice, uint16 uiSlot)
{
	uint16	uiResult;

	uiResult = CKernalDriver::Init(psDevice, uiSlot);
	muiTouched = 0;
	mbReady = Send(LCD_COMMAND, LCD_INSTRUCTION_FUNCTION) &&
			  Send(LCD_COMMAND, LCD_INSTRUCTION_DISPLAY_ON) &&
			  Send(LCD_COMMAND, LCD_INSTRUCTION_CLEAR) &&
			  Send(LCD_COMMAND, LCD_INSTRUCTION_ENTRY) &&
			  WaitReady();
	return uiResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The display and the two controllers.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalLCDBoard::NumUnits(void) const
{
	return LCD_BOARD_NUM_UNITS;
}


//////////////////////////////////////////////////////////////////////////
//
// A display that never answered can't be opened; the controllers always
// can be.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalLCDBoard::OpenUnit(uint16 uiUnit)
{
	if (uiUnit != LCD_BOARD_DISPLAY)
	{
		return KERNAL_DRIVER_SUCCESS;
	}
	return mbReady ? KERNAL_DRIVER_SUCCESS : KERNAL_DRIVER_FAILED;
}


//////////////////////////////////////////////////////////////////////////
//
// The characters from uiOffset (line 1 is 0 to 15, line 2 16 to 31) to
// the running process's memory at uiUserAddress.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalLCDBoard::Read(uint16 uiUnit, uint32 uiOffset, uint32 uiUserAddress, uint32 uiLength, uint32* puiDone)
{
	uint8	auiText[LCD_BOARD_SIZE];
	uint16	i;

	if (uiUnit != LCD_BOARD_DISPLAY)
	{
		return ReadController(uiUnit, uiOffset, uiUserAddress, uiLength, puiDone);
	}
	if (!IsRange(uiOffset, uiLength))
	{
		return KERNAL_DRIVER_BAD_ARGUMENT;
	}
	for (i = 0; i < (uint16)uiLength; i++)
	{
		if ((i == 0) || (((uint16)uiOffset + i) == LCD_BOARD_COLUMNS))
		{
			if (!SetAddress(uiOffset + i))
			{
				return KERNAL_DRIVER_TIMEOUT;
			}
		}
		if (!ReadByte(&auiText[i]))
		{
			return KERNAL_DRIVER_TIMEOUT;
		}
	}
	if (!KernalCopyToUser(uiUserAddress, auiText, uiLength))
	{
		return KERNAL_DRIVER_BAD_ARGUMENT;
	}
	*puiDone += uiLength;
	return KERNAL_DRIVER_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////
//
// The running process's bytes at uiUserAddress on the display from
// uiOffset.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalLCDBoard::Write(uint16 uiUnit, uint32 uiOffset, uint32 uiUserAddress, uint32 uiLength, uint32* puiDone)
{
	uint8	auiText[LCD_BOARD_SIZE];
	uint16	i;

	if (uiUnit != LCD_BOARD_DISPLAY)
	{
		return KERNAL_DRIVER_NOT_SUPPORTED;
	}
	if (!IsRange(uiOffset, uiLength) || !KernalCopyFromUser(auiText, uiUserAddress, uiLength))
	{
		return KERNAL_DRIVER_BAD_ARGUMENT;
	}
	for (i = 0; i < (uint16)uiLength; i++)
	{
		if ((i == 0) || (((uint16)uiOffset + i) == LCD_BOARD_COLUMNS))
		{
			if (!SetAddress(uiOffset + i))
			{
				return KERNAL_DRIVER_TIMEOUT;
			}
		}
		if (!Send(LCD_DATA, auiText[i]))
		{
			return KERNAL_DRIVER_TIMEOUT;
		}
		*puiDone += 1;
	}
	return KERNAL_DRIVER_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////
//
// HCP65LCDBoard.h's LCD_BOARD_* codes.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalLCDBoard::Control(uint16 uiUnit, uint16 uiCode, uint32 uiValue, uint32* puiResult)
{
	if (uiUnit != LCD_BOARD_DISPLAY)
	{
		return KERNAL_DRIVER_NOT_SUPPORTED;
	}
	switch (uiCode)
	{
	case LCD_BOARD_CLEAR:
		return Send(LCD_COMMAND, LCD_INSTRUCTION_CLEAR) ? KERNAL_DRIVER_SUCCESS : KERNAL_DRIVER_TIMEOUT;

	default:
		return KERNAL_DRIVER_NOT_SUPPORTED;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Until the busy flag is clear, false if it never is.  E rises with R/~W
// already high, and the status is read again with E still high, the
// first read having given the display time to put it out.
//
//////////////////////////////////////////////////////////////////////////
bool CKernalLCDBoard::WaitReady(void)
{
	uint16	i;
	uint8	uiStatus;

	for (i = 0; i < LCD_MAX_POLLS; i++)
	{
		Touch(LCD_STATUS);
		Touch(LCD_STATUS_E);
		uiStatus = mpuiRegisters[LCD_STATUS_E];
		Touch(LCD_STATUS);
		if (!(uiStatus & LCD_BUSY))
		{
			return true;
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// A read of uiRegister for the pins it sets.  What it reads is kept, so
// the read can't be left out.
//
//////////////////////////////////////////////////////////////////////////
void CKernalLCDBoard::Touch(uint8 uiRegister)
{
	muiTouched = mpuiRegisters[uiRegister];
}


//////////////////////////////////////////////////////////////////////////
//
// uiValue to the instruction register (LCD_COMMAND) or as data
// (LCD_DATA), once the display isn't busy: RS and R/~W set with E low, E
// raised by writing the byte, and dropped by a read.
//
//////////////////////////////////////////////////////////////////////////
bool CKernalLCDBoard::Send(uint8 uiRegister, uint8 uiValue)
{
	if (!WaitReady())
	{
		return false;
	}
	Touch(uiRegister);
	mpuiRegisters[uiRegister | LCD_COMMAND_E] = uiValue;
	Touch(uiRegister);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The data at the display's address counter, which then moves on.
//
//////////////////////////////////////////////////////////////////////////
bool CKernalLCDBoard::ReadByte(uint8* puiValue)
{
	if (!WaitReady())
	{
		return false;
	}
	Touch(LCD_READ_DATA);
	Touch(LCD_READ_DATA_E);
	*puiValue = mpuiRegisters[LCD_READ_DATA_E];
	Touch(LCD_READ_DATA);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The display's address counter on the character uiPosition is (0 to 31).
//
//////////////////////////////////////////////////////////////////////////
bool CKernalLCDBoard::SetAddress(uint32 uiPosition)
{
	uint8	uiAddress;

	uiAddress = (uint8)(uiPosition % LCD_BOARD_COLUMNS);
	if (uiPosition >= LCD_BOARD_COLUMNS)
	{
		uiAddress |= LCD_LINE_2;
	}
	return Send(LCD_COMMAND, (uint8)(LCD_INSTRUCTION_ADDRESS | uiAddress));
}


//////////////////////////////////////////////////////////////////////////
//
// A controller's buttons from its latches, pressed 1, low byte first:
// the high latch's eight are the word's low byte, the low latch's four
// the next.  Up to the two bytes from uiOffset.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalLCDBoard::ReadController(uint16 uiUnit, uint32 uiOffset, uint32 uiUserAddress, uint32 uiLength, uint32* puiDone)
{
	volatile uint8*	puiLatches;
	uint8			auiButtons[LCD_BOARD_SNES_SIZE];

	if ((uiOffset >= LCD_BOARD_SNES_SIZE) || (uiLength > (LCD_BOARD_SNES_SIZE - uiOffset)))
	{
		return KERNAL_DRIVER_BAD_ARGUMENT;
	}
	puiLatches = &mpuiRegisters[LCD_SNES_1_LOW + (uiUnit - LCD_BOARD_SNES_1) * LCD_SNES_SIZE];
	auiButtons[0] = (uint8)~puiLatches[1];
	auiButtons[1] = (uint8)(~puiLatches[0] & 0x0F);
	if (!KernalCopyToUser(uiUserAddress, &auiButtons[uiOffset], uiLength))
	{
		return KERNAL_DRIVER_BAD_ARGUMENT;
	}
	*puiDone += uiLength;
	return KERNAL_DRIVER_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////
//
// uiLength characters from uiOffset are all on the display.
//
//////////////////////////////////////////////////////////////////////////
bool CKernalLCDBoard::IsRange(uint32 uiOffset, uint32 uiLength)
{
	return (uiOffset < LCD_BOARD_SIZE) && (uiLength <= (LCD_BOARD_SIZE - uiOffset));
}


//The drivers of the LCD boards, in slot order.
static CKernalLCDBoard	gacLCDBoards[LCD_BOARD_MAX_BOARDS];


//////////////////////////////////////////////////////////////////////////
//
// The kernal's loader calls it, once the driver is in place and its
// globals are constructed, for each board it drives: the driver of the
// uiIndex'th, NULL if there are too many.
//
//////////////////////////////////////////////////////////////////////////
extern "C" CKernalDriver* KernalCreateDriver(uint16 uiIndex)
{
	return (uiIndex < LCD_BOARD_MAX_BOARDS) ? &gacLCDBoards[uiIndex] : NULL;
}
