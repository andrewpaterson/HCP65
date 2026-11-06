#include <string.h>
#include "HCP65User.h"


//////////////////////////////////////////////////////////////////////////
//
// A device call's block, cleared, for uiCall on uiHandle.
//
//////////////////////////////////////////////////////////////////////////
static void UserDeviceCall(SKernalDeviceCall* psCall, uint16 uiCall, uint16 uiHandle)
{
	memset(psCall, 0, sizeof(SKernalDeviceCall));
	psCall->uiCall = uiCall;
	psCall->uiHandle = uiHandle;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
extern "C" uint16 KernalDeviceInfo(uint16 uiIndex, SKernalDeviceInfo* psInfo)
{
	SKernalDeviceCall	sCall;

	UserDeviceCall(&sCall, KERNAL_DEVICE_CALL_INFO, uiIndex);
	sCall.uiAddress = (uint32)psInfo;
	return KernalDeviceCall(&sCall);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
extern "C" uint16 KernalDeviceOpen(const char* szManufacturer, const char* szName, uint16 uiIndex, uint16 uiUnit, uint16* puiHandle)
{
	SKernalDeviceCall	sCall;
	uint16				uiResult;

	UserDeviceCall(&sCall, KERNAL_DEVICE_CALL_OPEN, uiIndex);
	sCall.uiUnit = uiUnit;
	sCall.uiAddress = (uint32)szManufacturer;
	sCall.uiLength = (uint32)szName;
	uiResult = KernalDeviceCall(&sCall);
	*puiHandle = (uint16)sCall.uiResult;
	return uiResult;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
extern "C" uint16 KernalDeviceClose(uint16 uiHandle)
{
	SKernalDeviceCall	sCall;

	UserDeviceCall(&sCall, KERNAL_DEVICE_CALL_CLOSE, uiHandle);
	return KernalDeviceCall(&sCall);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
extern "C" uint16 KernalDeviceRead(uint16 uiHandle, uint32 uiOffset, void* pvDest, uint32 uiLength)
{
	SKernalDeviceCall	sCall;

	UserDeviceCall(&sCall, KERNAL_DEVICE_CALL_READ, uiHandle);
	sCall.uiValue = uiOffset;
	sCall.uiAddress = (uint32)pvDest;
	sCall.uiLength = uiLength;
	return KernalDeviceCall(&sCall);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
extern "C" uint16 KernalDeviceWrite(uint16 uiHandle, uint32 uiOffset, const void* pvSource, uint32 uiLength)
{
	SKernalDeviceCall	sCall;

	UserDeviceCall(&sCall, KERNAL_DEVICE_CALL_WRITE, uiHandle);
	sCall.uiValue = uiOffset;
	sCall.uiAddress = (uint32)pvSource;
	sCall.uiLength = uiLength;
	return KernalDeviceCall(&sCall);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
extern "C" uint16 KernalDeviceControl(uint16 uiHandle, uint16 uiCode, uint32 uiValue, uint32* puiResult)
{
	SKernalDeviceCall	sCall;
	uint16				uiResult;

	UserDeviceCall(&sCall, KERNAL_DEVICE_CALL_CONTROL, uiHandle);
	sCall.uiCode = uiCode;
	sCall.uiValue = uiValue;
	uiResult = KernalDeviceCall(&sCall);
	if (puiResult)
	{
		*puiResult = sCall.uiResult;
	}
	return uiResult;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
extern "C" uint16 KernalDeviceWait(uint16 uiHandle, uint32 uiValue, uint16 uiTicks)
{
	SKernalDeviceCall	sCall;

	UserDeviceCall(&sCall, KERNAL_DEVICE_CALL_WAIT, uiHandle);
	sCall.uiCode = uiTicks;
	sCall.uiValue = uiValue;
	return KernalDeviceCall(&sCall);
}
