#include "HCP65.h"
#include "HCP65Blitter.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65BlitterBoard::Init(void)
{
	InitBlitter(HCP65_SMALL_SLOT_SIZE, 0xFFFF);
	mcROM.Init(HCP65_BLITTER_NAME, HCP65_DEVICE_MANUFACTURER, 0, DEVICE_BUS_32KB, DEVICE_MAIN_BUS_MASTER, HCP65_BLITTER_DRIVER);
	mcROM.AddRegister(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_FLAGS, DEVICE_REGISTER_READ);
	mcROM.AddRegister(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_FLAGS, DEVICE_REGISTER_WRITE);
	mcROM.AddRegister(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_CLEAR_INTERRUPT, DEVICE_REGISTER_WRITE);
	mcROM.AddRegister(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_TRANSPARENT_COLOUR, DEVICE_REGISTER_READ);
	mcROM.AddRegister(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_TRANSPARENT_COLOUR, DEVICE_REGISTER_WRITE);
	mcROM.AddRegisters(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_DEST_LOW, 4, DEVICE_REGISTER_WRITE);
	mcROM.AddRegisters(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_SOURCE_LOW, 3, DEVICE_REGISTER_WRITE);
	mcROM.AddRegisters(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_COUNT_START_LOW, 2, DEVICE_REGISTER_READ);
	mcROM.AddRegisters(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_COUNT_START_LOW, 4, DEVICE_REGISTER_WRITE);
}


//////////////////////////////////////////////////////////////////////////
//
// The board without its ROM, which a board that is a Blitter Board and
// more (the Graphics Board) makes its own.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65BlitterBoard::InitBlitter(uint32 uiSlotSize, uint16 uiCountMask)
{
	CHCP65Device::Init(uiSlotSize);
	muiCountMask = uiCountMask;
	muiTransparentColour = 0;
	muiCountStart = 0;
	muiCountStop = 0;
	muiCountHighRead = 0;
	muiTotalCopied = 0;
	CHCP65BlitterBoard::Reset();
}


//////////////////////////////////////////////////////////////////////////
//
// The board is disabled, with its addresses and count 0.  The count's
// start and stop and the transparent colour are not reset.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65BlitterBoard::Reset(void)
{
	muiFlags = 0;
	mbEnabled = false;
	mbInterrupt = false;
	muiSource = 0;
	muiDest = 0;
	muiCount = 0;
	mbCopying = false;
	muiCopyStart = 0;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint8 CHCP65BlitterBoard::Read(uint32 uiOffset)
{
	if (uiOffset < DEVICE_ROM_SIZE)
	{
		return mcROM.Read(uiOffset);
	}
	if (uiOffset < 0x800)
	{
		return ReadRegister(uiOffset & 0x1F);
	}
	return 0xFF;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65BlitterBoard::Write(uint32 uiOffset, uint8 uiValue)
{
	if ((uiOffset >= DEVICE_ROM_SIZE) && (uiOffset < 0x800))
	{
		WriteRegister(uiOffset & 0x1F, uiValue);
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint8 CHCP65BlitterBoard::ReadRegister(uint32 uiRegister)
{
	uint8	uiValue;

	switch (uiRegister)
	{
	case HCP65_BLITTER_FLAGS:
		uiValue = (uint8)(muiFlags & 0x3F);
		if (mbEnabled)
		{
			uiValue |= HCP65_BLITTER_FLAG_ENABLE;
		}
		if (mbInterrupt)
		{
			uiValue |= HCP65_BLITTER_FLAG_INTERRUPT;
		}
		return uiValue;

	case HCP65_BLITTER_TRANSPARENT_COLOUR:
		return muiTransparentColour;

	case HCP65_BLITTER_COUNT_START_LOW:
		muiCountHighRead = (uint8)(muiCount >> 8);
		return (uint8)muiCount;

	case HCP65_BLITTER_COUNT_START_HIGH:
		return muiCountHighRead;
	}
	return 0xFF;
}


//////////////////////////////////////////////////////////////////////////
//
// The addresses and the count's start and stop aren't taken while a copy
// is under way.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65BlitterBoard::WriteRegister(uint32 uiRegister, uint8 uiValue)
{
	switch (uiRegister)
	{
	case HCP65_BLITTER_FLAGS:
		WriteFlags(uiValue);
		return;
	case HCP65_BLITTER_CLEAR_INTERRUPT:
		mbInterrupt = false;
		return;
	case HCP65_BLITTER_TRANSPARENT_COLOUR:
		muiTransparentColour = uiValue;
		return;
	case HCP65_BLITTER_START:
		Start();
		return;
	}
	if (mbCopying)
	{
		return;
	}

	switch (uiRegister)
	{
	case HCP65_BLITTER_DEST_LOW:
		muiDest = (muiDest & 0xFFFF00) | uiValue;
		break;
	case HCP65_BLITTER_DEST_HIGH:
		muiDest = (muiDest & 0xFF00FF) | ((uint32)uiValue << 8);
		break;
	case HCP65_BLITTER_DEST_BANK:
		muiDest = (muiDest & 0x00FFFF) | ((uint32)uiValue << 16);
		break;
	case HCP65_BLITTER_SOURCE_LOW:
		muiSource = (muiSource & 0xFFFF00) | uiValue;
		break;
	case HCP65_BLITTER_SOURCE_HIGH:
		muiSource = (muiSource & 0xFF00FF) | ((uint32)uiValue << 8);
		break;
	case HCP65_BLITTER_SOURCE_BANK:
		muiSource = (muiSource & 0x00FFFF) | ((uint32)uiValue << 16);
		break;
	case HCP65_BLITTER_COUNT_START_LOW:
		muiCountStart = (uint16)((muiCountStart & 0xFF00) | uiValue);
		break;
	case HCP65_BLITTER_COUNT_START_HIGH:
		muiCountStart = (uint16)(((muiCountStart & 0x00FF) | ((uint16)uiValue << 8)) & muiCountMask);
		break;
	case HCP65_BLITTER_COUNT_STOP_LOW:
		muiCountStop = (uint16)((muiCountStop & 0xFF00) | uiValue);
		break;
	case HCP65_BLITTER_COUNT_STOP_HIGH:
		muiCountStop = (uint16)(((muiCountStop & 0x00FF) | ((uint16)uiValue << 8)) & muiCountMask);
		break;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// D6 enables the board; it can't while the board is held in reset.
// Disabling stops a copy where it is.  The interrupt is cleared while
// interrupts or the board are disabled.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65BlitterBoard::WriteFlags(uint8 uiValue)
{
	muiFlags = (uint8)(uiValue & ~HCP65_BLITTER_FLAG_ENABLE);
	mbEnabled = ((uiValue & HCP65_BLITTER_FLAG_ENABLE) != 0) && !mbHeldInReset;
	if (!mbEnabled)
	{
		mbCopying = false;
	}
	if (!mbEnabled || !(muiFlags & HCP65_BLITTER_FLAG_INTERRUPT))
	{
		mbInterrupt = false;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Start: the count is its start, and the board has main memory until the
// count is its stop.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65BlitterBoard::Start(void)
{
	if (!mbEnabled || mbCopying || !mpcHCP65)
	{
		return;
	}
	mbInterrupt = false;
	muiCount = muiCountStart;
	mbCopying = true;
	muiCopyStart = mpcHCP65->NumCycles();
	if (muiCount == muiCountStop)
	{
		Finish();
		return;
	}
	mpcHCP65->SetMainMemoryBusy(CopyEnd());
}


//////////////////////////////////////////////////////////////////////////
//
// The CPU cycle the copy under way is done.
//
//////////////////////////////////////////////////////////////////////////
uint64 CHCP65BlitterBoard::CopyEnd(void) const
{
	return muiCopyStart + (uint64)(uint16)((muiCountStop - muiCountStart) & muiCountMask) * HCP65_BLITTER_CYCLES_PER_BYTE;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65BlitterBoard::Finish(void)
{
	mbCopying = false;
	if (muiFlags & HCP65_BLITTER_FLAG_INTERRUPT)
	{
		mbInterrupt = true;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The bytes the copy has had time for by uiCycle, each read then (unless
// it is transparent) written.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65BlitterBoard::CatchUp(uint64 uiCycle)
{
	uint64	uiBytes;
	uint8	uiByte;

	if (!mbCopying)
	{
		return;
	}
	uiBytes = (uiCycle > muiCopyStart) ? (uiCycle - muiCopyStart) / HCP65_BLITTER_CYCLES_PER_BYTE : 0;
	while ((uiBytes > 0) && (muiCount != muiCountStop))
	{
		uiByte = ReadSource();
		if (!(muiFlags & HCP65_BLITTER_FLAG_TRANSPARENCY) || (uiByte != muiTransparentColour))
		{
			mpcHCP65->WriteMainAsMaster(muiDest, uiByte);
		}
		muiDest = (muiDest + 1) & 0xFFFFFF;
		muiCount = (uint16)((muiCount + 1) & muiCountMask);
		muiCopyStart += HCP65_BLITTER_CYCLES_PER_BYTE;
		muiTotalCopied++;
		uiBytes--;
	}
	if (muiCount == muiCountStop)
	{
		Finish();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The copy's next byte, from the source address, which steps past it.
//
//////////////////////////////////////////////////////////////////////////
uint8 CHCP65BlitterBoard::ReadSource(void)
{
	uint8	uiByte;

	uiByte = mpcHCP65->ReadMainAsMaster(muiSource);
	muiSource = (muiSource + 1) & 0xFFFFFF;
	return uiByte;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint64 CHCP65BlitterBoard::NextEvent(void)
{
	if (mbCopying)
	{
		return muiCopyStart + (uint64)(uint16)((muiCountStop - muiCount) & muiCountMask) * HCP65_BLITTER_CYCLES_PER_BYTE;
	}
	return HCP65_NEVER;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65BlitterBoard::IsInterrupting(void)
{
	return mbInterrupt;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65BlitterBoard::IsCopying(void) const
{
	return mbCopying;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint32 CHCP65BlitterBoard::GetSource(void) const
{
	return muiSource;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint32 CHCP65BlitterBoard::GetDest(void) const
{
	return muiDest;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint16 CHCP65BlitterBoard::GetCount(void) const
{
	return muiCount;
}


//////////////////////////////////////////////////////////////////////////
//
// The bytes every copy has copied (a transparent byte is one).
//
//////////////////////////////////////////////////////////////////////////
uint64 CHCP65BlitterBoard::NumBytesCopied(void) const
{
	return muiTotalCopied;
}
