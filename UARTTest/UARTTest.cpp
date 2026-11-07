#include <string.h>
#include "HCP65User.h"
#include "HCP65UARTBoard.h"


//TestHCP65KernalUART boots this as \boot.exe with the UART Board, and plays the far end of both ports (HCP65Lib's
//CHCP65UARTBoard): it turns the RS232 port's CTS and DSR on and the TTL port's CTS on before the boot.
#define UART_TEST_PATH			"\\boot.exe"
#define UART_TEST_CHILD			0xC41D		//The argument it runs itself with as the child.
#define UART_TEST_MAX_BOARDS	20

#define UART_TEST_SENT			1000		//The pattern sent on the RS232 port: 3, then 7 more each byte.
#define UART_TEST_RECEIVED		2000		//The pattern the far end sends back on it (5, then 13 more each byte), once it
											//has had "CHILD" and the first pattern.
#define UART_TEST_ECHOES		50			//Bytes the far end sends on the TTL port once it is at 9600 7E1; each comes back
											//xor $20.
#define UART_TEST_WAIT_TICKS	KERNAL_DEVICE_TICKS_PER_SECOND
#define UART_TEST_MAX_WAITS		20			//Seconds without a byte before it gives up.


uint8	gauiUARTTestBuffer[256];


//////////////////////////////////////////////////////////////////////////
//
// The child, while the parent has the TTL port open: the TTL port is in
// use, the RS232 port isn't.  It sends "CHILD" and exits without closing
// it; the kernal closes it, which waits for "CHILD" to go.
//
//////////////////////////////////////////////////////////////////////////
uint16 UARTTestChild(void)
{
	uint16	uiHandle;
	uint32	uiDone;

	if (KernalDeviceOpen(UART_BOARD_MANUFACTURER, UART_BOARD_NAME, 0, UART_BOARD_TTL, &uiHandle) != KERNAL_DEVICE_IN_USE)
	{
		return 1;
	}
	if (KernalDeviceOpen(UART_BOARD_MANUFACTURER, UART_BOARD_NAME, 0, UART_BOARD_RS232, &uiHandle) != KERNAL_DEVICE_SUCCESS)
	{
		return 2;
	}
	if ((KernalDeviceWrite(uiHandle, 0, "CHILD", 5, &uiDone) != KERNAL_DEVICE_SUCCESS) || (uiDone != 5))
	{
		return 3;
	}
	return 0;
}


//////////////////////////////////////////////////////////////////////////
//
// The board is there with its two units; the handles and arguments the
// driver refuses.
//
//////////////////////////////////////////////////////////////////////////
uint16 UARTTestOpen(uint16* puiTTL, uint16* puiRS232)
{
	SKernalDeviceInfo	sInfo;
	uint16				uiIndex;
	uint16				uiHandle;
	uint32				uiResult;

	for (uiIndex = 0; uiIndex < UART_TEST_MAX_BOARDS; uiIndex++)
	{
		if (KernalDeviceInfo(uiIndex, &sInfo) != KERNAL_DEVICE_SUCCESS)
		{
			return 1;
		}
		if ((strcmp(sInfo.szManufacturer, UART_BOARD_MANUFACTURER) == 0) && (strcmp(sInfo.szName, UART_BOARD_NAME) == 0))
		{
			break;
		}
	}
	if ((sInfo.uiNumUnits != 2) || sInfo.uiOpen)
	{
		return 2;
	}
	if (KernalDeviceOpen(UART_BOARD_MANUFACTURER, UART_BOARD_NAME, 0, 2, &uiHandle) != KERNAL_DEVICE_BAD_UNIT)
	{
		return 3;
	}
	if ((KernalDeviceOpen(UART_BOARD_MANUFACTURER, UART_BOARD_NAME, 0, UART_BOARD_TTL, puiTTL) != KERNAL_DEVICE_SUCCESS) ||
		(KernalDeviceOpen(UART_BOARD_MANUFACTURER, UART_BOARD_NAME, 0, UART_BOARD_RS232, puiRS232) != KERNAL_DEVICE_SUCCESS))
	{
		return 4;
	}
	if (!KernalDeviceIsOpen(UART_BOARD_MANUFACTURER, UART_BOARD_NAME, 0))
	{
		return 5;
	}

	//Codes and values the driver doesn't take.
	if ((KernalDeviceControl(*puiTTL, 99, 0, &uiResult) != KERNAL_DEVICE_NOT_SUPPORTED) ||
		(KernalDeviceControl(*puiTTL, UART_BOARD_SET_DIVISOR, 0, NULL) != KERNAL_DEVICE_BAD_ARGUMENT) ||
		(KernalDeviceControl(*puiTTL, UART_BOARD_SET_FORMAT, 0x40, NULL) != KERNAL_DEVICE_BAD_ARGUMENT) ||
		(KernalDeviceWait(*puiTTL, UART_WAIT_RX, 1) != KERNAL_DEVICE_BAD_ARGUMENT) ||
		(KernalDeviceWait(*puiTTL, UART_WAIT_RX | 256, 1) != KERNAL_DEVICE_BAD_ARGUMENT))
	{
		return 6;
	}

	//Nothing has arrived: a read takes nothing, and a wait for a byte times out.
	uiResult = 1;
	if ((KernalDeviceRead(*puiTTL, 0, gauiUARTTestBuffer, 10, &uiResult) != KERNAL_DEVICE_SUCCESS) || (uiResult != 0))
	{
		return 7;
	}
	if (KernalDeviceWait(*puiTTL, UART_WAIT_RX | 1, 2) != KERNAL_DEVICE_TIMEOUT)
	{
		return 8;
	}
	return 0;
}


//////////////////////////////////////////////////////////////////////////
//
// The TTL port in loopback: what it sends comes back, and its RTS is its
// CTS.
//
//////////////////////////////////////////////////////////////////////////
uint16 UARTTestLoopback(uint16 uiTTL)
{
	uint32	uiDone;
	uint32	uiLines;

	if (KernalDeviceControl(uiTTL, UART_BOARD_SET_LOOPBACK, 1, NULL) != KERNAL_DEVICE_SUCCESS)
	{
		return 1;
	}
	if ((KernalDeviceWrite(uiTTL, 0, "LOOP", 4, &uiDone) != KERNAL_DEVICE_SUCCESS) || (uiDone != 4))
	{
		return 2;
	}
	if (KernalDeviceWait(uiTTL, UART_WAIT_RX | 4, UART_TEST_WAIT_TICKS) != KERNAL_DEVICE_SUCCESS)
	{
		return 3;
	}
	if ((KernalDeviceRead(uiTTL, 0, gauiUARTTestBuffer, sizeof(gauiUARTTestBuffer), &uiDone) != KERNAL_DEVICE_SUCCESS) || (uiDone != 4) || (memcmp(gauiUARTTestBuffer, "LOOP", 4) != 0))
	{
		return 4;
	}
	if ((KernalDeviceControl(uiTTL, UART_BOARD_GET_LINES, 0, &uiLines) != KERNAL_DEVICE_SUCCESS) || (uiLines != UART_LINE_CTS))
	{
		return 5;
	}
	if ((KernalDeviceControl(uiTTL, UART_BOARD_SET_LINES, UART_LINE_DTR, NULL) != KERNAL_DEVICE_SUCCESS) ||
		(KernalDeviceControl(uiTTL, UART_BOARD_GET_LINES, 0, &uiLines) != KERNAL_DEVICE_SUCCESS) || (uiLines != 0))
	{
		return 6;
	}
	if ((KernalDeviceControl(uiTTL, UART_BOARD_SET_LINES, UART_LINE_DTR | UART_LINE_RTS, NULL) != KERNAL_DEVICE_SUCCESS) ||
		(KernalDeviceControl(uiTTL, UART_BOARD_SET_LOOPBACK, 0, NULL) != KERNAL_DEVICE_SUCCESS))
	{
		return 7;
	}
	return 0;
}


//////////////////////////////////////////////////////////////////////////
//
// With the RS232 port closed the child runs: it can have the RS232 port
// but not the TTL port.  The port is free again once it has exited.
//
//////////////////////////////////////////////////////////////////////////
uint16 UARTTestChildOwnership(uint16 uiRS232, uint16* puiRS232)
{
	uint16	uiExitCode;
	uint32	uiLines;

	if ((KernalDeviceClose(uiRS232) != KERNAL_DEVICE_SUCCESS) || !KernalDeviceIsOpen(UART_BOARD_MANUFACTURER, UART_BOARD_NAME, 0))
	{
		return 1;
	}
	uiExitCode = 0xFFFF;
	if (KernalExecute(UART_TEST_PATH, UART_TEST_CHILD, &uiExitCode) != KERNAL_RESULT_EXITED)
	{
		return 2;
	}
	if (uiExitCode != 0)
	{
		return 10 + uiExitCode;
	}
	if (KernalDeviceOpen(UART_BOARD_MANUFACTURER, UART_BOARD_NAME, 0, UART_BOARD_RS232, puiRS232) != KERNAL_DEVICE_SUCCESS)
	{
		return 3;
	}

	//The far end has RTS (our CTS) and DTR (our DSR) on, and DCD off.
	if ((KernalDeviceControl(*puiRS232, UART_BOARD_GET_LINES, 0, &uiLines) != KERNAL_DEVICE_SUCCESS) || (uiLines != (UART_LINE_CTS | UART_LINE_DSR)))
	{
		return 4;
	}
	return 0;
}


//////////////////////////////////////////////////////////////////////////
//
// With RTS/CTS on, the RS232 port sends the first pattern in one write
// (more than the driver buffers), then takes the far end's, which it
// reads a full buffer at a time so that the far end is held off.
//
//////////////////////////////////////////////////////////////////////////
uint16 UARTTestStream(uint16 uiRS232)
{
	uint8	uiValue;
	uint32	uiDone;
	uint32	uiCount;
	uint32	uiErrors;
	uint16	uiWaits;
	uint16	i;
	uint16	uiSent;
	uint16	uiReceived;

	if (KernalDeviceControl(uiRS232, UART_BOARD_SET_FLOW, UART_FLOW_RTS_CTS, NULL) != KERNAL_DEVICE_SUCCESS)
	{
		return 1;
	}

	uiValue = 3;
	for (uiSent = 0; uiSent < UART_TEST_SENT; uiSent += uiDone)
	{
		uiCount = ((UART_TEST_SENT - uiSent) < sizeof(gauiUARTTestBuffer)) ? (UART_TEST_SENT - uiSent) : sizeof(gauiUARTTestBuffer);
		for (i = 0; i < uiCount; i++)
		{
			gauiUARTTestBuffer[i] = uiValue;
			uiValue = (uint8)(uiValue + 7);
		}
		if ((KernalDeviceWrite(uiRS232, 0, gauiUARTTestBuffer, uiCount, &uiDone) != KERNAL_DEVICE_SUCCESS) || (uiDone != uiCount))
		{
			return 2;
		}
	}
	if (KernalDeviceWait(uiRS232, UART_WAIT_TX_EMPTY, UART_TEST_WAIT_TICKS) != KERNAL_DEVICE_SUCCESS)
	{
		return 3;
	}
	if ((KernalDeviceControl(uiRS232, UART_BOARD_GET_TX_COUNT, 0, &uiCount) != KERNAL_DEVICE_SUCCESS) || (uiCount != 0))
	{
		return 4;
	}

	uiValue = 5;
	uiWaits = 0;
	for (uiReceived = 0; uiReceived < UART_TEST_RECEIVED; uiReceived += uiDone)
	{
		if (KernalDeviceWait(uiRS232, UART_WAIT_RX | UART_BOARD_BUFFER_SIZE, UART_TEST_WAIT_TICKS) != KERNAL_DEVICE_SUCCESS)
		{
			uiWaits++;
			if (uiWaits > UART_TEST_MAX_WAITS)
			{
				return 5;
			}
		}
		if (KernalDeviceRead(uiRS232, 0, gauiUARTTestBuffer, sizeof(gauiUARTTestBuffer), &uiDone) != KERNAL_DEVICE_SUCCESS)
		{
			return 6;
		}
		for (i = 0; i < uiDone; i++)
		{
			if (gauiUARTTestBuffer[i] != uiValue)
			{
				return 7;
			}
			uiValue = (uint8)(uiValue + 13);
		}
	}
	if (uiReceived != UART_TEST_RECEIVED)
	{
		return 8;
	}
	if ((KernalDeviceControl(uiRS232, UART_BOARD_GET_ERRORS, 0, &uiErrors) != KERNAL_DEVICE_SUCCESS) || (uiErrors != 0))
	{
		return 9;
	}
	return 0;
}


//////////////////////////////////////////////////////////////////////////
//
// The TTL port at 9600 7E1, without flow control, echoes each byte the far
// end sends xor $20.
//
//////////////////////////////////////////////////////////////////////////
uint16 UARTTestEcho(uint16 uiTTL)
{
	uint32	uiDone;
	uint32	uiErrors;
	uint16	uiEchoed;
	uint16	uiWaits;
	uint16	i;

	if ((KernalDeviceControl(uiTTL, UART_BOARD_SET_DIVISOR, UART_BOARD_DIVISOR(9600), NULL) != KERNAL_DEVICE_SUCCESS) ||
		(KernalDeviceControl(uiTTL, UART_BOARD_SET_FORMAT, UART_FORMAT_DATA_7 | UART_FORMAT_PARITY_EVEN | UART_FORMAT_STOP_1, NULL) != KERNAL_DEVICE_SUCCESS))
	{
		return 1;
	}

	uiWaits = 0;
	for (uiEchoed = 0; uiEchoed < UART_TEST_ECHOES; uiEchoed += uiDone)
	{
		if (KernalDeviceWait(uiTTL, UART_WAIT_RX | 1, UART_TEST_WAIT_TICKS) != KERNAL_DEVICE_SUCCESS)
		{
			uiWaits++;
			if (uiWaits > UART_TEST_MAX_WAITS)
			{
				return 2;
			}
		}
		if (KernalDeviceRead(uiTTL, 0, gauiUARTTestBuffer, sizeof(gauiUARTTestBuffer), &uiDone) != KERNAL_DEVICE_SUCCESS)
		{
			return 3;
		}
		for (i = 0; i < uiDone; i++)
		{
			gauiUARTTestBuffer[i] ^= 0x20;
		}
		if (KernalDeviceWrite(uiTTL, 0, gauiUARTTestBuffer, uiDone, NULL) != KERNAL_DEVICE_SUCCESS)
		{
			return 4;
		}
	}
	if (KernalDeviceWait(uiTTL, UART_WAIT_TX_EMPTY, UART_TEST_WAIT_TICKS) != KERNAL_DEVICE_SUCCESS)
	{
		return 5;
	}
	if ((KernalDeviceControl(uiTTL, UART_BOARD_GET_ERRORS, 0, &uiErrors) != KERNAL_DEVICE_SUCCESS) || (uiErrors != 0))
	{
		return 6;
	}
	return 0;
}


//////////////////////////////////////////////////////////////////////////
//
// The UART Board driver's test (TestHCP65KernalUART boots it as
// \boot.exe).  Exits 0 if everything did what it should, otherwise 100
// times the step that failed plus which check in it.  Run with
// UART_TEST_CHILD it is the child (UARTTestChild).
//
//////////////////////////////////////////////////////////////////////////
int main(void)
{
	uint16	uiTTL;
	uint16	uiRS232;
	uint16	uiResult;

	if (KernalGetArgument() == UART_TEST_CHILD)
	{
		return UARTTestChild();
	}

	uiResult = UARTTestOpen(&uiTTL, &uiRS232);
	if (uiResult != 0)
	{
		return 100 + uiResult;
	}
	uiResult = UARTTestLoopback(uiTTL);
	if (uiResult != 0)
	{
		return 200 + uiResult;
	}
	uiResult = UARTTestChildOwnership(uiRS232, &uiRS232);
	if (uiResult != 0)
	{
		return 300 + uiResult;
	}
	uiResult = UARTTestStream(uiRS232);
	if (uiResult != 0)
	{
		return 400 + uiResult;
	}
	uiResult = UARTTestEcho(uiTTL);
	if (uiResult != 0)
	{
		return 500 + uiResult;
	}

	//Closing both lets the board go.
	if ((KernalDeviceClose(uiTTL) != KERNAL_DEVICE_SUCCESS) || !KernalDeviceIsOpen(UART_BOARD_MANUFACTURER, UART_BOARD_NAME, 0) || (KernalDeviceClose(uiRS232) != KERNAL_DEVICE_SUCCESS) || KernalDeviceIsOpen(UART_BOARD_MANUFACTURER, UART_BOARD_NAME, 0))
	{
		return 600;
	}
	return 0;
}
