#include <string.h>
#include "HCP65User.h"
#include "HCP65VIABoard.h"


//TestHCP65KernalVIA runs this (\viatest.exe) as its start up program with a VIA Board that has breakouts 0 and 2 (AB, CD, XJ and KL), and plays
//what is on the VIAs' lines (HCP65Lib's CHCP65VIABoard): before the boot it drives XJ's port A with $77 and its port B
//with $C3, and it takes XJ's CA1 low once XJ's CA1 interrupt is enabled.
#define VIA_TEST_PATH			"\\viatest.exe"
#define VIA_TEST_CHILD			0xC41D		//The argument it runs itself with as the child.
#define VIA_TEST_MAX_BOARDS		20
#define VIA_TEST_FITTED			0x33		//AB, CD, XJ and KL.
#define VIA_TEST_T1				50000		//4ms of PHI2.
#define VIA_TEST_WAIT_TICKS		KERNAL_DEVICE_TICKS_PER_SECOND


uint8	gauiVIATestBuffer[VIA_NUM_REGISTERS];


//////////////////////////////////////////////////////////////////////////
//
// The child, while the parent has AB and XJ open: XJ is in use, CD isn't.
// It drives CD's port B with $A5 and exits without closing it; the kernal
// closes it, which makes every pin an input again.
//
//////////////////////////////////////////////////////////////////////////
uint16 VIATestChild(void)
{
	uint16	uiHandle;

	if (KernalDeviceOpen(VIA_BOARD_MANUFACTURER, VIA_BOARD_NAME, 0, VIA_BOARD_XJ, &uiHandle) != KERNAL_DEVICE_IN_USE)
	{
		return 1;
	}
	if (KernalDeviceOpen(VIA_BOARD_MANUFACTURER, VIA_BOARD_NAME, 0, VIA_BOARD_CD, &uiHandle) != KERNAL_DEVICE_SUCCESS)
	{
		return 2;
	}
	gauiVIATestBuffer[0] = 0xA5;
	gauiVIATestBuffer[1] = 0x00;
	gauiVIATestBuffer[2] = 0xFF;
	if (KernalDeviceWrite(uiHandle, VIA_ORB, gauiVIATestBuffer, 3, NULL) != KERNAL_DEVICE_SUCCESS)
	{
		return 3;
	}
	return 0;
}


//////////////////////////////////////////////////////////////////////////
//
// The board is there with its eight units, of which four are fitted; the
// handles and arguments the driver refuses.
//
//////////////////////////////////////////////////////////////////////////
uint16 VIATestOpen(uint16* puiAB, uint16* puiXJ)
{
	SKernalDeviceInfo	sInfo;
	uint16				uiIndex;
	uint16				uiHandle;
	uint32				uiResult;

	for (uiIndex = 0; uiIndex < VIA_TEST_MAX_BOARDS; uiIndex++)
	{
		if (KernalDeviceInfo(uiIndex, &sInfo) != KERNAL_DEVICE_SUCCESS)
		{
			return 1;
		}
		if ((strcmp(sInfo.szManufacturer, VIA_BOARD_MANUFACTURER) == 0) && (strcmp(sInfo.szName, VIA_BOARD_NAME) == 0))
		{
			break;
		}
	}
	if ((sInfo.uiNumUnits != VIA_BOARD_NUM_VIAS) || sInfo.uiOpen)
	{
		return 2;
	}
	if ((KernalDeviceOpen(VIA_BOARD_MANUFACTURER, VIA_BOARD_NAME, 0, VIA_BOARD_NUM_VIAS, &uiHandle) != KERNAL_DEVICE_BAD_UNIT) ||
		(KernalDeviceOpen(VIA_BOARD_MANUFACTURER, VIA_BOARD_NAME, 0, VIA_BOARD_EF, &uiHandle) != KERNAL_DEVICE_BAD_UNIT))
	{
		return 3;
	}
	if ((KernalDeviceOpen(VIA_BOARD_MANUFACTURER, VIA_BOARD_NAME, 0, VIA_BOARD_AB, puiAB) != KERNAL_DEVICE_SUCCESS) ||
		(KernalDeviceOpen(VIA_BOARD_MANUFACTURER, VIA_BOARD_NAME, 0, VIA_BOARD_XJ, puiXJ) != KERNAL_DEVICE_SUCCESS))
	{
		return 4;
	}
	if (!KernalDeviceIsOpen(VIA_BOARD_MANUFACTURER, VIA_BOARD_NAME, 0))
	{
		return 5;
	}
	if ((KernalDeviceControl(*puiAB, VIA_BOARD_GET_FITTED, 0, &uiResult) != KERNAL_DEVICE_SUCCESS) || (uiResult != VIA_TEST_FITTED))
	{
		return 6;
	}

	//Codes, registers and waits the driver doesn't take.  IER is the driver's: it can be read, not written.
	gauiVIATestBuffer[0] = 0;
	gauiVIATestBuffer[1] = 0;
	if ((KernalDeviceControl(*puiAB, 99, 0, &uiResult) != KERNAL_DEVICE_NOT_SUPPORTED) ||
		(KernalDeviceWrite(*puiAB, VIA_IER, gauiVIATestBuffer, 1, NULL) != KERNAL_DEVICE_BAD_ARGUMENT) ||
		(KernalDeviceWrite(*puiAB, VIA_IFR, gauiVIATestBuffer, 2, NULL) != KERNAL_DEVICE_BAD_ARGUMENT) ||
		(KernalDeviceRead(*puiAB, VIA_ORA_NH, gauiVIATestBuffer, 2, NULL) != KERNAL_DEVICE_BAD_ARGUMENT) ||
		(KernalDeviceRead(*puiAB, VIA_NUM_REGISTERS, gauiVIATestBuffer, 1, NULL) != KERNAL_DEVICE_BAD_ARGUMENT) ||
		(KernalDeviceWait(*puiAB, 0, 1) != KERNAL_DEVICE_BAD_ARGUMENT) ||
		(KernalDeviceWait(*puiAB, VIA_INT_ANY, 1) != KERNAL_DEVICE_BAD_ARGUMENT))
	{
		return 7;
	}
	uiResult = 0;
	if ((KernalDeviceRead(*puiAB, VIA_IER, gauiVIATestBuffer, 1, &uiResult) != KERNAL_DEVICE_SUCCESS) || (uiResult != 1) || (gauiVIATestBuffer[0] != VIA_INT_ANY))
	{
		return 8;
	}
	return 0;
}


//////////////////////////////////////////////////////////////////////////
//
// XJ's port B reads what is on its pins; AB's port A, all outputs, drives
// it inverted.
//
//////////////////////////////////////////////////////////////////////////
uint16 VIATestPorts(uint16 uiAB, uint16 uiXJ)
{
	uint32	uiDone;

	uiDone = 0;
	if ((KernalDeviceRead(uiXJ, VIA_ORB, gauiVIATestBuffer, 1, &uiDone) != KERNAL_DEVICE_SUCCESS) || (uiDone != 1) || (gauiVIATestBuffer[0] != 0xC3))
	{
		return 1;
	}

	//DDRB and DDRA in one write, then ORA.
	gauiVIATestBuffer[1] = (uint8)~gauiVIATestBuffer[0];
	gauiVIATestBuffer[2] = 0x00;
	gauiVIATestBuffer[3] = 0xFF;
	uiDone = 0;
	if ((KernalDeviceWrite(uiAB, VIA_DDRB, &gauiVIATestBuffer[2], 2, &uiDone) != KERNAL_DEVICE_SUCCESS) || (uiDone != 2))
	{
		return 2;
	}
	if (KernalDeviceWrite(uiAB, VIA_ORA, &gauiVIATestBuffer[1], 1, NULL) != KERNAL_DEVICE_SUCCESS)
	{
		return 3;
	}
	if ((KernalDeviceRead(uiAB, VIA_ORB, gauiVIATestBuffer, 4, NULL) != KERNAL_DEVICE_SUCCESS) || (gauiVIATestBuffer[1] != 0x3C) || (gauiVIATestBuffer[3] != 0xFF))
	{
		return 4;
	}
	return 0;
}


//////////////////////////////////////////////////////////////////////////
//
// AB's one shot T1: a wait for its flag ends when it times out, and
// leaves the flag set; reading T1C-L clears it.  It doesn't time out
// again, so a second wait times out.
//
//////////////////////////////////////////////////////////////////////////
uint16 VIATestTimer(uint16 uiAB)
{
	gauiVIATestBuffer[0] = (uint8)VIA_TEST_T1;
	gauiVIATestBuffer[1] = (uint8)(VIA_TEST_T1 >> 8);
	if (KernalDeviceWrite(uiAB, VIA_T1CL, gauiVIATestBuffer, 2, NULL) != KERNAL_DEVICE_SUCCESS)
	{
		return 1;
	}
	if (KernalDeviceWait(uiAB, VIA_INT_T1, VIA_TEST_WAIT_TICKS) != KERNAL_DEVICE_SUCCESS)
	{
		return 2;
	}
	if ((KernalDeviceRead(uiAB, VIA_IFR, gauiVIATestBuffer, 2, NULL) != KERNAL_DEVICE_SUCCESS) || ((gauiVIATestBuffer[0] & VIA_INT_T1) == 0) || (gauiVIATestBuffer[1] != VIA_INT_ANY))
	{
		return 3;
	}
	if ((KernalDeviceRead(uiAB, VIA_T1CL, gauiVIATestBuffer, 1, NULL) != KERNAL_DEVICE_SUCCESS) ||
		(KernalDeviceRead(uiAB, VIA_IFR, gauiVIATestBuffer, 1, NULL) != KERNAL_DEVICE_SUCCESS) || (gauiVIATestBuffer[0] & VIA_INT_T1))
	{
		return 4;
	}
	if (KernalDeviceWait(uiAB, VIA_INT_T1, 2) != KERNAL_DEVICE_TIMEOUT)
	{
		return 5;
	}
	return 0;
}


//////////////////////////////////////////////////////////////////////////
//
// XJ's CA1 on its negative edge: the test takes CA1 low once the wait has
// turned its interrupt on.  Reading ORA clears the flag and reads $77.
//
//////////////////////////////////////////////////////////////////////////
uint16 VIATestHandshake(uint16 uiXJ)
{
	gauiVIATestBuffer[0] = VIA_PCR_CA2_INPUT_NEGATIVE;		//And CA1's negative edge.
	if (KernalDeviceWrite(uiXJ, VIA_PCR, gauiVIATestBuffer, 1, NULL) != KERNAL_DEVICE_SUCCESS)
	{
		return 1;
	}
	if (KernalDeviceWait(uiXJ, VIA_INT_CA1 | VIA_INT_CB1, VIA_TEST_WAIT_TICKS) != KERNAL_DEVICE_SUCCESS)
	{
		return 2;
	}
	if ((KernalDeviceRead(uiXJ, VIA_IFR, gauiVIATestBuffer, 1, NULL) != KERNAL_DEVICE_SUCCESS) || (gauiVIATestBuffer[0] != VIA_INT_CA1))
	{
		return 3;
	}
	if ((KernalDeviceRead(uiXJ, VIA_ORA, gauiVIATestBuffer, 1, NULL) != KERNAL_DEVICE_SUCCESS) || (gauiVIATestBuffer[0] != 0x77))
	{
		return 4;
	}
	if ((KernalDeviceRead(uiXJ, VIA_IFR, gauiVIATestBuffer, 1, NULL) != KERNAL_DEVICE_SUCCESS) || (gauiVIATestBuffer[0] != 0))
	{
		return 5;
	}
	return 0;
}


//////////////////////////////////////////////////////////////////////////
//
// The child can have CD but not XJ.
//
//////////////////////////////////////////////////////////////////////////
uint16 VIATestChildOwnership(void)
{
	uint16	uiExitCode;

	uiExitCode = 0xFFFF;
	if (KernalExecute(VIA_TEST_PATH, VIA_TEST_CHILD, &uiExitCode) != KERNAL_RESULT_EXITED)
	{
		return 1;
	}
	if (uiExitCode != 0)
	{
		return 10 + uiExitCode;
	}
	return 0;
}


//////////////////////////////////////////////////////////////////////////
//
// The VIA Board driver's test (TestHCP65KernalVIA runs it).
// Exits 0 if everything did what it should, otherwise 100 times the step
// that failed plus which check in it.  Run with VIA_TEST_CHILD it is the
// child (VIATestChild).
//
//////////////////////////////////////////////////////////////////////////
int main(void)
{
	uint16	uiAB;
	uint16	uiXJ;
	uint16	uiResult;

	if (KernalGetArgument() == VIA_TEST_CHILD)
	{
		return VIATestChild();
	}

	uiResult = VIATestOpen(&uiAB, &uiXJ);
	if (uiResult != 0)
	{
		return 100 + uiResult;
	}
	uiResult = VIATestPorts(uiAB, uiXJ);
	if (uiResult != 0)
	{
		return 200 + uiResult;
	}
	uiResult = VIATestTimer(uiAB);
	if (uiResult != 0)
	{
		return 300 + uiResult;
	}
	uiResult = VIATestHandshake(uiXJ);
	if (uiResult != 0)
	{
		return 400 + uiResult;
	}
	uiResult = VIATestChildOwnership();
	if (uiResult != 0)
	{
		return 500 + uiResult;
	}

	//Closing both lets the board go.
	if ((KernalDeviceClose(uiXJ) != KERNAL_DEVICE_SUCCESS) || !KernalDeviceIsOpen(VIA_BOARD_MANUFACTURER, VIA_BOARD_NAME, 0) || (KernalDeviceClose(uiAB) != KERNAL_DEVICE_SUCCESS) || KernalDeviceIsOpen(VIA_BOARD_MANUFACTURER, VIA_BOARD_NAME, 0))
	{
		return 600;
	}
	return 0;
}

