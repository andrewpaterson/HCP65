#include <string.h>
#include "HCP65User.h"
#include "HCP65LCDBoard.h"


//\LCDTEST.EXE: HCP65 bounced left and right along the first line of the LCD and SNES Board's display, a character
//every LCD_TEST_STEP_TICKS, until a key is pressed.  The display is cleared as it ends.  Exits 1 without an LCD and
//SNES Board (or its driver), and 2 if the display stops answering.
#define LCD_TEST_TEXT			"HCP65"
#define LCD_TEST_TEXT_LENGTH	5
#define LCD_TEST_LAST_COLUMN	(LCD_BOARD_COLUMNS - LCD_TEST_TEXT_LENGTH)	//Where the text turns back.
#define LCD_TEST_STEP_TICKS		(KERNAL_DEVICE_TICKS_PER_SECOND / 8)


char	gacLCDTestLine[LCD_BOARD_COLUMNS];


//////////////////////////////////////////////////////////////////////////
//
// The first line: spaces, with the text from uiColumn.
//
//////////////////////////////////////////////////////////////////////////
static uint16 LCDTestDraw(uint16 uiHandle, uint16 uiColumn)
{
	memset(gacLCDTestLine, ' ', LCD_BOARD_COLUMNS);
	memcpy(&gacLCDTestLine[uiColumn], LCD_TEST_TEXT, LCD_TEST_TEXT_LENGTH);
	return KernalDeviceWrite(uiHandle, 0, gacLCDTestLine, LCD_BOARD_COLUMNS, NULL);
}


//////////////////////////////////////////////////////////////////////////
//
// Whether a key was pressed, waiting up to uiTicks for one.
//
//////////////////////////////////////////////////////////////////////////
static bool LCDTestKey(uint16 uiTicks)
{
	uint32	uiDone;
	char	c;

	KernalDeviceWait(KERNAL_STANDARD_INPUT, KERNAL_CONSOLE_WAIT_INPUT, uiTicks);
	uiDone = 0;
	KernalDeviceRead(KERNAL_STANDARD_INPUT, 0, &c, 1, &uiDone);
	return uiDone > 0;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
int main(void)
{
	uint16	uiHandle;
	uint16	uiColumn;
	bool	bRight;
	uint32	uiResult;
	int		iExit;

	if (KernalDeviceOpen(LCD_BOARD_MANUFACTURER, LCD_BOARD_NAME, 0, LCD_BOARD_DISPLAY, &uiHandle) != KERNAL_DEVICE_SUCCESS)
	{
		return 1;
	}

	iExit = 0;
	uiColumn = 0;
	bRight = true;
	for (;;)
	{
		if (LCDTestDraw(uiHandle, uiColumn) != KERNAL_DEVICE_SUCCESS)
		{
			iExit = 2;
			break;
		}
		if (LCDTestKey(LCD_TEST_STEP_TICKS))
		{
			break;
		}
		if (bRight)
		{
			uiColumn++;
			bRight = uiColumn < LCD_TEST_LAST_COLUMN;
		}
		else
		{
			uiColumn--;
			bRight = uiColumn == 0;
		}
	}

	KernalDeviceControl(uiHandle, LCD_BOARD_CLEAR, 0, &uiResult);
	KernalDeviceClose(uiHandle);
	return iExit;
}
