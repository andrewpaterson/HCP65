#include "KernalInterrupts.h"
#include "KernalProcess.h"
#include "SoundBoard.h"


//Offsets in the slot, and the registers (HCP65Lib/HCP65Audio.h).
#define SOUND_RAM_A					0x10000
#define SOUND_RAM_B					0x18000
#define SOUND_FLAGS					0
#define SOUND_CLEAR_INTERRUPT		1
#define SOUND_INTERRUPT_LOW			6
#define SOUND_INTERRUPT_HIGH		7
#define SOUND_FLAG_INTERRUPT		0x80


//The board's assembly (SoundBoardAudio.asm).
extern "C" void		KernalAudioCopy(uint8* puiDest, uint8* puiSource, uint16 uiCount);
extern "C" void		KernalAudioSetRegister(uint8* puiRegisters, uint16 uiRegister, uint16 uiValue);
extern "C" uint16	KernalAudioGetCounter(uint8* puiRegisters);


//////////////////////////////////////////////////////////////////////////
//
// The board starts disabled; its interrupt is trusted.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalSoundBoard::Init(SKernalDevice* psDevice, uint16 uiSlot)
{
	uint16	uiResult;

	uiResult = CKernalDriver::Init(psDevice, uiSlot);
	mbInterrupt = 0;
	SetInterrupt(&mpuiRegisters[SOUND_FLAGS], true);
	return uiResult;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalSoundBoard::NumUnits(void)
{
	return 2;
}


//////////////////////////////////////////////////////////////////////////
//
// The counter reached the Interrupt address.  Interrupt enable off, the
// interrupt cleared, and the Wait told.  Trusted: only the board's
// registers and MPU RAM.
//
//////////////////////////////////////////////////////////////////////////
void CKernalSoundBoard::Interrupt(void)
{
	mpuiRegisters[SOUND_FLAGS] = (uint8)(mpuiRegisters[SOUND_FLAGS] & ~SOUND_FLAG_INTERRUPT);
	mpuiRegisters[SOUND_CLEAR_INTERRUPT] = 0;
	mbInterrupt = 1;
}


//////////////////////////////////////////////////////////////////////////
//
// uiLength samples from the running process's memory at uiUserAddress to
// the unit's RAM at uiOffset, wrapping at its end.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalSoundBoard::Write(uint16 uiUnit, uint32 uiOffset, uint32 uiUserAddress, uint32 uiLength)
{
	uint8*	puiRAM;
	uint16	uiPart;

	if (uiUnit >= NumUnits())
	{
		return KERNAL_DRIVER_BAD_UNIT;
	}
	puiRAM = (uint8*)(mpsDevice->uiAddress + ((uiUnit == KERNAL_SOUND_LEFT) ? SOUND_RAM_A : SOUND_RAM_B));
	uiOffset &= KERNAL_SOUND_RAM_SIZE - 1;
	while (uiLength > 0)
	{
		uiPart = (uint16)(KERNAL_SOUND_RAM_SIZE - uiOffset);
		uiPart = (uiPart < uiLength) ? uiPart : (uint16)uiLength;
		CopyFromUser(&puiRAM[uiOffset], uiUserAddress, uiPart);
		uiUserAddress += uiPart;
		uiLength -= uiPart;
		uiOffset = 0;
	}
	return KERNAL_DRIVER_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////
//
// KERNAL_SOUND_SET_REGISTER + n writes register n; KERNAL_SOUND_GET_COUNTER
// reads the counter.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalSoundBoard::Control(uint16 uiUnit, uint16 uiCode, uint32 uiValue, uint32* puiResult)
{
	if (uiCode < KERNAL_SOUND_SET_REGISTER + KERNAL_SOUND_NUM_REGISTERS)
	{
		KernalAudioSetRegister((uint8*)mpuiRegisters, uiCode - KERNAL_SOUND_SET_REGISTER, (uint16)uiValue);
		return KERNAL_DRIVER_SUCCESS;
	}
	if (uiCode == KERNAL_SOUND_GET_COUNTER)
	{
		*puiResult = KernalAudioGetCounter((uint8*)mpuiRegisters);
		return KERNAL_DRIVER_SUCCESS;
	}
	return KERNAL_DRIVER_NOT_SUPPORTED;
}


//////////////////////////////////////////////////////////////////////////
//
// Waits for the counter to reach the address uiValue (0 to $7FFF), or
// uiTicks: it may have passed it already.  KERNAL_DRIVER_TIMEOUT if it
// didn't interrupt.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalSoundBoard::Wait(uint16 uiUnit, uint32 uiValue, uint16 uiTicks)
{
	uint32	uiStart;

	mbInterrupt = 0;
	mpuiRegisters[SOUND_INTERRUPT_LOW] = (uint8)uiValue;
	mpuiRegisters[SOUND_INTERRUPT_HIGH] = (uint8)(uiValue >> 8);
	mpuiRegisters[SOUND_FLAGS] = (uint8)(mpuiRegisters[SOUND_FLAGS] | SOUND_FLAG_INTERRUPT);

	uiStart = KernalTicks();
	while (!mbInterrupt && ((KernalTicks() - uiStart) <= uiTicks))
	{
		KernalWaitForInterrupt();
	}
	mpuiRegisters[SOUND_FLAGS] = (uint8)(mpuiRegisters[SOUND_FLAGS] & ~SOUND_FLAG_INTERRUPT);
	return mbInterrupt ? KERNAL_DRIVER_SUCCESS : KERNAL_DRIVER_TIMEOUT;
}


//////////////////////////////////////////////////////////////////////////
//
// Copies uiLength samples from the running process's memory at uiAddress
// to the board (KernalAudioCopy), a run in one of its banks at a time.
//
//////////////////////////////////////////////////////////////////////////
void CKernalSoundBoard::CopyFromUser(uint8* puiDest, uint32 uiAddress, uint16 uiLength)
{
	uint8*	puiSource;
	uint32	uiInBank;

	while (uiLength > 0)
	{
		puiSource = KernalUserPointer(uiAddress, &uiInBank);
		if (!puiSource)
		{
			return;
		}
		uiInBank = (uiInBank < uiLength) ? uiInBank : uiLength;
		KernalAudioCopy(puiDest, puiSource, (uint16)uiInBank);
		uiAddress += uiInBank;
		puiDest += uiInBank;
		uiLength = (uint16)(uiLength - uiInBank);
	}
}


//The driver of the first sound board.
static CKernalSoundBoard	gcSoundBoard;


//////////////////////////////////////////////////////////////////////////
//
// The kernal's loader calls it once the driver is in place and its globals
// are constructed: the driver it starts for the board.
//
//////////////////////////////////////////////////////////////////////////
extern "C" CKernalDriver* KernalCreateDriver(void)
{
	return &gcSoundBoard;
}
