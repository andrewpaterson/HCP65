#include <string.h>
#include "UserLib/HCP65User.h"
#include "UserLib/HCP65UARTBoard.h"
#include "KernalProcess.h"
#include "KernalConsole.h"


CKernalConsole	gcKernalConsole;


//////////////////////////////////////////////////////////////////////////
//
// Nothing taken yet.
//
//////////////////////////////////////////////////////////////////////////
void CKernalConsole::Init(void)
{
	mpcOutput = NULL;
	muiOutputUnit = 0;
	mpcInput = NULL;
	muiInputUnit = 0;
}


//////////////////////////////////////////////////////////////////////////
//
// The boards' units, if they aren't taken already and are there to take:
// the UART Board's TTL port, both ways.
//
//////////////////////////////////////////////////////////////////////////
void CKernalConsole::Take(void)
{
	CKernalDriver*	pcUART;

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
// uiLength bytes of kernal memory to the output, waiting while it is
// busy.
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

	//The process's bytes go in the buffer's second half, and out with each '\n' sent as "\r\n" from its start.
	puiIn = &mauiBuffer[KERNAL_CONSOLE_BUFFER_SIZE];
	while (uiLength > 0)
	{
		uiChunk = (uiLength < KERNAL_CONSOLE_BUFFER_SIZE / 2) ? uiLength : KERNAL_CONSOLE_BUFFER_SIZE / 2;
		if (!KernalCopyFromUser(puiIn, uiUserAddress, uiChunk))
		{
			return KERNAL_DRIVER_BAD_ARGUMENT;
		}
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
		*puiDone += uiChunk;
		uiUserAddress += uiChunk;
		uiLength -= uiChunk;
	}
	return KERNAL_DRIVER_SUCCESS;
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
		*puiResult = mpcOutput ? KERNAL_CONSOLE_UART_COLUMNS : 0;
		return KERNAL_DRIVER_SUCCESS;

	case KERNAL_CONSOLE_GET_ROWS:
		*puiResult = mpcOutput ? KERNAL_CONSOLE_UART_ROWS : 0;
		return KERNAL_DRIVER_SUCCESS;

	case KERNAL_CONSOLE_CLEAR:
		//A VT100's: erase the screen, then the cursor to the top left.
		return mpcOutput ? SendText("\x1B[2J\x1B[H") : KERNAL_DRIVER_SUCCESS;

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
	return mpcInput->Wait(muiInputUnit, UART_WAIT_RX | 1, uiTicks);
}
