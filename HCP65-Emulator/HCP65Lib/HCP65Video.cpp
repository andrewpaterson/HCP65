#include <stdlib.h>
#include <string.h>
#include "HCP65.h"
#include "HCP65Video.h"


//Clock Select's eight pixel clocks (Video Clock): four oscillators and each halved.
static const uint64	gauiHCP65VideoClocks[HCP65_VIDEO_NUM_CLOCKS] = { 12587500, 20000000, 25175000, 32500000, 37125000, 40000000, 65000000, 74250000 };


//////////////////////////////////////////////////////////////////////////
//
// Held in reset, with the pixel clock stopped and the output off.  The
// memory is zeros.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65VideoBoard::Init(void)
{
	int		i;

	CHCP65Device::Init(HCP65_LARGE_SLOT_SIZE);

	mcROM.Init(HCP65_VIDEO_BOARD_NAME, HCP65_DEVICE_MANUFACTURER, 0, DEVICE_BUS_128KB, DEVICE_MAIN_BUS_WINDOW_A | DEVICE_MAIN_BUS_WINDOW_B | DEVICE_MAIN_BUS_PRIVATE, HCP65_VIDEO_DRIVER);
	mcROM.AddRegister(HCP65_VIDEO_REGISTERS + HCP65_VIDEO_FLAGS, DEVICE_REGISTER_READ);
	mcROM.AddRegister(HCP65_VIDEO_REGISTERS + HCP65_VIDEO_ADDRESS_LOW, DEVICE_REGISTER_READ);
	mcROM.AddRegister(HCP65_VIDEO_REGISTERS + HCP65_VIDEO_ADDRESS_HIGH, DEVICE_REGISTER_READ);
	mcROM.AddRegister(HCP65_VIDEO_REGISTERS + HCP65_VIDEO_ADDRESS_BANK, DEVICE_REGISTER_READ);
	mcROM.AddRegister(HCP65_VIDEO_REGISTERS + HCP65_VIDEO_H_FRONT_PORCH, DEVICE_REGISTER_READ);
	mcROM.AddRegister(HCP65_VIDEO_REGISTERS + HCP65_VIDEO_H_SYNC, DEVICE_REGISTER_READ);
	mcROM.AddRegister(HCP65_VIDEO_REGISTERS + HCP65_VIDEO_V_FRONT_PORCH, DEVICE_REGISTER_READ);
	mcROM.AddRegister(HCP65_VIDEO_REGISTERS + HCP65_VIDEO_V_FRONT_PORCH + 1, DEVICE_REGISTER_READ);
	mcROM.AddRegister(HCP65_VIDEO_REGISTERS + HCP65_VIDEO_FLAGS, DEVICE_REGISTER_WRITE);
	mcROM.AddRegister(HCP65_VIDEO_REGISTERS + HCP65_VIDEO_CLEAR_INTERRUPT, DEVICE_REGISTER_WRITE);
	mcROM.AddRegisters(HCP65_VIDEO_REGISTERS + HCP65_VIDEO_CLOCK_SELECT, HCP65_VIDEO_V_END + 2 - HCP65_VIDEO_CLOCK_SELECT, DEVICE_REGISTER_WRITE);
	mcROM.AddRegister(HCP65_VIDEO_REGISTERS + HCP65_VIDEO_PALETTE_BYTE, DEVICE_REGISTER_WRITE);
	mcROM.AddRegister(HCP65_VIDEO_REGISTERS + HCP65_VIDEO_SIGNAL_DELAY, DEVICE_REGISTER_WRITE);

	for (i = 0; i < 4; i++)
	{
		mapuiChips[i] = (uint8*)malloc(HCP65_VIDEO_FRAME_SIZE);
		memset(mapuiChips[i], 0, HCP65_VIDEO_FRAME_SIZE);
	}
	mpuiRowTable = (uint8*)malloc(HCP65_VIDEO_ROW_TABLE_SIZE);
	mpuiRed = (uint8*)malloc(HCP65_VIDEO_PALETTE_SIZE);
	mpuiGreen = (uint8*)malloc(HCP65_VIDEO_PALETTE_SIZE);
	mpuiBlue = (uint8*)malloc(HCP65_VIDEO_PALETTE_SIZE);
	memset(mpuiRowTable, 0, HCP65_VIDEO_ROW_TABLE_SIZE);
	memset(mpuiRed, 0, HCP65_VIDEO_PALETTE_SIZE);
	memset(mpuiGreen, 0, HCP65_VIDEO_PALETTE_SIZE);
	memset(mpuiBlue, 0, HCP65_VIDEO_PALETTE_SIZE);

	muiFlags = 0;
	mbEnable = false;
	mbPending = false;
	muiClockSelect = 0;
	memset(mauiHorizontal, 0, sizeof(mauiHorizontal));
	memset(mauiVertical, 0, sizeof(mauiVertical));
	muiPattern = 0;
	muiPaletteByte = 0;
	muiSignalDelay = HCP65_VIDEO_DELAY_OFF;
	muiAddressSnapshot = 0;

	muiCycle = 0;
	muiTick = 0;
	muiRateCycle = 0;
	muiRateTick = 0;
	muiLine = 0;
	muiLineStart = 0;

	mpuiDrawing = (uint32*)malloc(HCP65_VIDEO_MAX_WIDTH * HCP65_VIDEO_MAX_ROWS * sizeof(uint32));
	mpuiShown = (uint32*)malloc(HCP65_VIDEO_MAX_WIDTH * HCP65_VIDEO_MAX_ROWS * sizeof(uint32));
	memset(mpuiDrawing, 0, HCP65_VIDEO_MAX_WIDTH * HCP65_VIDEO_MAX_ROWS * sizeof(uint32));
	memset(mpuiShown, 0, HCP65_VIDEO_MAX_WIDTH * HCP65_VIDEO_MAX_ROWS * sizeof(uint32));
	muiShownWidth = 0;
	muiShownHeight = 0;
	muiFrames = 0;
	mcFrameMutex.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65VideoBoard::Kill(void)
{
	int		i;

	mcFrameMutex.Kill();
	free(mpuiShown);
	free(mpuiDrawing);
	free(mpuiBlue);
	free(mpuiGreen);
	free(mpuiRed);
	free(mpuiRowTable);
	for (i = 0; i < 4; i++)
	{
		free(mapuiChips[i]);
	}
	CHCP65Device::Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The device ROM, the registers, then device RAM: the row table and the
// three palettes.  Nothing else drives the bus.
//
//////////////////////////////////////////////////////////////////////////
uint8 CHCP65VideoBoard::Read(uint32 uiOffset)
{
	uint8*	puiRAM;

	if (uiOffset < HCP65_VIDEO_REGISTERS)
	{
		return mcROM.Read(uiOffset);
	}
	if (uiOffset < HCP65_VIDEO_DEVICE_RAM)
	{
		return ReadRegister(uiOffset & HCP65_VIDEO_DECODE);
	}
	puiRAM = GetDeviceRAM(uiOffset);
	return puiRAM ? *puiRAM : 0xFF;
}


//////////////////////////////////////////////////////////////////////////
//
// The device ROM is flash; writes to it are ignored.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65VideoBoard::Write(uint32 uiOffset, uint8 uiValue)
{
	uint8*	puiRAM;

	if (uiOffset < HCP65_VIDEO_REGISTERS)
	{
		return;
	}
	if (uiOffset < HCP65_VIDEO_DEVICE_RAM)
	{
		WriteRegister(uiOffset & HCP65_VIDEO_DECODE, uiValue);
		return;
	}
	puiRAM = GetDeviceRAM(uiOffset);
	if (puiRAM)
	{
		*puiRAM = uiValue;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The byte of device RAM at uiOffset in the slot, NULL where there is
// none ($00800-$03FFF).
//
//////////////////////////////////////////////////////////////////////////
uint8* CHCP65VideoBoard::GetDeviceRAM(uint32 uiOffset)
{
	if (uiOffset < HCP65_VIDEO_ROW_TABLE)
	{
		return NULL;
	}
	if (uiOffset < HCP65_VIDEO_RED)
	{
		return &mpuiRowTable[uiOffset - HCP65_VIDEO_ROW_TABLE];
	}
	if (uiOffset < HCP65_VIDEO_GREEN)
	{
		return &mpuiRed[uiOffset - HCP65_VIDEO_RED];
	}
	if (uiOffset < HCP65_VIDEO_BLUE)
	{
		return &mpuiGreen[uiOffset - HCP65_VIDEO_GREEN];
	}
	if (uiOffset < HCP65_VIDEO_BLUE + HCP65_VIDEO_PALETTE_SIZE)
	{
		return &mpuiBlue[uiOffset - HCP65_VIDEO_BLUE];
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// Reading the pixel address's low byte takes the whole address, for its
// high and bank bytes.
//
//////////////////////////////////////////////////////////////////////////
uint8 CHCP65VideoBoard::ReadRegister(uint32 uiRegister)
{
	uint8	uiValue;

	switch (uiRegister)
	{
	case HCP65_VIDEO_FLAGS:
		uiValue = (uint8)(muiFlags & 0x3F);
		if (mbEnable)
		{
			uiValue |= HCP65_VIDEO_FLAG_ENABLE;
		}
		if (IsInterrupting())
		{
			uiValue |= HCP65_VIDEO_FLAG_INTERRUPT;
		}
		return uiValue;

	case HCP65_VIDEO_ADDRESS_LOW:
		muiAddressSnapshot = GetPixelAddress();
		return (uint8)muiAddressSnapshot;

	case HCP65_VIDEO_ADDRESS_HIGH:
		return (uint8)(muiAddressSnapshot >> 8);

	case HCP65_VIDEO_ADDRESS_BANK:
		return (uint8)((muiAddressSnapshot >> 16) & 0x1F);

	case HCP65_VIDEO_H_FRONT_PORCH:
		return (uint8)GetHorizontal();

	case HCP65_VIDEO_H_SYNC:
		return (uint8)((GetHorizontal() >> 8) & 0x0F);

	case HCP65_VIDEO_V_FRONT_PORCH:
		return (uint8)muiLine;

	case HCP65_VIDEO_V_FRONT_PORCH + 1:
		return (uint8)((muiLine >> 8) & 0x0F);

	default:
		return 0xFF;
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65VideoBoard::WriteRegister(uint32 uiRegister, uint8 uiValue)
{
	int		iVertical;

	switch (uiRegister)
	{
	case HCP65_VIDEO_FLAGS:
		WriteFlags(uiValue);
		break;

	case HCP65_VIDEO_CLEAR_INTERRUPT:
		mbPending = false;
		break;

	case HCP65_VIDEO_CLOCK_SELECT:
		SetClock(uiValue);
		break;

	case HCP65_VIDEO_H_FRONT_PORCH:
	case HCP65_VIDEO_H_SYNC:
	case HCP65_VIDEO_H_BACK_PORCH:
	case HCP65_VIDEO_H_END:
		mauiHorizontal[uiRegister - HCP65_VIDEO_H_FRONT_PORCH] = uiValue;
		break;

	case HCP65_VIDEO_PATTERN_LOW:
		muiPattern = (uint16)((muiPattern & 0xFF0) | (uiValue & 0x0F));
		break;

	case HCP65_VIDEO_PATTERN_HIGH:
		muiPattern = (uint16)((muiPattern & 0x00F) | (uiValue << 4));
		break;

	case HCP65_VIDEO_PALETTE_BYTE:
		muiPaletteByte = uiValue;
		break;

	case HCP65_VIDEO_SIGNAL_DELAY:
		muiSignalDelay = uiValue;
		break;

	default:
		if ((uiRegister >= HCP65_VIDEO_V_FRONT_PORCH) && (uiRegister < HCP65_VIDEO_V_END + 2))
		{
			iVertical = (int)(uiRegister - HCP65_VIDEO_V_FRONT_PORCH) / 2;
			if ((uiRegister - HCP65_VIDEO_V_FRONT_PORCH) & 1)
			{
				mauiVertical[iVertical] = (uint16)((mauiVertical[iVertical] & 0x0FF) | ((uiValue & 0x03) << 8));
			}
			else
			{
				mauiVertical[iVertical] = (uint16)((mauiVertical[iVertical] & 0x300) | uiValue);
			}
		}
		break;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Enable can't be set while the board is held in reset.  The interrupt
// is cleared while Enable or the interrupt enable is off.  Memory Frame
// and Palette Write change what main memory has of the board.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65VideoBoard::WriteFlags(uint8 uiValue)
{
	uint8	uiOld;

	uiOld = muiFlags;
	muiFlags = (uint8)(uiValue & ~HCP65_VIDEO_FLAG_ENABLE);
	mbEnable = ((uiValue & HCP65_VIDEO_FLAG_ENABLE) != 0) && !mbHeldInReset;
	if (!mbEnable || !(muiFlags & HCP65_VIDEO_FLAG_INTERRUPT))
	{
		mbPending = false;
	}
	if (((uiOld ^ muiFlags) & (HCP65_VIDEO_FLAG_MEMORY_FRAME | HCP65_VIDEO_FLAG_PALETTE_WRITE)) && mpcHCP65)
	{
		mpcHCP65->MapMainMemory();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The pixel clock changes (or starts or stops) from the tick it has
// reached.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65VideoBoard::SetClock(uint8 uiClockSelect)
{
	Rebase();
	muiClockSelect = uiClockSelect;
}


//////////////////////////////////////////////////////////////////////////
//
// The clock is counted from the tick and cycle the board is at.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65VideoBoard::Rebase(void)
{
	muiRateTick = muiTick;
	muiRateCycle = muiCycle;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65VideoBoard::IsClockRunning(void) const
{
	return mpcHCP65 && !mbHeldInReset && (muiClockSelect & HCP65_VIDEO_CLOCK_ENABLE);
}


//////////////////////////////////////////////////////////////////////////
//
// The pixel clocks counted by CPU cycle uiCycle.
//
//////////////////////////////////////////////////////////////////////////
uint64 CHCP65VideoBoard::TickAt(uint64 uiCycle)
{
	uint64	uiRate;

	if (!IsClockRunning() || (uiCycle <= muiRateCycle))
	{
		return muiRateTick;
	}
	uiRate = GetPixelClockRate();
	return muiRateTick + mpcHCP65->DeviceTick(uiCycle, uiRate) - mpcHCP65->DeviceTick(muiRateCycle, uiRate);
}


//////////////////////////////////////////////////////////////////////////
//
// The timing makes lines and frames.
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65VideoBoard::IsTiming(void) const
{
	return (mauiHorizontal[3] != 0) && (mauiVertical[3] != 0);
}


//////////////////////////////////////////////////////////////////////////
//
// The lines that have ended by uiCycle, each shown row drawn as it ends,
// and the interrupt set as vertical blank starts.  More than two frames at
// once (the host has run the HCP65 a long way between accesses) skips the
// frames between without drawing them.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65VideoBoard::CatchUp(uint64 uiCycle)
{
	uint64	uiLength;
	uint64	uiFrame;
	uint64	uiFrames;

	if (!mpcHCP65 || (uiCycle <= muiCycle))
	{
		return;
	}
	muiCycle = uiCycle;
	muiTick = TickAt(uiCycle);
	if (!IsTiming())
	{
		muiLineStart = muiTick;
		return;
	}

	uiLength = GetLineLength();
	uiFrame = uiLength * mauiVertical[3];
	if (muiTick - muiLineStart > 2 * uiFrame)
	{
		uiFrames = (muiTick - muiLineStart) / uiFrame - 1;
		muiLineStart += uiFrames * uiFrame;
		if (mbEnable && (muiFlags & HCP65_VIDEO_FLAG_INTERRUPT) && (mauiVertical[0] < mauiVertical[3]))
		{
			mbPending = true;
		}
	}

	while (muiTick >= muiLineStart + uiLength)
	{
		EndLine();
		muiLineStart += uiLength;
		muiLine++;
		if (muiLine >= mauiVertical[3])
		{
			muiLine = 0;
			EndFrame();
		}
		if ((muiLine == mauiVertical[0]) && mbEnable && (muiFlags & HCP65_VIDEO_FLAG_INTERRUPT))
		{
			mbPending = true;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The line has ended: a shown row is drawn.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65VideoBoard::EndLine(void)
{
	if ((muiLine < mauiVertical[0]) && (muiLine < HCP65_VIDEO_MAX_ROWS))
	{
		DrawRow(muiLine, GetActiveWidth());
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The frame drawn becomes the one shown.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65VideoBoard::EndFrame(void)
{
	uint32*	puiFrame;

	mcFrameMutex.Lock();
	puiFrame = mpuiShown;
	mpuiShown = mpuiDrawing;
	mpuiDrawing = puiFrame;
	muiShownWidth = GetActiveWidth();
	muiShownHeight = GetActiveLines();
	muiFrames++;
	mcFrameMutex.Unlock();
}


//////////////////////////////////////////////////////////////////////////
//
// Row uiRow of the shown frame: from its row table entry's address, the
// pixel at the address each clock, the address moving on where the
// pattern's step is 1.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65VideoBoard::DrawRow(uint16 uiRow, uint32 uiWidth)
{
	uint32*	puiDest;
	uint8*	puiIndices;
	uint8*	puiPalettes;
	uint32	uiAddress;
	uint32	uiColour;
	uint32	i;
	int		iStep;

	puiDest = &mpuiDrawing[(size)uiRow * HCP65_VIDEO_MAX_WIDTH];
	puiIndices = mapuiChips[ShownFrame() * 2];
	puiPalettes = mapuiChips[ShownFrame() * 2 + 1];
	uiAddress = RowStart(uiRow);
	iStep = 0;
	for (i = 0; i < uiWidth; i++)
	{
		uiAddress &= HCP65_VIDEO_ADDRESS_MASK;
		uiColour = ((uint32)(puiPalettes[uiAddress] & HCP65_VIDEO_PALETTE_NUMBER) << 8) | puiIndices[uiAddress];
		puiDest[i] = 0xFF000000 | ((uint32)mpuiRed[uiColour] << 16) | ((uint32)mpuiGreen[uiColour] << 8) | mpuiBlue[uiColour];
		if (muiPattern & (1 << iStep))
		{
			uiAddress++;
		}
		iStep++;
		if (iStep == HCP65_VIDEO_PATTERN_STEPS)
		{
			iStep = 0;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Row uiRow's start address: its row table entry's Low, High and Bank.
//
//////////////////////////////////////////////////////////////////////////
uint32 CHCP65VideoBoard::RowStart(uint16 uiRow)
{
	uint8*	puiEntry;

	puiEntry = &mpuiRowTable[(uiRow % HCP65_VIDEO_MAX_ROWS) * 4];
	return ((uint32)puiEntry[0] | ((uint32)puiEntry[1] << 8) | ((uint32)puiEntry[2] << 16)) & HCP65_VIDEO_ADDRESS_MASK;
}


//////////////////////////////////////////////////////////////////////////
//
// How far the address has moved on after uiTicks pixel clocks of a line.
//
//////////////////////////////////////////////////////////////////////////
uint32 CHCP65VideoBoard::Advances(uint32 uiTicks)
{
	uint32	uiCount;
	uint32	uiPerPattern;
	uint32	i;

	uiPerPattern = 0;
	uiCount = 0;
	for (i = 0; i < HCP65_VIDEO_PATTERN_STEPS; i++)
	{
		if (muiPattern & (1 << i))
		{
			uiPerPattern++;
			if (i < uiTicks % HCP65_VIDEO_PATTERN_STEPS)
			{
				uiCount++;
			}
		}
	}
	return uiCount + (uiTicks / HCP65_VIDEO_PATTERN_STEPS) * uiPerPattern;
}


//////////////////////////////////////////////////////////////////////////
//
// The next cycle the interrupt will be set at: the start of the next
// vertical blank, while it is enabled and not set.
//
//////////////////////////////////////////////////////////////////////////
uint64 CHCP65VideoBoard::NextEvent(void)
{
	uint64	uiRate;
	uint64	uiLines;
	uint64	uiTick;

	if (!IsClockRunning() || !IsTiming() || !mbEnable || !(muiFlags & HCP65_VIDEO_FLAG_INTERRUPT) || mbPending || (mauiVertical[0] >= mauiVertical[3]))
	{
		return HCP65_NEVER;
	}
	uiLines = (muiLine < mauiVertical[0]) ? (uint64)(mauiVertical[0] - muiLine) : (uint64)(mauiVertical[3] - muiLine + mauiVertical[0]);
	uiTick = muiLineStart + uiLines * GetLineLength();
	uiRate = GetPixelClockRate();
	return mpcHCP65->CycleOfDeviceTick(mpcHCP65->DeviceTick(muiRateCycle, uiRate) + (uiTick - muiRateTick), uiRate);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65VideoBoard::IsInterrupting(void)
{
	return !mbHeldInReset && mbEnable && (muiFlags & HCP65_VIDEO_FLAG_INTERRUPT) && mbPending;
}


//////////////////////////////////////////////////////////////////////////
//
// The pixel clock stops, the output goes off, Enable and the interrupt
// clear and the frame starts again.  The other registers are kept.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65VideoBoard::Reset(void)
{
	if (mpcHCP65)
	{
		CatchUp(mpcHCP65->NumCycles());
	}
	SetClock(0);
	muiSignalDelay = HCP65_VIDEO_DELAY_OFF;
	mbEnable = false;
	mbPending = false;
	muiLine = 0;
	muiLineStart = muiTick;
}


//////////////////////////////////////////////////////////////////////////
//
// The clock is counted from now either way.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65VideoBoard::SetHeldInReset(bool bHeld)
{
	if (mpcHCP65)
	{
		CatchUp(mpcHCP65->NumCycles());
	}
	Rebase();
	CHCP65Device::SetHeldInReset(bHeld);
}


//////////////////////////////////////////////////////////////////////////
//
// Window A is the processor's frame's indices, window B its palette
// numbers.
//
//////////////////////////////////////////////////////////////////////////
uint8* CHCP65VideoBoard::GetMemoryChip(int iChip)
{
	if ((iChip < 0) || (iChip > 1))
	{
		return NULL;
	}
	return mapuiChips[ProcessorFrame() * 2 + iChip];
}


//////////////////////////////////////////////////////////////////////////
//
// While Palette Write is set the board sees window A's writes.
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65VideoBoard::IsMemoryChipWatched(int iChip)
{
	return (iChip == 0) && (muiFlags & HCP65_VIDEO_FLAG_PALETTE_WRITE);
}


//////////////////////////////////////////////////////////////////////////
//
// A pixel write: the index, and the Palette Byte at the same offset in
// window B.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65VideoBoard::WriteMemoryChip(int iChip, uint32 uiOffset, uint8 uiValue)
{
	int		iFrame;

	iFrame = ProcessorFrame();
	mapuiChips[iFrame * 2 + iChip][uiOffset & HCP65_VIDEO_ADDRESS_MASK] = uiValue;
	if ((iChip == 0) && (muiFlags & HCP65_VIDEO_FLAG_PALETTE_WRITE))
	{
		mapuiChips[iFrame * 2 + 1][uiOffset & HCP65_VIDEO_ADDRESS_MASK] = muiPaletteByte;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The last whole frame into puiDest (uiMaxWidth pixels a row), as much of
// it as fits, and its size.  False, with a size of 0, if there is no
// picture.  Any thread.
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65VideoBoard::GetFrame(uint32* puiDest, uint32 uiMaxWidth, uint32 uiMaxHeight, uint32* puiWidth, uint32* puiHeight)
{
	uint32	uiWidth;
	uint32	uiHeight;
	uint32	y;

	mcFrameMutex.Lock();
	if (!IsSignal() || (muiShownWidth == 0) || (muiShownHeight == 0))
	{
		mcFrameMutex.Unlock();
		*puiWidth = 0;
		*puiHeight = 0;
		return false;
	}
	uiWidth = (muiShownWidth < uiMaxWidth) ? muiShownWidth : uiMaxWidth;
	uiHeight = (muiShownHeight < uiMaxHeight) ? muiShownHeight : uiMaxHeight;
	for (y = 0; y < uiHeight; y++)
	{
		memcpy(&puiDest[(size)y * uiMaxWidth], &mpuiShown[(size)y * HCP65_VIDEO_MAX_WIDTH], uiWidth * sizeof(uint32));
	}
	*puiWidth = muiShownWidth;
	*puiHeight = muiShownHeight;
	mcFrameMutex.Unlock();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// There is a picture: the clock runs, the output is on, the timing makes
// frames and one has been drawn.
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65VideoBoard::IsSignal(void) const
{
	return IsClockRunning() && !(muiSignalDelay & HCP65_VIDEO_DELAY_OFF) && IsTiming() && (muiFrames > 0);
}


uint64 CHCP65VideoBoard::NumFrames(void) const { return muiFrames; }
uint64 CHCP65VideoBoard::GetPixelClockRate(void) const { return gauiHCP65VideoClocks[muiClockSelect & HCP65_VIDEO_CLOCK_RATE]; }
uint32 CHCP65VideoBoard::GetLineLength(void) const { return 8 * (uint32)mauiHorizontal[3]; }
uint32 CHCP65VideoBoard::GetFrameLines(void) const { return mauiVertical[3]; }
uint16 CHCP65VideoBoard::GetLine(void) const { return muiLine; }
int CHCP65VideoBoard::ProcessorFrame(void) { return (muiFlags & HCP65_VIDEO_FLAG_MEMORY_FRAME) ? 1 : 0; }
int CHCP65VideoBoard::ShownFrame(void) { return 1 - ProcessorFrame(); }
uint64 CHCP65VideoBoard::GetClockRate(int iSelect) { return gauiHCP65VideoClocks[iSelect & HCP65_VIDEO_CLOCK_RATE]; }


//////////////////////////////////////////////////////////////////////////
//
// Frame iFrame's indices (iKind 0) or palette numbers (1).
//
//////////////////////////////////////////////////////////////////////////
uint8* CHCP65VideoBoard::GetChip(int iFrame, int iKind)
{
	return mapuiChips[(iFrame & 1) * 2 + (iKind & 1)];
}


//////////////////////////////////////////////////////////////////////////
//
// The active part of a line: 8 x Front Porch Start, inside the line and
// at most HCP65_VIDEO_MAX_WIDTH.
//
//////////////////////////////////////////////////////////////////////////
uint32 CHCP65VideoBoard::GetActiveWidth(void) const
{
	uint32	uiWidth;

	uiWidth = 8 * (uint32)mauiHorizontal[0];
	if (uiWidth > GetLineLength())
	{
		uiWidth = GetLineLength();
	}
	return (uiWidth < HCP65_VIDEO_MAX_WIDTH) ? uiWidth : HCP65_VIDEO_MAX_WIDTH;
}


//////////////////////////////////////////////////////////////////////////
//
// The lines shown: Front Porch Start, inside the frame and at most
// HCP65_VIDEO_MAX_ROWS.
//
//////////////////////////////////////////////////////////////////////////
uint32 CHCP65VideoBoard::GetActiveLines(void) const
{
	uint32	uiLines;

	uiLines = mauiVertical[0];
	if (uiLines > mauiVertical[3])
	{
		uiLines = mauiVertical[3];
	}
	return (uiLines < HCP65_VIDEO_MAX_ROWS) ? uiLines : HCP65_VIDEO_MAX_ROWS;
}


//////////////////////////////////////////////////////////////////////////
//
// The horizontal counter: 1 at the start of a line.  0 without timing.
//
//////////////////////////////////////////////////////////////////////////
uint32 CHCP65VideoBoard::GetHorizontal(void) const
{
	if (!IsTiming())
	{
		return 0;
	}
	return (uint32)(muiTick - muiLineStart) + 1;
}


//////////////////////////////////////////////////////////////////////////
//
// The pixel address counter: the line's start address moved on by the
// pixel clocks of its active part so far.
//
//////////////////////////////////////////////////////////////////////////
uint32 CHCP65VideoBoard::GetPixelAddress(void)
{
	uint64	uiTicks;

	if (!IsTiming())
	{
		return RowStart(muiLine);
	}
	uiTicks = muiTick - muiLineStart;
	if (uiTicks > GetActiveWidth())
	{
		uiTicks = GetActiveWidth();
	}
	return (RowStart(muiLine) + Advances((uint32)uiTicks)) & HCP65_VIDEO_ADDRESS_MASK;
}

