#include "KernalInterrupts.h"
#include "KernalProcess.h"
#include "VIABoard.h"


//////////////////////////////////////////////////////////////////////////
//
// Finds the fitted VIAs, which come out of reset with DDRA 0 and hold
// what is written to it, and quietens them.  The interrupt is trusted.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalVIABoard::Init(SKernalDevice* psDevice, uint16 uiSlot)
{
	volatile uint8*	puiVIA;
	uint16			uiResult;
	uint16			i;

	uiResult = CKernalDriver::Init(psDevice, uiSlot);
	mbSeparateUnits = true;
	muiFitted = 0;
	for (i = 0; i < VIA_BOARD_NUM_VIAS; i++)
	{
		puiVIA = GetVIA(i);
		puiVIA[VIA_DDRA] = 0x55;
		if (puiVIA[VIA_DDRA] == 0x55)
		{
			puiVIA[VIA_DDRA] = 0xAA;
			if (puiVIA[VIA_DDRA] == 0xAA)
			{
				muiFitted |= (uint8)(1 << i);
			}
		}
		puiVIA[VIA_DDRA] = 0;
		if (IsFitted(i))
		{
			Quieten(i);
		}
	}
	SetInterrupt(NULL, true);
	return uiResult;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CKernalVIABoard::Kill(void)
{
	uint16	i;

	for (i = 0; i < VIA_BOARD_NUM_VIAS; i++)
	{
		if (IsFitted(i))
		{
			Quieten(i);
		}
	}
	CKernalDriver::Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// All eight, fitted or not, so that a unit is always the same VIA.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalVIABoard::NumUnits(void) const
{
	return VIA_BOARD_NUM_VIAS;
}


//////////////////////////////////////////////////////////////////////////
//
// Each interrupting VIA's set flags have their interrupts turned off;
// the flags stay set for the waiting process.  Trusted: only the VIAs.
//
//////////////////////////////////////////////////////////////////////////
void CKernalVIABoard::Interrupt(void)
{
	volatile uint8*	puiVIA;
	uint8			uiInterrupts;
	uint16			i;

	uiInterrupts = mpuiRegisters[VIA_BOARD_INTERRUPTS];
	for (i = 0; i < VIA_BOARD_NUM_VIAS; i++)
	{
		if (uiInterrupts & (uint8)(1 << i))
		{
			puiVIA = GetVIA(i);
			puiVIA[VIA_IER] = (uint8)(puiVIA[VIA_IFR] & puiVIA[VIA_IER] & VIA_INT_FLAGS);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The process has the VIA as reset left it.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalVIABoard::OpenUnit(uint16 uiUnit)
{
	if (!IsFitted(uiUnit))
	{
		return KERNAL_DRIVER_BAD_UNIT;
	}
	Quieten(uiUnit);
	return KERNAL_DRIVER_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CKernalVIABoard::CloseUnit(uint16 uiUnit)
{
	Quieten(uiUnit);
}


//////////////////////////////////////////////////////////////////////////
//
// The registers from uiOffset to the running process's memory at
// uiUserAddress, one a byte.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalVIABoard::Read(uint16 uiUnit, uint32 uiOffset, uint32 uiUserAddress, uint32 uiLength, uint32* puiDone)
{
	volatile uint8*	puiVIA;
	uint8			uiValue;
	uint16			i;

	if (!IsRange(uiOffset, uiLength))
	{
		return KERNAL_DRIVER_BAD_ARGUMENT;
	}
	puiVIA = GetVIA(uiUnit);
	for (i = 0; i < (uint16)uiLength; i++)
	{
		uiValue = puiVIA[(uint16)uiOffset + i];
		if (!KernalCopyToUser(uiUserAddress + i, &uiValue, 1))
		{
			return KERNAL_DRIVER_BAD_ARGUMENT;
		}
		*puiDone += 1;
	}
	return KERNAL_DRIVER_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////
//
// The running process's bytes at uiUserAddress to the registers from
// uiOffset.  Nothing is written if they take in IER.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalVIABoard::Write(uint16 uiUnit, uint32 uiOffset, uint32 uiUserAddress, uint32 uiLength, uint32* puiDone)
{
	volatile uint8*	puiVIA;
	uint8			uiValue;
	uint16			i;

	if (!IsRange(uiOffset, uiLength) || ((uiOffset <= VIA_IER) && ((uiOffset + uiLength) > VIA_IER)))
	{
		return KERNAL_DRIVER_BAD_ARGUMENT;
	}
	puiVIA = GetVIA(uiUnit);
	for (i = 0; i < (uint16)uiLength; i++)
	{
		if (!KernalCopyFromUser(&uiValue, uiUserAddress + i, 1))
		{
			return KERNAL_DRIVER_BAD_ARGUMENT;
		}
		puiVIA[(uint16)uiOffset + i] = uiValue;
		*puiDone += 1;
	}
	return KERNAL_DRIVER_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////
//
// HCP65VIABoard.h's VIA_BOARD_* codes.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalVIABoard::Control(uint16 uiUnit, uint16 uiCode, uint32 uiValue, uint32* puiResult)
{
	switch (uiCode)
	{
	case VIA_BOARD_GET_FITTED:
		*puiResult = muiFitted;
		return KERNAL_DRIVER_SUCCESS;

	default:
		return KERNAL_DRIVER_NOT_SUPPORTED;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Sleeps up to uiTicks until one of the IFR flags in uiValue is set, with
// their interrupts on meanwhile.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalVIABoard::Wait(uint16 uiUnit, uint32 uiValue, uint16 uiTicks)
{
	volatile uint8*	puiVIA;
	uint16			uiResult;

	if ((uiValue == 0) || (uiValue > VIA_INT_FLAGS))
	{
		return KERNAL_DRIVER_BAD_ARGUMENT;
	}
	if (IsWaitOver(uiUnit, uiValue))
	{
		return KERNAL_DRIVER_SUCCESS;
	}
	puiVIA = GetVIA(uiUnit);
	puiVIA[VIA_IER] = (uint8)(VIA_INT_ANY | uiValue);
	uiResult = WaitFor(uiUnit, uiValue, uiTicks);
	puiVIA[VIA_IER] = (uint8)uiValue;
	return uiResult;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CKernalVIABoard::IsWaitOver(uint16 uiUnit, uint32 uiValue)
{
	return (GetVIA(uiUnit)[VIA_IFR] & (uint8)uiValue) != 0;
}


//////////////////////////////////////////////////////////////////////////
//
// The kernal only gives the driver units below NumUnits.
//
//////////////////////////////////////////////////////////////////////////
volatile uint8* CKernalVIABoard::GetVIA(uint16 uiUnit)
{
	return &mpuiRegisters[uiUnit * VIA_BOARD_REGISTERS_SIZE];
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CKernalVIABoard::IsFitted(uint16 uiUnit)
{
	return (muiFitted & (uint8)(1 << uiUnit)) != 0;
}


//////////////////////////////////////////////////////////////////////////
//
// As the W65C22 is after reset: its interrupts off, its flags cleared,
// every pin an input, ACR and PCR 0.  The timers' latches and counters
// are left.
//
//////////////////////////////////////////////////////////////////////////
void CKernalVIABoard::Quieten(uint16 uiUnit)
{
	volatile uint8*	puiVIA;

	puiVIA = GetVIA(uiUnit);
	puiVIA[VIA_IER] = VIA_INT_FLAGS;
	puiVIA[VIA_ACR] = 0;
	puiVIA[VIA_PCR] = 0;
	puiVIA[VIA_DDRA] = 0;
	puiVIA[VIA_DDRB] = 0;
	puiVIA[VIA_ORA_NH] = 0;
	puiVIA[VIA_ORB] = 0;
	puiVIA[VIA_IFR] = VIA_INT_FLAGS;
}


//////////////////////////////////////////////////////////////////////////
//
// uiLength registers from uiOffset are all a VIA's.
//
//////////////////////////////////////////////////////////////////////////
bool CKernalVIABoard::IsRange(uint32 uiOffset, uint32 uiLength)
{
	return (uiOffset < VIA_NUM_REGISTERS) && (uiLength <= (VIA_NUM_REGISTERS - uiOffset));
}


//The drivers of the VIA boards, in slot order.
static CKernalVIABoard	gacVIABoards[VIA_BOARD_MAX_BOARDS];


//////////////////////////////////////////////////////////////////////////
//
// The kernal's loader calls it, once the driver is in place and its
// globals are constructed, for each board it drives: the driver of the
// uiIndex'th, NULL if there are too many.
//
//////////////////////////////////////////////////////////////////////////
extern "C" CKernalDriver* KernalCreateDriver(uint16 uiIndex)
{
	return (uiIndex < VIA_BOARD_MAX_BOARDS) ? &gacVIABoards[uiIndex] : NULL;
}
