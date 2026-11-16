#include <string.h>
#include "HCP65User.h"
#include "HCP65VideoBoard.h"


//TestHCP65KernalVideo runs this (\vidtest.exe) as its start up program with a Video Board, and checks the picture it leaves: 640x480, the pixel
//at (x, y) palette 0's colour (x + y) & $FF, but row 0, whose palette number is 5 (all red), and row 1, which the row
//table makes row 0 again.  Palette 0's colour i is red i, green 255 - i and blue $40.
#define VIDEO_TEST_MAX_BOARDS	20
#define VIDEO_TEST_WAIT_TICKS	KERNAL_DEVICE_TICKS_PER_SECOND
#define VIDEO_TEST_WIDTH		640
#define VIDEO_TEST_HEIGHT		480
#define VIDEO_TEST_RED			5


uint8	gauiVideoTestRow[VIDEO_TEST_WIDTH];


//////////////////////////////////////////////////////////////////////////
//
// The board is there with its four units; it opens and refuses what it
// doesn't take.
//
//////////////////////////////////////////////////////////////////////////
uint16 VideoTestOpen(uint16* puiPixels, uint16* puiPalettes, uint16* puiNumbers, uint16* puiRows)
{
	SKernalDeviceInfo	sInfo;
	uint16				uiIndex;
	uint16				uiHandle;
	uint32				uiResult;

	for (uiIndex = 0; uiIndex < VIDEO_TEST_MAX_BOARDS; uiIndex++)
	{
		if (KernalDeviceInfo(uiIndex, &sInfo) != KERNAL_DEVICE_SUCCESS)
		{
			return 1;
		}
		if ((strcmp(sInfo.szManufacturer, VIDEO_BOARD_MANUFACTURER) == 0) && (strcmp(sInfo.szName, VIDEO_BOARD_NAME) == 0))
		{
			break;
		}
	}
	if ((sInfo.uiNumUnits != 4) || sInfo.uiOpen)
	{
		return 2;
	}
	if (KernalDeviceOpen(VIDEO_BOARD_MANUFACTURER, VIDEO_BOARD_NAME, 0, 4, &uiHandle) != KERNAL_DEVICE_BAD_UNIT)
	{
		return 3;
	}
	if ((KernalDeviceOpen(VIDEO_BOARD_MANUFACTURER, VIDEO_BOARD_NAME, 0, VIDEO_BOARD_PIXELS, puiPixels) != KERNAL_DEVICE_SUCCESS) ||
		(KernalDeviceOpen(VIDEO_BOARD_MANUFACTURER, VIDEO_BOARD_NAME, 0, VIDEO_BOARD_PALETTES, puiPalettes) != KERNAL_DEVICE_SUCCESS) ||
		(KernalDeviceOpen(VIDEO_BOARD_MANUFACTURER, VIDEO_BOARD_NAME, 0, VIDEO_BOARD_PALETTE_NUMBERS, puiNumbers) != KERNAL_DEVICE_SUCCESS) ||
		(KernalDeviceOpen(VIDEO_BOARD_MANUFACTURER, VIDEO_BOARD_NAME, 0, VIDEO_BOARD_ROWS, puiRows) != KERNAL_DEVICE_SUCCESS))
	{
		return 4;
	}

	uiResult = 1;
	if ((KernalDeviceControl(*puiPixels, VIDEO_BOARD_GET_WIDTH, 0, &uiResult) != KERNAL_DEVICE_SUCCESS) || (uiResult != 0))
	{
		return 5;
	}
	if ((KernalDeviceControl(*puiPixels, 99, 0, &uiResult) != KERNAL_DEVICE_NOT_SUPPORTED) ||
		(KernalDeviceControl(*puiPixels, VIDEO_BOARD_SET_MODE, VIDEO_NUM_MODES, NULL) != KERNAL_DEVICE_BAD_ARGUMENT) ||
		(KernalDeviceControl(*puiPixels, VIDEO_BOARD_SET_PALETTE, VIDEO_BOARD_NUM_PALETTES, NULL) != KERNAL_DEVICE_BAD_ARGUMENT) ||
		(KernalDeviceWait(*puiPixels, 1, 1) != KERNAL_DEVICE_BAD_ARGUMENT) ||
		(KernalDeviceWrite(*puiPixels, VIDEO_BOARD_FRAME_SIZE - 1, gauiVideoTestRow, 2, NULL) != KERNAL_DEVICE_BAD_ARGUMENT) ||
		(KernalDeviceWrite(*puiRows, VIDEO_BOARD_ROWS_SIZE, gauiVideoTestRow, 1, NULL) != KERNAL_DEVICE_BAD_ARGUMENT))
	{
		return 6;
	}
	return 0;
}


//////////////////////////////////////////////////////////////////////////
//
// 640x480, and palettes 0 (red i, green 255 - i, blue $40) and 5 (red).
//
//////////////////////////////////////////////////////////////////////////
uint16 VideoTestMode(uint16 uiPixels, uint16 uiPalettes)
{
	uint32	uiResult;
	uint16	i;

	if (KernalDeviceControl(uiPixels, VIDEO_BOARD_SET_MODE, VIDEO_MODE_640_480, NULL) != KERNAL_DEVICE_SUCCESS)
	{
		return 1;
	}
	if ((KernalDeviceControl(uiPixels, VIDEO_BOARD_GET_WIDTH, 0, &uiResult) != KERNAL_DEVICE_SUCCESS) || (uiResult != VIDEO_TEST_WIDTH) ||
		(KernalDeviceControl(uiPixels, VIDEO_BOARD_GET_HEIGHT, 0, &uiResult) != KERNAL_DEVICE_SUCCESS) || (uiResult != VIDEO_TEST_HEIGHT))
	{
		return 2;
	}

	for (i = 0; i < 256; i++)
	{
		gauiVideoTestRow[i] = (uint8)i;
	}
	if (KernalDeviceWrite(uiPalettes, 0, gauiVideoTestRow, 256, NULL) != KERNAL_DEVICE_SUCCESS)
	{
		return 3;
	}
	for (i = 0; i < 256; i++)
	{
		gauiVideoTestRow[i] = (uint8)(255 - i);
	}
	if (KernalDeviceWrite(uiPalettes, VIDEO_BOARD_GREEN, gauiVideoTestRow, 256, NULL) != KERNAL_DEVICE_SUCCESS)
	{
		return 4;
	}
	memset(gauiVideoTestRow, 0x40, 256);
	if (KernalDeviceWrite(uiPalettes, VIDEO_BOARD_BLUE, gauiVideoTestRow, 256, NULL) != KERNAL_DEVICE_SUCCESS)
	{
		return 5;
	}

	memset(gauiVideoTestRow, 0xFF, 256);
	if (KernalDeviceWrite(uiPalettes, VIDEO_TEST_RED * 256, gauiVideoTestRow, 256, NULL) != KERNAL_DEVICE_SUCCESS)
	{
		return 6;
	}
	memset(gauiVideoTestRow, 0, 256);
	if ((KernalDeviceWrite(uiPalettes, VIDEO_BOARD_GREEN + VIDEO_TEST_RED * 256, gauiVideoTestRow, 256, NULL) != KERNAL_DEVICE_SUCCESS) ||
		(KernalDeviceWrite(uiPalettes, VIDEO_BOARD_BLUE + VIDEO_TEST_RED * 256, gauiVideoTestRow, 256, NULL) != KERNAL_DEVICE_SUCCESS))
	{
		return 7;
	}
	return 0;
}


//////////////////////////////////////////////////////////////////////////
//
// The frame being drawn: (x + y) & $FF with palette 0 (the pixel writes
// set it), then row 0's palette numbers 5.  It is shown at the next
// vertical blank.
//
//////////////////////////////////////////////////////////////////////////
uint16 VideoTestDraw(uint16 uiPixels, uint16 uiNumbers)
{
	uint32	uiShown;
	uint32	uiNowShown;
	uint32	uiDone;
	uint16	x;
	uint16	y;

	if (KernalDeviceControl(uiPixels, VIDEO_BOARD_SET_PALETTE, 0, NULL) != KERNAL_DEVICE_SUCCESS)
	{
		return 1;
	}
	for (y = 0; y < VIDEO_TEST_HEIGHT; y++)
	{
		for (x = 0; x < VIDEO_TEST_WIDTH; x++)
		{
			gauiVideoTestRow[x] = (uint8)(x + y);
		}
		uiDone = 0;
		if ((KernalDeviceWrite(uiPixels, (uint32)y * VIDEO_TEST_WIDTH, gauiVideoTestRow, VIDEO_TEST_WIDTH, &uiDone) != KERNAL_DEVICE_SUCCESS) || (uiDone != VIDEO_TEST_WIDTH))
		{
			return 2;
		}
	}
	memset(gauiVideoTestRow, VIDEO_TEST_RED, VIDEO_TEST_WIDTH);
	if (KernalDeviceWrite(uiNumbers, 0, gauiVideoTestRow, VIDEO_TEST_WIDTH, NULL) != KERNAL_DEVICE_SUCCESS)
	{
		return 3;
	}

	//Read back what was written.
	if ((KernalDeviceRead(uiPixels, VIDEO_TEST_WIDTH + 10, gauiVideoTestRow, 1, NULL) != KERNAL_DEVICE_SUCCESS) || (gauiVideoTestRow[0] != 11))
	{
		return 4;
	}

	if (KernalDeviceControl(uiPixels, VIDEO_BOARD_GET_SHOWN, 0, &uiShown) != KERNAL_DEVICE_SUCCESS)
	{
		return 5;
	}
	if (KernalDeviceControl(uiPixels, VIDEO_BOARD_FLIP, 0, NULL) != KERNAL_DEVICE_SUCCESS)
	{
		return 6;
	}
	if (KernalDeviceWait(uiPixels, VIDEO_WAIT_VBLANK, VIDEO_TEST_WAIT_TICKS) != KERNAL_DEVICE_SUCCESS)
	{
		return 7;
	}
	if ((KernalDeviceControl(uiPixels, VIDEO_BOARD_GET_SHOWN, 0, &uiNowShown) != KERNAL_DEVICE_SUCCESS) || (uiNowShown == uiShown))
	{
		return 8;
	}
	return 0;
}


//////////////////////////////////////////////////////////////////////////
//
// Row 1 is made row 0 again through the row table, and the picture is
// left as it is once two more frames have been drawn.
//
//////////////////////////////////////////////////////////////////////////
uint16 VideoTestRows(uint16 uiPixels, uint16 uiRows)
{
	uint32	uiVBlanks;
	uint16	i;

	memset(gauiVideoTestRow, 0, 4);
	if (KernalDeviceWrite(uiRows, 4, gauiVideoTestRow, 4, NULL) != KERNAL_DEVICE_SUCCESS)
	{
		return 1;
	}
	if ((KernalDeviceRead(uiRows, 8, gauiVideoTestRow, 4, NULL) != KERNAL_DEVICE_SUCCESS) || (gauiVideoTestRow[0] != (uint8)(2 * VIDEO_TEST_WIDTH)) || (gauiVideoTestRow[1] != (uint8)((2 * VIDEO_TEST_WIDTH) >> 8)))
	{
		return 2;
	}
	for (i = 0; i < 2; i++)
	{
		if (KernalDeviceWait(uiPixels, VIDEO_WAIT_VBLANK, VIDEO_TEST_WAIT_TICKS) != KERNAL_DEVICE_SUCCESS)
		{
			return 3;
		}
	}
	if ((KernalDeviceControl(uiPixels, VIDEO_BOARD_GET_VBLANKS, 0, &uiVBlanks) != KERNAL_DEVICE_SUCCESS) || (uiVBlanks < 3))
	{
		return 4;
	}
	return 0;
}


//////////////////////////////////////////////////////////////////////////
//
// The Video Board driver's test (TestHCP65KernalVideo's card starts it:
// \OS\STARTUP.TXT names it).  Exits 0 if everything did what it should, otherwise 100
// times the step that failed plus which check in it.  It leaves the
// picture shown.
//
//////////////////////////////////////////////////////////////////////////
int main(void)
{
	uint16	uiPixels;
	uint16	uiPalettes;
	uint16	uiNumbers;
	uint16	uiRows;
	uint16	uiResult;

	uiResult = VideoTestOpen(&uiPixels, &uiPalettes, &uiNumbers, &uiRows);
	if (uiResult != 0)
	{
		return 100 + uiResult;
	}
	uiResult = VideoTestMode(uiPixels, uiPalettes);
	if (uiResult != 0)
	{
		return 200 + uiResult;
	}
	uiResult = VideoTestDraw(uiPixels, uiNumbers);
	if (uiResult != 0)
	{
		return 300 + uiResult;
	}
	uiResult = VideoTestRows(uiPixels, uiRows);
	if (uiResult != 0)
	{
		return 400 + uiResult;
	}
	return 0;
}

