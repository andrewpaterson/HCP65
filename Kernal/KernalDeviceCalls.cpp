#include <string.h>
#include "UserLib/HCP65User.h"
#include "KernalDriver.h"
#include "Kernal.h"
#include "KernalProcess.h"
#include "KernalConsole.h"
#include "KernalDeviceCalls.h"


//KERNAL_DEVICE_SUCCESS to KERNAL_DEVICE_TIMEOUT are a driver's KERNAL_DRIVER_* results, passed on as they are.
#define KERNAL_DEVICE_CALL_RESULT	20		//SKernalDeviceCall's uiResult.


//A handle: unit uiUnit of the board in uiSlot, open by uiProcess (0 when the handle is free).
struct SKernalHandle
{
	uint8	uiProcess;
	uint8	uiSlot;
	uint16	uiUnit;
};


static SKernalHandle	gasKernalHandles[KERNAL_MAX_HANDLES];


//////////////////////////////////////////////////////////////////////////
//
// The handle table's entry for uiHandle (KERNAL_FIRST_DEVICE_HANDLE on),
// NULL if it isn't one.
//
//////////////////////////////////////////////////////////////////////////
static SKernalHandle* KernalGetHandle(uint16 uiHandle)
{
	if ((uiHandle < KERNAL_FIRST_DEVICE_HANDLE) || (uiHandle >= KERNAL_FIRST_DEVICE_HANDLE + KERNAL_MAX_HANDLES))
	{
		return NULL;
	}
	return &gasKernalHandles[uiHandle - KERNAL_FIRST_DEVICE_HANDLE];
}


//////////////////////////////////////////////////////////////////////////
//
// The driver and unit of uiHandle if the running process has it open,
// NULL if not.
//
//////////////////////////////////////////////////////////////////////////
static CKernalDriver* KernalHandleDriver(uint16 uiHandle, uint16* puiUnit)
{
	SKernalHandle*	psHandle;

	psHandle = KernalGetHandle(uiHandle);
	if (!psHandle)
	{
		return NULL;
	}
	if ((psHandle->uiProcess == 0) || (psHandle->uiProcess != KernalCurrentProcess()))
	{
		return NULL;
	}
	*puiUnit = psHandle->uiUnit;
	return KernalGetDriver(psHandle->uiSlot);
}


//////////////////////////////////////////////////////////////////////////
//
// The uiIndex'th board in the device sockets, written to the running
// process's memory at uiAddress.
//
//////////////////////////////////////////////////////////////////////////
static uint16 KernalDeviceInfoCall(uint16 uiIndex, uint32 uiAddress)
{
	SKernalDeviceInfo	sInfo;
	SKernalDeviceROM*	psROM;
	CKernalDriver*		pcDriver;
	uint16				uiSlot;

	for (uiSlot = 0; uiSlot < KERNAL_NUM_DEVICE_SLOTS; uiSlot++)
	{
		psROM = gasKernalDevices[uiSlot].psROM;
		if (!psROM)
		{
			continue;
		}
		if (uiIndex > 0)
		{
			uiIndex--;
			continue;
		}

		memset(&sInfo, 0, sizeof(SKernalDeviceInfo));
		pcDriver = KernalGetDriver(uiSlot);
		sInfo.uiSlot = uiSlot;
		sInfo.uiNumUnits = pcDriver ? pcDriver->NumUnits() : 0;
		sInfo.uiRevision = psROM->uiRevision;
		sInfo.uiOpen = (pcDriver && pcDriver->IsOpen()) ? 1 : 0;
		strcpy(sInfo.szName, psROM->szName);
		strcpy(sInfo.szManufacturer, psROM->szManufacturer);
		if (pcDriver && (gsKernalStatus.uiLoadedDrivers & ((uint32)1 << uiSlot)))
		{
			strncpy(sInfo.szDriverFile, psROM->szDriverFile, KERNAL_DEVICE_DRIVER_LENGTH - 1);
		}
		return KernalCopyToUser(uiAddress, (uint8*)&sInfo, sizeof(SKernalDeviceInfo)) ? KERNAL_DEVICE_SUCCESS : KERNAL_DEVICE_BAD_ARGUMENT;
	}
	return KERNAL_DEVICE_NOT_FOUND;
}


//////////////////////////////////////////////////////////////////////////
//
// Opens unit uiUnit of the uiIndex'th board made by the manufacturer called
// the name (the strings at uiManufacturer and uiName in the running
// process's memory).  *puiHandle is its handle.
//
//////////////////////////////////////////////////////////////////////////
static uint16 KernalDeviceOpenCall(uint32 uiManufacturer, uint32 uiName, uint16 uiIndex, uint16 uiUnit, uint32* puiHandle)
{
	char			szManufacturer[KERNAL_DEVICE_NAME_LENGTH];
	char			szName[KERNAL_DEVICE_NAME_LENGTH];
	CKernalDriver*	pcDriver;
	SKernalHandle*	psHandle;
	uint16			uiHandle;
	uint16			uiResult;

	*puiHandle = 0;
	if (!KernalUserString(uiManufacturer, szManufacturer, KERNAL_DEVICE_NAME_LENGTH) || !KernalUserString(uiName, szName, KERNAL_DEVICE_NAME_LENGTH))
	{
		return KERNAL_DEVICE_BAD_ARGUMENT;
	}
	pcDriver = KernalFindDriver(szManufacturer, szName, uiIndex);
	if (!pcDriver)
	{
		return KERNAL_DEVICE_NOT_FOUND;
	}
	for (uiHandle = 0; uiHandle < KERNAL_MAX_HANDLES; uiHandle++)
	{
		if (gasKernalHandles[uiHandle].uiProcess == 0)
		{
			break;
		}
	}
	if (uiHandle == KERNAL_MAX_HANDLES)
	{
		return KERNAL_DEVICE_NO_HANDLES;
	}

	uiResult = pcDriver->Open(uiUnit, KernalCurrentProcess());
	if (uiResult != KERNAL_DRIVER_SUCCESS)
	{
		return uiResult;
	}
	psHandle = &gasKernalHandles[uiHandle];
	psHandle->uiProcess = KernalCurrentProcess();
	psHandle->uiSlot = (uint8)pcDriver->GetSlot();
	psHandle->uiUnit = uiUnit;
	*puiHandle = (uint32)uiHandle + KERNAL_FIRST_DEVICE_HANDLE;
	return KERNAL_DEVICE_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////
//
// Frees the handle; its unit is let go when it was the process's last
// handle on it.
//
//////////////////////////////////////////////////////////////////////////
static uint16 KernalDeviceCloseCall(uint16 uiHandle)
{
	CKernalDriver*	pcDriver;
	uint16			uiUnit;
	SKernalHandle*	psHandle;
	uint8			uiProcess;
	uint8			uiSlot;
	uint16			i;

	pcDriver = KernalHandleDriver(uiHandle, &uiUnit);
	if (!pcDriver)
	{
		return KERNAL_DEVICE_BAD_HANDLE;
	}
	psHandle = KernalGetHandle(uiHandle);
	uiProcess = psHandle->uiProcess;
	uiSlot = psHandle->uiSlot;
	psHandle->uiProcess = 0;
	for (i = 0; i < KERNAL_MAX_HANDLES; i++)
	{
		psHandle = &gasKernalHandles[i];
		if ((psHandle->uiProcess == uiProcess) && (psHandle->uiSlot == uiSlot) && (psHandle->uiUnit == uiUnit))
		{
			return KERNAL_DEVICE_SUCCESS;
		}
	}
	pcDriver->Close(uiUnit, uiProcess);
	return KERNAL_DEVICE_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////
//
// A read, write, control or wait on a standard handle: the console's
// (input on KERNAL_STANDARD_INPUT, output and control on
// KERNAL_STANDARD_OUTPUT).  *psCall is the process's block at uiAddress.
//
//////////////////////////////////////////////////////////////////////////
static uint16 KernalConsoleCall(SKernalDeviceCall* psCall, uint32 uiAddress)
{
	bool	bInput;
	uint16	uiResult;

	bInput = psCall->uiHandle == KERNAL_STANDARD_INPUT;
	psCall->uiResult = 0;
	switch (psCall->uiCall)
	{
	case KERNAL_DEVICE_CALL_READ:
		uiResult = bInput ? gcKernalConsole.Read(psCall->uiAddress, psCall->uiLength, &psCall->uiResult) : KERNAL_DEVICE_NOT_SUPPORTED;
		break;

	case KERNAL_DEVICE_CALL_WRITE:
		uiResult = !bInput ? gcKernalConsole.Write(psCall->uiAddress, psCall->uiLength, &psCall->uiResult) : KERNAL_DEVICE_NOT_SUPPORTED;
		break;

	case KERNAL_DEVICE_CALL_CONTROL:
		uiResult = !bInput ? gcKernalConsole.Control(psCall->uiCode, psCall->uiValue, &psCall->uiResult) : KERNAL_DEVICE_NOT_SUPPORTED;
		break;

	case KERNAL_DEVICE_CALL_WAIT:
		return bInput ? gcKernalConsole.Wait(psCall->uiValue, psCall->uiCode) : KERNAL_DEVICE_NOT_SUPPORTED;

	default:
		return KERNAL_DEVICE_BAD_HANDLE;
	}
	KernalCopyToUser(uiAddress + KERNAL_DEVICE_CALL_RESULT, (uint8*)&psCall->uiResult, sizeof(uint32));
	return uiResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The device call whose block is at uiAddress in the running process's
// memory.
//
//////////////////////////////////////////////////////////////////////////
uint16 KernalCallDevice(uint32 uiAddress)
{
	SKernalDeviceCall	sCall;
	CKernalDriver*		pcDriver;
	uint16				uiUnit;
	uint16				uiResult;

	if (!KernalCopyFromUser((uint8*)&sCall, uiAddress, sizeof(SKernalDeviceCall)))
	{
		return KERNAL_DEVICE_BAD_ARGUMENT;
	}

	switch (sCall.uiCall)
	{
	case KERNAL_DEVICE_CALL_INFO:
		return KernalDeviceInfoCall(sCall.uiHandle, sCall.uiAddress);

	case KERNAL_DEVICE_CALL_OPEN:
		uiResult = KernalDeviceOpenCall(sCall.uiAddress, sCall.uiLength, sCall.uiHandle, sCall.uiUnit, &sCall.uiResult);
		KernalCopyToUser(uiAddress + KERNAL_DEVICE_CALL_RESULT, (uint8*)&sCall.uiResult, sizeof(uint32));
		return uiResult;

	case KERNAL_DEVICE_CALL_CLOSE:
		return KernalDeviceCloseCall(sCall.uiHandle);
	}

	if ((sCall.uiHandle == KERNAL_STANDARD_INPUT) || (sCall.uiHandle == KERNAL_STANDARD_OUTPUT))
	{
		return KernalConsoleCall(&sCall, uiAddress);
	}

	pcDriver = KernalHandleDriver(sCall.uiHandle, &uiUnit);
	if (!pcDriver)
	{
		return KERNAL_DEVICE_BAD_HANDLE;
	}
	switch (sCall.uiCall)
	{
	case KERNAL_DEVICE_CALL_READ:
		sCall.uiResult = 0;
		uiResult = pcDriver->Read(uiUnit, sCall.uiValue, sCall.uiAddress, sCall.uiLength, &sCall.uiResult);
		KernalCopyToUser(uiAddress + KERNAL_DEVICE_CALL_RESULT, (uint8*)&sCall.uiResult, sizeof(uint32));
		return uiResult;

	case KERNAL_DEVICE_CALL_WRITE:
		sCall.uiResult = 0;
		uiResult = pcDriver->Write(uiUnit, sCall.uiValue, sCall.uiAddress, sCall.uiLength, &sCall.uiResult);
		KernalCopyToUser(uiAddress + KERNAL_DEVICE_CALL_RESULT, (uint8*)&sCall.uiResult, sizeof(uint32));
		return uiResult;

	case KERNAL_DEVICE_CALL_CONTROL:
		sCall.uiResult = 0;
		uiResult = pcDriver->Control(uiUnit, sCall.uiCode, sCall.uiValue, &sCall.uiResult);
		KernalCopyToUser(uiAddress + KERNAL_DEVICE_CALL_RESULT, (uint8*)&sCall.uiResult, sizeof(uint32));
		return uiResult;

	case KERNAL_DEVICE_CALL_WAIT:
		return pcDriver->Wait(uiUnit, sCall.uiValue, sCall.uiCode);
	}
	return KERNAL_DEVICE_BAD_ARGUMENT;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void KernalCloseHandles(uint8 uiProcess)
{
	uint16	i;

	for (i = 0; i < KERNAL_MAX_HANDLES; i++)
	{
		if (gasKernalHandles[i].uiProcess == uiProcess)
		{
			gasKernalHandles[i].uiProcess = 0;
		}
	}
}
