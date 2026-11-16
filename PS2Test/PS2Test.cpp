#include <string.h>
#include "HCP65User.h"
#include "HCP65PS2Board.h"


//TestHCP65KernalPS2 boots this as \boot.exe with a PS2 Board that has a keyboard (HCP65Lib's CHCP65PS2Keyboard) in
//port 1 and nothing in port 2.  Once the keyboard is scanning with its Num Lock LED on (after PS2TestCommands) it
//presses and lets go of A, then Cursor Up.
#define PS2_TEST_PATH			"\\boot.exe"
#define PS2_TEST_CHILD			0xC41D		//The argument it runs itself with as the child.
#define PS2_TEST_MAX_BOARDS		20
#define PS2_TEST_WAIT_TICKS		KERNAL_DEVICE_TICKS_PER_SECOND
#define PS2_TEST_KEYS			8			//A down and up, Cursor Up down and up.


uint8	gauiPS2TestBuffer[16];

static const uint8	gauiPS2TestKeys[PS2_TEST_KEYS] = { 0x1C, PS2_KEYBOARD_BREAK, 0x1C, PS2_KEYBOARD_EXTENDED, 0x75, PS2_KEYBOARD_EXTENDED, PS2_KEYBOARD_BREAK, 0x75 };


//////////////////////////////////////////////////////////////////////////
//
// The child, while the parent has port 1 open: port 1 is in use, port 2
// isn't.  It exits without closing port 2.
//
//////////////////////////////////////////////////////////////////////////
uint16 PS2TestChild(void)
{
	uint16	uiHandle;

	if (KernalDeviceOpen(PS2_BOARD_MANUFACTURER, PS2_BOARD_NAME, 0, PS2_BOARD_PORT_1, &uiHandle) != KERNAL_DEVICE_IN_USE)
	{
		return 1;
	}
	if (KernalDeviceOpen(PS2_BOARD_MANUFACTURER, PS2_BOARD_NAME, 0, PS2_BOARD_PORT_2, &uiHandle) != KERNAL_DEVICE_SUCCESS)
	{
		return 2;
	}
	return 0;
}


//////////////////////////////////////////////////////////////////////////
//
// uiCommand sent to the keyboard, then uiLength bytes waited for, which
// must be puiExpected.
//
//////////////////////////////////////////////////////////////////////////
uint16 PS2TestCommand(uint16 uiHandle, uint8 uiCommand, const uint8* puiExpected, uint16 uiLength)
{
	uint32	uiDone;

	uiDone = 0;
	if ((KernalDeviceWrite(uiHandle, 0, &uiCommand, 1, &uiDone) != KERNAL_DEVICE_SUCCESS) || (uiDone != 1))
	{
		return 1;
	}
	if (KernalDeviceWait(uiHandle, uiLength, PS2_TEST_WAIT_TICKS) != KERNAL_DEVICE_SUCCESS)
	{
		return 2;
	}
	uiDone = 0;
	if ((KernalDeviceRead(uiHandle, 0, gauiPS2TestBuffer, sizeof(gauiPS2TestBuffer), &uiDone) != KERNAL_DEVICE_SUCCESS) || (uiDone != uiLength))
	{
		return 3;
	}
	if (memcmp(gauiPS2TestBuffer, puiExpected, uiLength) != 0)
	{
		return 4;
	}
	return 0;
}


//////////////////////////////////////////////////////////////////////////
//
// The board is there with its two units; the handles and arguments the
// driver refuses.
//
//////////////////////////////////////////////////////////////////////////
uint16 PS2TestOpen(uint16* puiPort1, uint16* puiPort2)
{
	SKernalDeviceInfo	sInfo;
	uint16				uiIndex;
	uint16				uiHandle;
	uint32				uiResult;

	for (uiIndex = 0; uiIndex < PS2_TEST_MAX_BOARDS; uiIndex++)
	{
		if (KernalDeviceInfo(uiIndex, &sInfo) != KERNAL_DEVICE_SUCCESS)
		{
			return 1;
		}
		if ((strcmp(sInfo.szManufacturer, PS2_BOARD_MANUFACTURER) == 0) && (strcmp(sInfo.szName, PS2_BOARD_NAME) == 0))
		{
			break;
		}
	}
	if ((sInfo.uiNumUnits != 2) || sInfo.uiOpen)
	{
		return 2;
	}
	if (KernalDeviceOpen(PS2_BOARD_MANUFACTURER, PS2_BOARD_NAME, 0, 2, &uiHandle) != KERNAL_DEVICE_BAD_UNIT)
	{
		return 3;
	}
	if ((KernalDeviceOpen(PS2_BOARD_MANUFACTURER, PS2_BOARD_NAME, 0, PS2_BOARD_PORT_1, puiPort1) != KERNAL_DEVICE_SUCCESS) ||
		(KernalDeviceOpen(PS2_BOARD_MANUFACTURER, PS2_BOARD_NAME, 0, PS2_BOARD_PORT_2, puiPort2) != KERNAL_DEVICE_SUCCESS))
	{
		return 4;
	}
	if (!KernalDeviceIsOpen(PS2_BOARD_MANUFACTURER, PS2_BOARD_NAME, 0))
	{
		return 5;
	}
	if ((KernalDeviceControl(*puiPort1, 99, 0, &uiResult) != KERNAL_DEVICE_NOT_SUPPORTED) ||
		(KernalDeviceWait(*puiPort1, 0, 1) != KERNAL_DEVICE_BAD_ARGUMENT) ||
		(KernalDeviceWait(*puiPort1, PS2_BOARD_BUFFER_SIZE + 1, 1) != KERNAL_DEVICE_BAD_ARGUMENT))
	{
		return 6;
	}
	uiResult = 1;
	if ((KernalDeviceControl(*puiPort1, PS2_BOARD_GET_RX_COUNT, 0, &uiResult) != KERNAL_DEVICE_SUCCESS) || (uiResult != 0))
	{
		return 7;
	}
	return 0;
}


//////////////////////////////////////////////////////////////////////////
//
// Nothing is in port 2: a byte sent to it fails.
//
//////////////////////////////////////////////////////////////////////////
uint16 PS2TestNothing(uint16 uiPort2)
{
	uint32	uiDone;
	uint8	uiReset;

	uiReset = PS2_KEYBOARD_RESET;
	uiDone = 1;
	if ((KernalDeviceWrite(uiPort2, 0, &uiReset, 1, &uiDone) != KERNAL_DEVICE_FAILED) || (uiDone != 0))
	{
		return 1;
	}
	return 0;
}


//////////////////////////////////////////////////////////////////////////
//
// The keyboard reset, scanning disabled, Num Lock's LED on, identified
// and scanning enabled again.  None of the clocks the sends make reaches
// the reads.
//
//////////////////////////////////////////////////////////////////////////
uint16 PS2TestCommands(uint16 uiPort1)
{
	static const uint8	auiReset[] = { PS2_KEYBOARD_ACK, PS2_KEYBOARD_BAT_OK };
	static const uint8	auiAck[] = { PS2_KEYBOARD_ACK };
	static const uint8	auiIdentity[] = { PS2_KEYBOARD_ACK, 0xAB, 0x83 };
	uint16	uiResult;

	uiResult = PS2TestCommand(uiPort1, PS2_KEYBOARD_RESET, auiReset, sizeof(auiReset));
	if (uiResult != 0)
	{
		return 10 + uiResult;
	}
	uiResult = PS2TestCommand(uiPort1, PS2_KEYBOARD_DISABLE, auiAck, sizeof(auiAck));
	if (uiResult != 0)
	{
		return 20 + uiResult;
	}
	uiResult = PS2TestCommand(uiPort1, PS2_KEYBOARD_SET_LEDS, auiAck, sizeof(auiAck));
	if (uiResult != 0)
	{
		return 30 + uiResult;
	}
	uiResult = PS2TestCommand(uiPort1, PS2_KEYBOARD_LED_NUM, auiAck, sizeof(auiAck));
	if (uiResult != 0)
	{
		return 40 + uiResult;
	}
	uiResult = PS2TestCommand(uiPort1, PS2_KEYBOARD_IDENTIFY, auiIdentity, sizeof(auiIdentity));
	if (uiResult != 0)
	{
		return 50 + uiResult;
	}
	uiResult = PS2TestCommand(uiPort1, PS2_KEYBOARD_ENABLE, auiAck, sizeof(auiAck));
	if (uiResult != 0)
	{
		return 60 + uiResult;
	}
	return 0;
}


//////////////////////////////////////////////////////////////////////////
//
// The keys the test presses arrive as their scan codes.
//
//////////////////////////////////////////////////////////////////////////
uint16 PS2TestKeys(uint16 uiPort1)
{
	uint32	uiDone;
	uint32	uiOverruns;

	if (KernalDeviceWait(uiPort1, PS2_TEST_KEYS, 2 * PS2_TEST_WAIT_TICKS) != KERNAL_DEVICE_SUCCESS)
	{
		return 1;
	}
	uiDone = 0;
	if ((KernalDeviceRead(uiPort1, 0, gauiPS2TestBuffer, sizeof(gauiPS2TestBuffer), &uiDone) != KERNAL_DEVICE_SUCCESS) || (uiDone != PS2_TEST_KEYS))
	{
		return 2;
	}
	if (memcmp(gauiPS2TestBuffer, gauiPS2TestKeys, PS2_TEST_KEYS) != 0)
	{
		return 3;
	}
	uiOverruns = 1;
	if ((KernalDeviceControl(uiPort1, PS2_BOARD_GET_OVERRUNS, 0, &uiOverruns) != KERNAL_DEVICE_SUCCESS) || (uiOverruns != 0))
	{
		return 4;
	}
	return 0;
}


//////////////////////////////////////////////////////////////////////////
//
// With port 2 closed the child runs: it can have port 2 but not port 1.
//
//////////////////////////////////////////////////////////////////////////
uint16 PS2TestChildOwnership(uint16 uiPort2)
{
	uint16	uiExitCode;

	if (KernalDeviceClose(uiPort2) != KERNAL_DEVICE_SUCCESS)
	{
		return 1;
	}
	uiExitCode = 0xFFFF;
	if (KernalExecute(PS2_TEST_PATH, PS2_TEST_CHILD, &uiExitCode) != KERNAL_RESULT_EXITED)
	{
		return 2;
	}
	if (uiExitCode != 0)
	{
		return 10 + uiExitCode;
	}
	return 0;
}


//////////////////////////////////////////////////////////////////////////
//
// The PS2 Board driver's test (TestHCP65KernalPS2 boots it as \boot.exe).
// Exits 0 if everything did what it should, otherwise 100 times the step
// that failed plus which check in it.  Run with PS2_TEST_CHILD it is the
// child (PS2TestChild).
//
//////////////////////////////////////////////////////////////////////////
int main(void)
{
	uint16	uiPort1;
	uint16	uiPort2;
	uint16	uiResult;

	if (KernalGetArgument() == PS2_TEST_CHILD)
	{
		return PS2TestChild();
	}

	uiResult = PS2TestOpen(&uiPort1, &uiPort2);
	if (uiResult != 0)
	{
		return 100 + uiResult;
	}
	uiResult = PS2TestNothing(uiPort2);
	if (uiResult != 0)
	{
		return 200 + uiResult;
	}
	uiResult = PS2TestCommands(uiPort1);
	if (uiResult != 0)
	{
		return 300 + uiResult;
	}
	uiResult = PS2TestKeys(uiPort1);
	if (uiResult != 0)
	{
		return 400 + uiResult;
	}
	uiResult = PS2TestChildOwnership(uiPort2);
	if (uiResult != 0)
	{
		return 500 + uiResult;
	}

	//Closing it lets the board go.
	if ((KernalDeviceClose(uiPort1) != KERNAL_DEVICE_SUCCESS) || KernalDeviceIsOpen(PS2_BOARD_MANUFACTURER, PS2_BOARD_NAME, 0))
	{
		return 600;
	}
	return 0;
}

