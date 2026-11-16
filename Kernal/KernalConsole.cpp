#include <string.h>
#include "BaseLib/FatFile.h"
#include "UserLib/HCP65User.h"
#include "UserLib/HCP65UARTBoard.h"
#include "UserLib/HCP65PS2Board.h"
#include "UserLib/HCP65VideoBoard.h"
#include "KernalProcess.h"
#include "KernalStartUp.h"
#include "KernalConsole.h"


CKernalConsole	gcKernalConsole;
static uint8	gauiKernalConsoleFont[KERNAL_CONSOLE_FONT_SIZE];


//////////////////////////////////////////////////////////////////////////
//
// Nothing taken yet.
//
//////////////////////////////////////////////////////////////////////////
void CKernalConsole::Init(void)
{
	mpcOutput = NULL;
	muiOutputUnit = 0;
	mbVideo = false;
	mpcInput = NULL;
	muiInputUnit = 0;
	mbKeyboard = false;
	muiColumn = 0;
	muiRow = 0;
	muiColumns = 0;
	muiRows = 0;
	mcKeyboard.Init();
	mbCapsLock = false;
	muiTyped = 0;
}


//////////////////////////////////////////////////////////////////////////
//
// The Video Board's text: its font loaded from the SD card, 640x480 (80
// by 30) and the cursor at the top left.  False (and the board let go) if
// there is no Video Board, its font or its pixels.
//
//////////////////////////////////////////////////////////////////////////
bool CKernalConsole::TakeVideo(void)
{
	CKernalDriver*	pcVideo;
	CFatFile		cFile;
	lsize			uiRead;
	uint32			uiDone;
	bool			bResult;

	pcVideo = KernalFindDriver(VIDEO_BOARD_MANUFACTURER, VIDEO_BOARD_NAME, 0);
	if (!pcVideo || (pcVideo->Open(VIDEO_BOARD_TEXT, KERNAL_CONSOLE_OWNER) != KERNAL_DRIVER_SUCCESS))
	{
		return false;
	}

	cFile.Init(&gcKernalVolume);
	uiRead = 0;
	bResult = cFile.Open(KERNAL_CONSOLE_FONT_FILE, FAT_FILE_ACCESS_READ) == FAT_SUCCESS;
	if (bResult)
	{
		bResult = (cFile.Read(gauiKernalConsoleFont, KERNAL_CONSOLE_FONT_SIZE, &uiRead) == FAT_SUCCESS) && (uiRead == KERNAL_CONSOLE_FONT_SIZE);
		cFile.Close();
	}

	uiDone = 0;
	KernalSetKernalCopy(true);
	bResult = bResult && (pcVideo->Write(VIDEO_BOARD_FONT, 0, (uint32)gauiKernalConsoleFont, KERNAL_CONSOLE_FONT_SIZE, &uiDone) == KERNAL_DRIVER_SUCCESS);
	KernalSetKernalCopy(false);
	mpcOutput = pcVideo;
	muiOutputUnit = VIDEO_BOARD_TEXT;
	bResult = bResult && (VideoMode(VIDEO_MODE_640_480) == KERNAL_DRIVER_SUCCESS);
	if (!bResult)
	{
		mpcOutput = NULL;
		pcVideo->Close(VIDEO_BOARD_TEXT, KERNAL_CONSOLE_OWNER);
		return false;
	}
	mbVideo = true;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The PS2 Board's port 1, for a keyboard.  False if there is no PS2
// Board.
//
//////////////////////////////////////////////////////////////////////////
bool CKernalConsole::TakeKeyboard(void)
{
	CKernalDriver*	pcPS2;

	pcPS2 = KernalFindDriver(PS2_BOARD_MANUFACTURER, PS2_BOARD_NAME, 0);
	if (!pcPS2 || (pcPS2->Open(PS2_BOARD_PORT_1, KERNAL_CONSOLE_OWNER) != KERNAL_DRIVER_SUCCESS))
	{
		return false;
	}
	mpcInput = pcPS2;
	muiInputUnit = PS2_BOARD_PORT_1;
	mbKeyboard = true;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The boards' units, if they aren't taken already and are there to take:
// the Video Board and the keyboard, or else the UART Board's TTL port.
//
//////////////////////////////////////////////////////////////////////////
void CKernalConsole::Take(void)
{
	CKernalDriver*	pcUART;

	if (mpcOutput && mpcInput)
	{
		return;
	}
	if (!mpcOutput)
	{
		TakeVideo();
	}
	if (!mpcInput)
	{
		TakeKeyboard();
	}
	if (mpcOutput && mpcInput)
	{
		return;
	}

	pcUART = KernalFindDriver(UART_BOARD_MANUFACTURER, UART_BOARD_NAME, 0);
	if (pcUART && (pcUART->Open(UART_BOARD_TTL, KERNAL_CONSOLE_OWNER) == KERNAL_DRIVER_SUCCESS))
	{
		if (!mpcOutput)
		{
			mpcOutput = pcUART;
			muiOutputUnit = UART_BOARD_TTL;
		}
		if (!mpcInput)
		{
			mpcInput = pcUART;
			muiInputUnit = UART_BOARD_TTL;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// uiLength bytes of kernal memory to the UART, waiting while it is busy.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalConsole::Send(const uint8* puiBytes, uint32 uiLength)
{
	uint32	uiSent;
	uint32	uiDone;
	uint16	uiResult;

	uiResult = KERNAL_DRIVER_SUCCESS;
	uiSent = 0;
	KernalSetKernalCopy(true);
	while (uiSent < uiLength)
	{
		uiDone = 0;
		uiResult = mpcOutput->Write(muiOutputUnit, 0, (uint32)&puiBytes[uiSent], uiLength - uiSent, &uiDone);
		uiSent += uiDone;
		if ((uiResult != KERNAL_DRIVER_SUCCESS) && (uiResult != KERNAL_DRIVER_TIMEOUT))
		{
			break;
		}
		uiResult = KERNAL_DRIVER_SUCCESS;
	}
	KernalSetKernalCopy(false);
	return uiResult;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalConsole::SendText(const char* szText)
{
	return Send((const uint8*)szText, (uint32)strlen(szText));
}


//////////////////////////////////////////////////////////////////////////
//
// A Video Board control, from the kernal.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalConsole::VideoControl(uint16 uiCode, uint32 uiValue, uint32* puiResult)
{
	uint32	uiResult;

	uiResult = 0;
	uiCode = mpcOutput->Control(muiOutputUnit, uiCode, uiValue, &uiResult);
	if (puiResult)
	{
		*puiResult = uiResult;
	}
	return uiCode;
}


//////////////////////////////////////////////////////////////////////////
//
// The Video Board in text mode uiMode, cleared, and its size.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalConsole::VideoMode(uint32 uiMode)
{
	uint32	uiColumns;
	uint32	uiRows;
	uint16	uiResult;

	uiResult = VideoControl(VIDEO_BOARD_SET_TEXT_MODE, uiMode, NULL);
	if (uiResult != KERNAL_DRIVER_SUCCESS)
	{
		return uiResult;
	}
	VideoControl(VIDEO_BOARD_GET_COLUMNS, 0, &uiColumns);
	VideoControl(VIDEO_BOARD_GET_ROWS, 0, &uiRows);
	muiColumns = (uint16)uiColumns;
	muiRows = (uint16)uiRows;
	muiColumn = 0;
	muiRow = 0;
	VideoCursor();
	return KERNAL_DRIVER_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////
//
// The cursor's cell to the Video Board.
//
//////////////////////////////////////////////////////////////////////////
void CKernalConsole::VideoCursor(void)
{
	VideoControl(VIDEO_BOARD_SET_CURSOR, (uint32)muiRow * muiColumns + muiColumn, NULL);
}


//////////////////////////////////////////////////////////////////////////
//
// uiLength characters of kernal memory at the cursor, all on its row.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalConsole::VideoText(const uint8* puiChars, uint32 uiLength)
{
	uint32	uiDone;
	uint16	uiResult;

	uiDone = 0;
	KernalSetKernalCopy(true);
	uiResult = mpcOutput->Write(muiOutputUnit, (uint32)muiRow * muiColumns + muiColumn, (uint32)puiChars, uiLength, &uiDone);
	KernalSetKernalCopy(false);
	muiColumn = (uint16)(muiColumn + uiLength);
	return uiResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The cursor to the start of the next row, the text scrolled up if it was
// on the last.
//
//////////////////////////////////////////////////////////////////////////
void CKernalConsole::VideoNewLine(void)
{
	muiColumn = 0;
	if (muiRow + 1 < muiRows)
	{
		muiRow++;
	}
	else
	{
		VideoControl(VIDEO_BOARD_SCROLL_TEXT, 1, NULL);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// uiLength bytes of kernal memory as text on the Video Board: runs of
// printable characters a row at a time, and the control characters.
//
//////////////////////////////////////////////////////////////////////////
void CKernalConsole::VideoWrite(const uint8* puiBytes, uint32 uiLength)
{
	uint32	uiRun;
	uint32	uiFits;
	uint8	c;
	uint8	uiSpace;

	while (uiLength > 0)
	{
		c = *puiBytes;
		if ((c >= 0x20) && (c != 0x7F))
		{
			if (muiColumn >= muiColumns)
			{
				VideoNewLine();
			}
			uiFits = (uint32)(muiColumns - muiColumn);
			for (uiRun = 1; (uiRun < uiLength) && (uiRun < uiFits) && (puiBytes[uiRun] >= 0x20) && (puiBytes[uiRun] != 0x7F); uiRun++)
			{
			}
			VideoText(puiBytes, uiRun);
			puiBytes += uiRun;
			uiLength -= uiRun;
			continue;
		}

		if (c == '\n')
		{
			VideoNewLine();
		}
		else if (c == '\r')
		{
			muiColumn = 0;
		}
		else if (c == '\b')
		{
			if (muiColumn > 0)
			{
				muiColumn--;
			}
		}
		else if (c == '\t')
		{
			uiSpace = ' ';
			do
			{
				if (muiColumn >= muiColumns)
				{
					VideoNewLine();
				}
				VideoText(&uiSpace, 1);
			}
			while (muiColumn % 8);
		}
		else if (c == '\f')
		{
			VideoControl(VIDEO_BOARD_CLEAR_TEXT, 0, NULL);
			muiColumn = 0;
			muiRow = 0;
		}
		puiBytes++;
		uiLength--;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The running process's uiLength bytes at uiUserAddress, as text.  All of
// them are done (dropped) without an output.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalConsole::Write(uint32 uiUserAddress, uint32 uiLength, uint32* puiDone)
{
	uint8*	puiIn;
	uint32	uiChunk;
	uint32	uiOut;
	uint32	i;
	uint16	uiResult;

	Take();
	if (!mpcOutput)
	{
		*puiDone += uiLength;
		return KERNAL_DRIVER_SUCCESS;
	}

	//The process's bytes go in the buffer's second half; for the UART, out with each '\n' sent as "\r\n" from its
	//start.
	puiIn = &mauiBuffer[KERNAL_CONSOLE_BUFFER_SIZE];
	while (uiLength > 0)
	{
		uiChunk = (uiLength < KERNAL_CONSOLE_BUFFER_SIZE / 2) ? uiLength : KERNAL_CONSOLE_BUFFER_SIZE / 2;
		if (!KernalCopyFromUser(puiIn, uiUserAddress, uiChunk))
		{
			return KERNAL_DRIVER_BAD_ARGUMENT;
		}
		if (mbVideo)
		{
			VideoWrite(puiIn, uiChunk);
		}
		else
		{
			uiOut = 0;
			for (i = 0; i < uiChunk; i++)
			{
				if (puiIn[i] == '\n')
				{
					mauiBuffer[uiOut++] = '\r';
				}
				mauiBuffer[uiOut++] = puiIn[i];
			}
			uiResult = Send(mauiBuffer, uiOut);
			if (uiResult != KERNAL_DRIVER_SUCCESS)
			{
				return uiResult;
			}
		}
		*puiDone += uiChunk;
		uiUserAddress += uiChunk;
		uiLength -= uiChunk;
	}
	if (mbVideo)
	{
		if (muiColumn >= muiColumns)
		{
			VideoNewLine();
		}
		VideoCursor();
	}
	return KERNAL_DRIVER_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////
//
// What the keyboard has sent, as characters into macTyped (while there is
// room).  Caps Lock's LED follows it: ED and its argument in one write
// (the driver waits for the line to be quiet before each byte); the
// keyboard's answers (FA) are dropped with the keys.
//
//////////////////////////////////////////////////////////////////////////
void CKernalConsole::ReadKeyboard(void)
{
	uint8	auiCodes[KERNAL_CONSOLE_BUFFER_SIZE];
	uint8	auiCommand[2];
	char	szChars[KERNAL_KEYBOARD_MAX_CHARS + 1];
	uint32	uiRead;
	uint32	uiDone;
	uint32	i;
	uint8	uiChars;
	uint8	j;

	for (;;)
	{
		if (muiTyped + KERNAL_KEYBOARD_MAX_CHARS > KERNAL_CONSOLE_TYPED_SIZE)
		{
			return;
		}
		uiRead = 0;
		KernalSetKernalCopy(true);
		mpcInput->Read(muiInputUnit, 0, (uint32)auiCodes, (KERNAL_CONSOLE_TYPED_SIZE - muiTyped) / KERNAL_KEYBOARD_MAX_CHARS, &uiRead);
		KernalSetKernalCopy(false);
		if (uiRead == 0)
		{
			return;
		}
		for (i = 0; i < uiRead; i++)
		{
			uiChars = mcKeyboard.Decode(auiCodes[i], szChars);
			for (j = 0; j < uiChars; j++)
			{
				macTyped[muiTyped++] = szChars[j];
			}
		}
		if (mcKeyboard.IsCapsLock() != mbCapsLock)
		{
			mbCapsLock = mcKeyboard.IsCapsLock();
			auiCommand[0] = PS2_KEYBOARD_SET_LEDS;
			auiCommand[1] = mbCapsLock ? PS2_KEYBOARD_LED_CAPS : 0;
			uiDone = 0;
			KernalSetKernalCopy(true);
			mpcInput->Write(muiInputUnit, 0, (uint32)auiCommand, 2, &uiDone);
			KernalSetKernalCopy(false);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// What has been typed, up to uiLength bytes, to the running process's
// memory at uiUserAddress, without waiting: *puiDone may be 0.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalConsole::Read(uint32 uiUserAddress, uint32 uiLength, uint32* puiDone)
{
	uint32	uiChunk;
	uint32	uiRead;
	uint16	uiResult;

	Take();
	if (!mpcInput)
	{
		return KERNAL_DRIVER_SUCCESS;
	}

	if (mbKeyboard)
	{
		ReadKeyboard();
		uiChunk = (uiLength < muiTyped) ? uiLength : muiTyped;
		if ((uiChunk > 0) && !KernalCopyToUser(uiUserAddress, (uint8*)macTyped, uiChunk))
		{
			return KERNAL_DRIVER_BAD_ARGUMENT;
		}
		memmove(macTyped, &macTyped[uiChunk], muiTyped - uiChunk);
		muiTyped = (uint16)(muiTyped - uiChunk);
		*puiDone += uiChunk;
		return KERNAL_DRIVER_SUCCESS;
	}

	uiChunk = (uiLength < KERNAL_CONSOLE_BUFFER_SIZE) ? uiLength : KERNAL_CONSOLE_BUFFER_SIZE;
	uiRead = 0;
	KernalSetKernalCopy(true);
	uiResult = mpcInput->Read(muiInputUnit, 0, (uint32)mauiBuffer, uiChunk, &uiRead);
	KernalSetKernalCopy(false);
	if ((uiRead > 0) && !KernalCopyToUser(uiUserAddress, mauiBuffer, uiRead))
	{
		return KERNAL_DRIVER_BAD_ARGUMENT;
	}
	*puiDone += uiRead;
	return uiResult;
}


//////////////////////////////////////////////////////////////////////////
//
// HCP65User.h's KERNAL_CONSOLE_* codes.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalConsole::Control(uint16 uiCode, uint32 uiValue, uint32* puiResult)
{
	Take();
	switch (uiCode)
	{
	case KERNAL_CONSOLE_GET_COLUMNS:
		*puiResult = mbVideo ? muiColumns : (mpcOutput ? KERNAL_CONSOLE_UART_COLUMNS : 0);
		return KERNAL_DRIVER_SUCCESS;

	case KERNAL_CONSOLE_GET_ROWS:
		*puiResult = mbVideo ? muiRows : (mpcOutput ? KERNAL_CONSOLE_UART_ROWS : 0);
		return KERNAL_DRIVER_SUCCESS;

	case KERNAL_CONSOLE_CLEAR:
		if (mbVideo)
		{
			muiColumn = 0;
			muiRow = 0;
			VideoControl(VIDEO_BOARD_CLEAR_TEXT, 0, NULL);
			VideoCursor();
			return KERNAL_DRIVER_SUCCESS;
		}
		//A VT100's: erase the screen, then the cursor to the top left.
		return mpcOutput ? SendText("\x1B[2J\x1B[H") : KERNAL_DRIVER_SUCCESS;

	case KERNAL_CONSOLE_SET_MODE:
		if (!mbVideo || (uiValue >= VIDEO_NUM_MODES))
		{
			return KERNAL_DRIVER_BAD_ARGUMENT;
		}
		return VideoMode(uiValue);

	case KERNAL_CONSOLE_SET_COLOUR:
		if (uiValue >= VIDEO_TEXT_CURSOR_PALETTE)
		{
			return KERNAL_DRIVER_BAD_ARGUMENT;
		}
		return mbVideo ? VideoControl(VIDEO_BOARD_SET_TEXT_COLOUR, uiValue, NULL) : KERNAL_DRIVER_SUCCESS;

	default:
		return KERNAL_DRIVER_NOT_SUPPORTED;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Waits up to uiTicks for KERNAL_CONSOLE_WAIT_INPUT: something typed.
// KERNAL_DRIVER_TIMEOUT if nothing was (or there is no input).
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalConsole::Wait(uint32 uiValue, uint16 uiTicks)
{
	if (uiValue != KERNAL_CONSOLE_WAIT_INPUT)
	{
		return KERNAL_DRIVER_BAD_ARGUMENT;
	}
	Take();
	if (!mpcInput)
	{
		return KERNAL_DRIVER_TIMEOUT;
	}
	if (mbKeyboard)
	{
		ReadKeyboard();
		if (muiTyped > 0)
		{
			return KERNAL_DRIVER_SUCCESS;
		}
		return mpcInput->Wait(muiInputUnit, 1, uiTicks);
	}
	return mpcInput->Wait(muiInputUnit, UART_WAIT_RX | 1, uiTicks);
}
