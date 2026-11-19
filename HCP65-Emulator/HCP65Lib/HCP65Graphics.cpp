#include "HCP65.h"
#include "HCP65Graphics.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65GraphicsBoard::Init(void)
{
	InitBlitter(HCP65_LARGE_SLOT_SIZE, HCP65_GRAPHICS_COUNT_MASK);
	muiSource = 0;
	mcROM.Init(HCP65_GRAPHICS_NAME, HCP65_DEVICE_MANUFACTURER, 0, DEVICE_BUS_128KB, DEVICE_MAIN_BUS_MASTER, HCP65_GRAPHICS_DRIVER);
	mcROM.AddRegister(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_FLAGS, DEVICE_REGISTER_READ);
	mcROM.AddRegister(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_FLAGS, DEVICE_REGISTER_WRITE);
	mcROM.AddRegister(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_CLEAR_INTERRUPT, DEVICE_REGISTER_WRITE);
	mcROM.AddRegister(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_TRANSPARENT_COLOUR, DEVICE_REGISTER_READ);
	mcROM.AddRegister(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_TRANSPARENT_COLOUR, DEVICE_REGISTER_WRITE);
	mcROM.AddRegisters(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_DEST_LOW, 4, DEVICE_REGISTER_WRITE);
	mcROM.AddRegisters(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_SOURCE_LOW, 3, DEVICE_REGISTER_READ);
	mcROM.AddRegisters(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_SOURCE_LOW, 3, DEVICE_REGISTER_WRITE);
	mcROM.AddRegisters(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_COUNT_START_LOW, 2, DEVICE_REGISTER_READ);
	mcROM.AddRegisters(HCP65_BLITTER_REGISTERS + HCP65_BLITTER_COUNT_START_LOW, 4, DEVICE_REGISTER_WRITE);

	mpuiOffsetsLow = (uint8*)malloc(HCP65_GRAPHICS_OFFSETS);
	mpuiOffsetsHigh = (uint8*)malloc(HCP65_GRAPHICS_OFFSETS);
	memset(mpuiOffsetsLow, 0, HCP65_GRAPHICS_OFFSETS);
	memset(mpuiOffsetsHigh, 0, HCP65_GRAPHICS_OFFSETS);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65GraphicsBoard::Kill(void)
{
	free(mpuiOffsetsLow);
	free(mpuiOffsetsHigh);
	mpuiOffsetsLow = NULL;
	mpuiOffsetsHigh = NULL;
	CHCP65BlitterBoard::Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// As the Blitter Board's, but the source address is kept.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65GraphicsBoard::Reset(void)
{
	uint32	uiSource;

	uiSource = muiSource;
	CHCP65BlitterBoard::Reset();
	muiSource = uiSource;
}


//////////////////////////////////////////////////////////////////////////
//
// The offsets are the board's while it copies.
//
//////////////////////////////////////////////////////////////////////////
uint8 CHCP65GraphicsBoard::Read(uint32 uiOffset)
{
	if ((uiOffset >= HCP65_GRAPHICS_OFFSETS_LOW) && (uiOffset < HCP65_GRAPHICS_OFFSETS_HIGH + HCP65_GRAPHICS_OFFSETS))
	{
		if (mbCopying)
		{
			return 0xFF;
		}
		if (uiOffset < HCP65_GRAPHICS_OFFSETS_HIGH)
		{
			return mpuiOffsetsLow[uiOffset - HCP65_GRAPHICS_OFFSETS_LOW];
		}
		return mpuiOffsetsHigh[uiOffset - HCP65_GRAPHICS_OFFSETS_HIGH];
	}
	return CHCP65BlitterBoard::Read(uiOffset);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65GraphicsBoard::Write(uint32 uiOffset, uint8 uiValue)
{
	if ((uiOffset >= HCP65_GRAPHICS_OFFSETS_LOW) && (uiOffset < HCP65_GRAPHICS_OFFSETS_HIGH + HCP65_GRAPHICS_OFFSETS))
	{
		if (!mbCopying)
		{
			if (uiOffset < HCP65_GRAPHICS_OFFSETS_HIGH)
			{
				mpuiOffsetsLow[uiOffset - HCP65_GRAPHICS_OFFSETS_LOW] = uiValue;
			}
			else
			{
				mpuiOffsetsHigh[uiOffset - HCP65_GRAPHICS_OFFSETS_HIGH] = uiValue;
			}
		}
		return;
	}
	CHCP65BlitterBoard::Write(uiOffset, uiValue);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint8 CHCP65GraphicsBoard::ReadRegister(uint32 uiRegister)
{
	switch (uiRegister)
	{
	case HCP65_BLITTER_SOURCE_LOW:
		return (uint8)muiSource;
	case HCP65_BLITTER_SOURCE_HIGH:
		return (uint8)(muiSource >> 8);
	case HCP65_BLITTER_SOURCE_BANK:
		return (uint8)(muiSource >> 16);
	}
	return CHCP65BlitterBoard::ReadRegister(uiRegister);
}


//////////////////////////////////////////////////////////////////////////
//
// The source address is latches, which take a write whenever it comes.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65GraphicsBoard::WriteRegister(uint32 uiRegister, uint8 uiValue)
{
	switch (uiRegister)
	{
	case HCP65_BLITTER_SOURCE_LOW:
		muiSource = (muiSource & 0xFFFF00) | uiValue;
		return;
	case HCP65_BLITTER_SOURCE_HIGH:
		muiSource = (muiSource & 0xFF00FF) | ((uint32)uiValue << 8);
		return;
	case HCP65_BLITTER_SOURCE_BANK:
		muiSource = (muiSource & 0x00FFFF) | ((uint32)uiValue << 16);
		return;
	}
	CHCP65BlitterBoard::WriteRegister(uiRegister, uiValue);
}


//////////////////////////////////////////////////////////////////////////
//
// The copy's next byte: the one at the source address + the count's
// offset.
//
//////////////////////////////////////////////////////////////////////////
uint8 CHCP65GraphicsBoard::ReadSource(void)
{
	return mpcHCP65->ReadMainAsMaster((muiSource + GetOffset(muiCount)) & 0xFFFFFF);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint16 CHCP65GraphicsBoard::GetOffset(uint16 uiCount) const
{
	uiCount &= HCP65_GRAPHICS_COUNT_MASK;
	return (uint16)(mpuiOffsetsLow[uiCount] | (mpuiOffsetsHigh[uiCount] << 8));
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65GraphicsBoard::SetOffset(uint16 uiCount, uint16 uiOffset)
{
	uiCount &= HCP65_GRAPHICS_COUNT_MASK;
	mpuiOffsetsLow[uiCount] = (uint8)uiOffset;
	mpuiOffsetsHigh[uiCount] = (uint8)(uiOffset >> 8);
}
