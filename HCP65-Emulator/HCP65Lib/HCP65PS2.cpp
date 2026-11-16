#include <string.h>
#include "HCP65.h"
#include "HCP65PS2.h"


//////////////////////////////////////////////////////////////////////////
//
// Nothing pulls the lines low and nothing is waiting.  The port starts
// held, as the board does.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65PS2Port::Init(void)
{
	mbBoardClockLow = false;
	mbBoardDataLow = false;
	mbDeviceClockLow = false;
	mbDeviceDataLow = false;
	muiTick = 0;

	mbHeld = true;
	muiShift = 0;
	mbPacketPulse = false;
	muiPacketEnd = HCP65_NEVER;
	muiCodeEnd = HCP65_NEVER;
	mbPacketWaiting = false;
	mbCodeWaiting = false;
	memset(mauiFIFO, 0, sizeof(mauiFIFO));
	muiFIFOFirst = 0;
	muiFIFOCount = 0;
	muiFIFOOutput = 0;

	meDevice = HPD_Idle;
	muiDeviceBit = 0;
	muiDevicePhase = 0;
	muiDeviceFrame = 0;
	muiDeviceEvent = HCP65_NEVER;
	muiIdleSince = 0;
	muiFrameEnd = 0;
	macHostSend.Init();
	muiHostSendIndex = 0;
	macHostReceived.Init();
	muiAborts = 0;
	muiErrors = 0;
	mpcDevice = NULL;
	mbPlugged = true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65PS2Port::Kill(void)
{
	macHostReceived.Kill();
	macHostSend.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The board's ~Reset: the shift registers (~MR), the flags' flip-flops
// and the FIFO (MR, as it is meant to be wired).  The FIFO's last byte
// out stays, as the 74LVC574 holding it isn't reset.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65PS2Port::Reset(void)
{
	muiShift = 0;
	mbPacketWaiting = false;
	mbCodeWaiting = false;
	muiFIFOFirst = 0;
	muiFIFOCount = 0;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65PS2Port::SetHeld(bool bHeld)
{
	mbHeld = bHeld;
	if (bHeld)
	{
		Reset();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// What the board's GPIO does to the lines.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65PS2Port::SetBoardDrive(bool bClockLow, bool bDataLow)
{
	bool	bOldClock;
	bool	bOldData;

	bOldClock = IsClockHigh();
	bOldData = IsDataHigh();
	mbBoardDataLow = bDataLow;
	LinesChanged(bOldClock, bOldData);
	bOldData = IsDataHigh();
	mbBoardClockLow = bClockLow;
	LinesChanged(bOldClock, bOldData);
}


//////////////////////////////////////////////////////////////////////////
//
// What is plugged in, if it is more than the host's calls; NULL to take
// it out.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65PS2Port::SetDevice(CHCP65PS2Device* pcDevice)
{
	mpcDevice = pcDevice;
}


CHCP65PS2Device* CHCP65PS2Port::GetDevice(void) { return mpcDevice; }
bool CHCP65PS2Port::IsPlugged(void) { return mbPlugged; }


//////////////////////////////////////////////////////////////////////////
//
// Something is plugged into the port (the far end), or nothing is.
// Unplugged, the far end lets go of the lines and stops what it was doing;
// what it had to send it sends once plugged in again.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65PS2Port::SetPlugged(bool bPlugged)
{
	mbPlugged = bPlugged;
	if (!bPlugged)
	{
		meDevice = HPD_Idle;
		muiDeviceEvent = HCP65_NEVER;
		SetDeviceDrive(false, false);
	}
	else
	{
		ScheduleSend();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The far end's frames and the receiver's timers up to uiTick, an event
// at a time, after the device has sent what it has.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65PS2Port::CatchUp(uint64 uiTick)
{
	uint64	uiNext;

	if (mpcDevice)
	{
		mpcDevice->CatchUp(this);
	}
	for (;;)
	{
		uiNext = NextEvent();
		if (uiNext > uiTick)
		{
			break;
		}
		if (uiNext > muiTick)
		{
			muiTick = uiNext;
		}
		if (muiPacketEnd == uiNext)
		{
			EndPacket();
		}
		else if (muiCodeEnd == uiNext)
		{
			EndCode();
		}
		else
		{
			DeviceStep();
		}
	}
	if (uiTick > muiTick)
	{
		muiTick = uiTick;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The next tick anything happens at on its own.
//
//////////////////////////////////////////////////////////////////////////
uint64 CHCP65PS2Port::NextEvent(void)
{
	uint64	uiNext;

	uiNext = muiPacketEnd;
	if (muiCodeEnd < uiNext)
	{
		uiNext = muiCodeEnd;
	}
	if (muiDeviceEvent < uiNext)
	{
		uiNext = muiDeviceEvent;
	}
	return uiNext;
}


//////////////////////////////////////////////////////////////////////////
//
// The 74LVC574 takes the FIFO's first byte on the read, and the FIFO lets
// it go.
//
//////////////////////////////////////////////////////////////////////////
uint8 CHCP65PS2Port::ReadFIFO(void)
{
	if (muiFIFOCount > 0)
	{
		muiFIFOOutput = mauiFIFO[muiFIFOFirst];
		muiFIFOFirst = (uint8)((muiFIFOFirst + 1) % HCP65_PS2_FIFO_SIZE);
		muiFIFOCount--;
	}
	return muiFIFOOutput;
}


//////////////////////////////////////////////////////////////////////////
//
// A write to the port's Clear Interrupt.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65PS2Port::ClearWaiting(void)
{
	mbPacketWaiting = false;
	mbCodeWaiting = false;
}


bool CHCP65PS2Port::IsPacketWaiting(void) { return mbPacketWaiting; }
bool CHCP65PS2Port::IsCodeWaiting(void) { return mbCodeWaiting; }
bool CHCP65PS2Port::IsOutputReady(void) { return muiFIFOCount > 0; }
bool CHCP65PS2Port::IsClockHigh(void) { return !mbBoardClockLow && !mbDeviceClockLow; }
bool CHCP65PS2Port::IsDataHigh(void) { return !mbBoardDataLow && !mbDeviceDataLow; }
size CHCP65PS2Port::NumHostToSend(void) { return macHostSend.NumElements() - muiHostSendIndex; }
size CHCP65PS2Port::NumHostReceived(void) { return macHostReceived.NumElements(); }
uint64 CHCP65PS2Port::NumAborts(void) { return muiAborts; }
uint64 CHCP65PS2Port::NumErrors(void) { return muiErrors; }
uint64 CHCP65PS2Port::GetTick(void) { return muiTick; }


//////////////////////////////////////////////////////////////////////////
//
// The far end will send uiLength more bytes.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65PS2Port::HostSend(const uint8* puiData, size uiLength)
{
	if (muiHostSendIndex == macHostSend.NumElements())
	{
		macHostSend.ReInit();
		muiHostSendIndex = 0;
	}
	macHostSend.Add((uint8*)puiData, uiLength);
	ScheduleSend();
}


//////////////////////////////////////////////////////////////////////////
//
// What the computer sent the far end, up to uiMaxLength bytes, taken.
//
//////////////////////////////////////////////////////////////////////////
size CHCP65PS2Port::HostReceive(uint8* puiDest, size uiMaxLength)
{
	size	uiLength;

	uiLength = macHostReceived.NumElements();
	if (uiLength > uiMaxLength)
	{
		uiLength = uiMaxLength;
	}
	if (uiLength > 0)
	{
		memcpy(puiDest, macHostReceived.GetData(), uiLength);
		macHostReceived.RemoveRange(0, uiLength);
	}
	return uiLength;
}


//////////////////////////////////////////////////////////////////////////
//
// What the far end does to the lines, Data first.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65PS2Port::SetDeviceDrive(bool bClockLow, bool bDataLow)
{
	bool	bOldClock;
	bool	bOldData;

	bOldClock = IsClockHigh();
	bOldData = IsDataHigh();
	mbDeviceDataLow = bDataLow;
	LinesChanged(bOldClock, bOldData);
	bOldData = IsDataHigh();
	mbDeviceClockLow = bClockLow;
	LinesChanged(bOldClock, bOldData);
}


//////////////////////////////////////////////////////////////////////////
//
// The lines were bOldClock and bOldData.  A fall of Clock shifts Data in
// and starts (or keeps up) a packet; a rise starts the packet's quiet.
// The computer letting Clock go with Data low is a request to send.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65PS2Port::LinesChanged(bool bOldClock, bool bOldData)
{
	bool	bClock;
	bool	bData;

	bClock = IsClockHigh();
	bData = IsDataHigh();
	if (bOldClock && !bClock)
	{
		if (!mbHeld)
		{
			muiShift = (uint16)((muiShift << 1) | (bData ? 1 : 0));
		}
		mbPacketPulse = true;
		muiPacketEnd = HCP65_NEVER;
		muiCodeEnd = HCP65_NEVER;
	}
	else if (!bOldClock && bClock)
	{
		muiPacketEnd = muiTick + HCP65_PS2_PACKET_QUIET;
		if (mbPlugged && (meDevice != HPD_Receiving) && !bData && mbBoardDataLow)
		{
			//A frame it was sending is sent again later.
			if (meDevice == HPD_Sending)
			{
				muiAborts++;
			}
			meDevice = HPD_Receiving;
			mbDeviceDataLow = false;
			muiDeviceBit = 1;
			muiDevicePhase = 0;
			muiDeviceFrame = 0;
			muiDeviceEvent = muiTick + HCP65_PS2_RTS_TICKS;
		}
	}

	if (bClock && bData && !(bOldClock && bOldData))
	{
		muiIdleSince = muiTick;
		ScheduleSend();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Clock has been high long enough: Packet Pulse falls, setting Packet
// Waiting, and the byte goes into the FIFO.  The code's quiet starts.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65PS2Port::EndPacket(void)
{
	mbPacketPulse = false;
	muiPacketEnd = HCP65_NEVER;
	muiCodeEnd = muiTick + HCP65_PS2_CODE_QUIET;
	if (mbHeld)
	{
		return;
	}
	mbPacketWaiting = true;
	if (muiFIFOCount < HCP65_PS2_FIFO_SIZE)
	{
		mauiFIFO[(muiFIFOFirst + muiFIFOCount) % HCP65_PS2_FIFO_SIZE] = LatchedByte();
		muiFIFOCount++;
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65PS2Port::EndCode(void)
{
	muiCodeEnd = HCP65_NEVER;
	if (!mbHeld)
	{
		mbCodeWaiting = true;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The data bits of the last eleven shifted in, as the 74LVC595s' outputs
// are wired: D0 came in nine bits before the stop bit.
//
//////////////////////////////////////////////////////////////////////////
uint8 CHCP65PS2Port::LatchedByte(void)
{
	uint8	uiByte;
	int		i;

	uiByte = 0;
	for (i = 0; i < 8; i++)
	{
		if (muiShift & (1 << (9 - i)))
		{
			uiByte |= (uint8)(1 << i);
		}
	}
	return uiByte;
}


//////////////////////////////////////////////////////////////////////////
//
// The far end's event at muiDeviceEvent.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65PS2Port::DeviceStep(void)
{
	muiDeviceEvent = HCP65_NEVER;
	switch (meDevice)
	{
	case HPD_Idle:
		if ((NumHostToSend() > 0) && IsClockHigh() && IsDataHigh() && (muiTick >= muiIdleSince + HCP65_PS2_IDLE_TICKS) && (muiTick >= muiFrameEnd + HCP65_PS2_BYTE_GAP))
		{
			StartFrame();
		}
		else
		{
			ScheduleSend();
		}
		break;

	case HPD_Sending:
		StepSending();
		break;

	case HPD_Receiving:
		StepReceiving();
		break;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A bit: Data, then Clock low, then Clock high.  Clock held low under the
// far end gives the frame up.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65PS2Port::StepSending(void)
{
	bool	bBit;

	switch (muiDevicePhase)
	{
	case 0:
		bBit = ((muiDeviceFrame >> muiDeviceBit) & 1) != 0;
		SetDeviceDrive(false, !bBit);
		muiDevicePhase = 1;
		muiDeviceEvent = muiTick + HCP65_PS2_SETUP_TICKS;
		break;

	case 1:
		if (!IsClockHigh())
		{
			AbortFrame();
			return;
		}
		SetDeviceDrive(true, mbDeviceDataLow);
		muiDevicePhase = 2;
		muiDeviceEvent = muiTick + HCP65_PS2_HALF_TICKS;
		break;

	default:
		SetDeviceDrive(false, mbDeviceDataLow);
		if (!IsClockHigh())
		{
			AbortFrame();
			return;
		}
		muiDeviceBit++;
		if (muiDeviceBit < 11)
		{
			muiDevicePhase = 0;
			muiDeviceEvent = muiTick + HCP65_PS2_BIT_TICKS - HCP65_PS2_SETUP_TICKS - HCP65_PS2_HALF_TICKS;
			break;
		}
		SetDeviceDrive(false, false);
		muiHostSendIndex++;
		meDevice = HPD_Idle;
		muiFrameEnd = muiTick;
		ScheduleSend();
		break;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A clock of the computer's byte.  Data is read as Clock rises, for
// clocks 1 to 10 (eight data bits, parity and stop); clock 11 is the far
// end's acknowledge, Data low.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65PS2Port::StepReceiving(void)
{
	if (muiDevicePhase == 0)
	{
		SetDeviceDrive(true, muiDeviceBit == 11);
		muiDevicePhase = 1;
		muiDeviceEvent = muiTick + HCP65_PS2_HALF_TICKS;
		return;
	}

	SetDeviceDrive(false, mbDeviceDataLow);
	if (muiDeviceBit <= 10)
	{
		if (IsDataHigh())
		{
			muiDeviceFrame |= (uint16)(1 << (muiDeviceBit - 1));
		}
	}
	else
	{
		SetDeviceDrive(false, false);
		FinishReceiving();
		return;
	}
	muiDeviceBit++;
	muiDevicePhase = 0;
	muiDeviceEvent = muiTick + HCP65_PS2_HALF_TICKS;
}


//////////////////////////////////////////////////////////////////////////
//
// The byte is the host's if its parity is odd and its stop bit is 1.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65PS2Port::FinishReceiving(void)
{
	uint8	uiByte;
	int		iOnes;
	int		i;

	iOnes = 0;
	for (i = 0; i < 9; i++)
	{
		iOnes += (muiDeviceFrame >> i) & 1;
	}
	uiByte = (uint8)muiDeviceFrame;
	meDevice = HPD_Idle;
	muiFrameEnd = muiTick;
	if ((iOnes & 1) && (muiDeviceFrame & 0x200))
	{
		if (mpcDevice)
		{
			mpcDevice->Received(this, uiByte);
		}
		else
		{
			macHostReceived.Add(uiByte);
		}
	}
	else
	{
		muiErrors++;
	}
	ScheduleSend();
}


//////////////////////////////////////////////////////////////////////////
//
// While the far end is idle with something to send, it looks again when
// the lines will have been idle long enough and the gap since its last
// frame is over.  While a line is low it waits for them to go high.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65PS2Port::ScheduleSend(void)
{
	uint64	uiStart;

	if (!mbPlugged || (meDevice != HPD_Idle) || (NumHostToSend() == 0) || !IsClockHigh() || !IsDataHigh())
	{
		if (meDevice == HPD_Idle)
		{
			muiDeviceEvent = HCP65_NEVER;
		}
		return;
	}
	uiStart = muiIdleSince + HCP65_PS2_IDLE_TICKS;
	if (muiFrameEnd + HCP65_PS2_BYTE_GAP > uiStart)
	{
		uiStart = muiFrameEnd + HCP65_PS2_BYTE_GAP;
	}
	if (muiTick > uiStart)
	{
		uiStart = muiTick;
	}
	muiDeviceEvent = uiStart;
}


//////////////////////////////////////////////////////////////////////////
//
// Start (0), the eight data bits from D0, odd parity and stop (1).
//
//////////////////////////////////////////////////////////////////////////
void CHCP65PS2Port::StartFrame(void)
{
	uint8	uiByte;
	int		iOnes;
	int		i;

	uiByte = macHostSend.GetValue(muiHostSendIndex);
	iOnes = 0;
	for (i = 0; i < 8; i++)
	{
		iOnes += (uiByte >> i) & 1;
	}
	muiDeviceFrame = (uint16)((uiByte << 1) | ((iOnes & 1) ? 0 : 0x200) | 0x400);
	meDevice = HPD_Sending;
	muiDeviceBit = 0;
	muiDevicePhase = 0;
	StepSending();
}


//////////////////////////////////////////////////////////////////////////
//
// The computer held Clock low: the far end lets go and sends the byte
// again later.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65PS2Port::AbortFrame(void)
{
	muiAborts++;
	meDevice = HPD_Idle;
	muiFrameEnd = muiTick;
	SetDeviceDrive(false, false);
	ScheduleSend();
}


//////////////////////////////////////////////////////////////////////////
//
// Held in reset with Enable off; the ROM's registers are the five Stage
// B decodes.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65PS2Board::Init(void)
{
	int		i;

	CHCP65Device::Init(HCP65_SMALL_SLOT_SIZE);

	mcROM.Init(HCP65_PS2_BOARD_NAME, HCP65_DEVICE_MANUFACTURER, 0, DEVICE_BUS_32KB, 0, HCP65_PS2_DRIVER);
	for (i = HCP65_PS2_FLAGS; i <= HCP65_PS2_DIRECT; i++)
	{
		mcROM.AddRegister((uint16)(HCP65_PS2_REGISTERS + i), DEVICE_REGISTER_READ);
		mcROM.AddRegister((uint16)(HCP65_PS2_REGISTERS + i), DEVICE_REGISTER_WRITE);
	}
	mcROM.AddRegister(HCP65_PS2_REGISTERS + HCP65_PS2_STATE, DEVICE_REGISTER_READ);

	for (i = 0; i < HCP65_PS2_NUM_PORTS; i++)
	{
		macPorts[i].Init();
	}
	muiFlags = 0;
	mbEnable = false;
	muiGPIO = 0;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65PS2Board::Kill(void)
{
	int		i;

	for (i = 0; i < HCP65_PS2_NUM_PORTS; i++)
	{
		macPorts[i].Kill();
	}
	CHCP65Device::Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Below $400 the device ROM; from $400 the Stage B, where A3 and A4 are
// low.  Nothing else drives the bus.
//
//////////////////////////////////////////////////////////////////////////
uint8 CHCP65PS2Board::Read(uint32 uiOffset)
{
	uint32	uiDecode;
	uint8	uiValue;

	if (uiOffset < HCP65_PS2_REGISTERS)
	{
		return mcROM.Read(uiOffset);
	}
	if (uiOffset >= HCP65_PS2_REGISTERS_END)
	{
		return 0xFF;
	}

	uiDecode = uiOffset & HCP65_PS2_DECODE;
	switch (uiDecode)
	{
	case HCP65_PS2_FLAGS:
		uiValue = (uint8)(muiFlags & 0x3F);
		if (mbEnable)
		{
			uiValue |= HCP65_PS2_FLAG_ENABLE;
		}
		if (IsInterrupting())
		{
			uiValue |= HCP65_PS2_FLAG_INTERRUPT;
		}
		return uiValue;

	case HCP65_PS2_FIFO_1:
		return macPorts[0].ReadFIFO();

	case HCP65_PS2_FIFO_2:
		return macPorts[1].ReadFIFO();

	case HCP65_PS2_DIRECT:
		return ReadLines();

	case HCP65_PS2_STATE:
		return ReadState();

	default:
		return 0xFF;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The device ROM is flash.  Enable can't be set while the board is held
// in reset.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65PS2Board::Write(uint32 uiOffset, uint8 uiValue)
{
	if ((uiOffset < HCP65_PS2_REGISTERS) || (uiOffset >= HCP65_PS2_REGISTERS_END))
	{
		return;
	}

	switch (uiOffset & HCP65_PS2_DECODE)
	{
	case HCP65_PS2_FLAGS:
		muiFlags = (uint8)(uiValue & ~HCP65_PS2_FLAG_ENABLE);
		mbEnable = ((uiValue & HCP65_PS2_FLAG_ENABLE) != 0) && !mbHeldInReset;
		UpdateDrive();
		break;

	case HCP65_PS2_FIFO_1:
		macPorts[0].ClearWaiting();
		break;

	case HCP65_PS2_FIFO_2:
		macPorts[1].ClearWaiting();
		break;

	case HCP65_PS2_DIRECT:
		muiGPIO = uiValue;
		UpdateDrive();
		break;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// ~Reset clears the ports and turns Enable off; Flags' other bits and the
// GPIO's latch are kept.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65PS2Board::Reset(void)
{
	int		i;

	for (i = 0; i < HCP65_PS2_NUM_PORTS; i++)
	{
		macPorts[i].Reset();
	}
	mbEnable = false;
	UpdateDrive();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65PS2Board::SetHeldInReset(bool bHeld)
{
	int		i;

	CHCP65Device::SetHeldInReset(bHeld);
	for (i = 0; i < HCP65_PS2_NUM_PORTS; i++)
	{
		macPorts[i].SetHeld(bHeld);
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65PS2Board::CatchUp(uint64 uiCycle)
{
	uint64	uiTick;
	int		i;

	if (!mpcHCP65)
	{
		return;
	}
	uiTick = mpcHCP65->DeviceTick(uiCycle, HCP65_PS2_TICK_RATE);
	for (i = 0; i < HCP65_PS2_NUM_PORTS; i++)
	{
		macPorts[i].CatchUp(uiTick);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The interrupt can only change on its own while it is enabled.
//
//////////////////////////////////////////////////////////////////////////
uint64 CHCP65PS2Board::NextEvent(void)
{
	uint64	uiNext;
	uint64	uiEvent;
	int		i;

	if (!mpcHCP65 || mbHeldInReset || !mbEnable || !(muiFlags & HCP65_PS2_FLAG_INTERRUPT))
	{
		return HCP65_NEVER;
	}
	uiNext = HCP65_NEVER;
	for (i = 0; i < HCP65_PS2_NUM_PORTS; i++)
	{
		uiEvent = macPorts[i].NextEvent();
		if (uiEvent < uiNext)
		{
			uiNext = uiEvent;
		}
	}
	return (uiNext == HCP65_NEVER) ? HCP65_NEVER : mpcHCP65->CycleOfDeviceTick(uiNext, HCP65_PS2_TICK_RATE);
}


//////////////////////////////////////////////////////////////////////////
//
// Flags & Interrupt: Enable and the interrupt enable on, and a packet
// waiting on either port.
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65PS2Board::IsInterrupting(void)
{
	return !mbHeldInReset && mbEnable && (muiFlags & HCP65_PS2_FLAG_INTERRUPT) && (macPorts[0].IsPacketWaiting() || macPorts[1].IsPacketWaiting());
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
CHCP65PS2Port* CHCP65PS2Board::GetPort(int iPort)
{
	return ((iPort >= 0) && (iPort < HCP65_PS2_NUM_PORTS)) ? &macPorts[iPort] : NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The State Reader's S0-S7: each port's ~Packet Waiting, ~Code Waiting
// and Data-Out Ready, S3 and S7 low.
//
//////////////////////////////////////////////////////////////////////////
uint8 CHCP65PS2Board::ReadState(void)
{
	uint8	uiState;
	int		i;

	uiState = 0;
	for (i = 0; i < HCP65_PS2_NUM_PORTS; i++)
	{
		if (!macPorts[i].IsPacketWaiting())
		{
			uiState |= (uint8)(HCP65_PS2_STATE_NO_PACKET_1 << (i * HCP65_PS2_STATE_PORT_SHIFT));
		}
		if (!macPorts[i].IsCodeWaiting())
		{
			uiState |= (uint8)(HCP65_PS2_STATE_NO_CODE_1 << (i * HCP65_PS2_STATE_PORT_SHIFT));
		}
		if (macPorts[i].IsOutputReady())
		{
			uiState |= (uint8)(HCP65_PS2_STATE_READY_1 << (i * HCP65_PS2_STATE_PORT_SHIFT));
		}
	}
	return uiState;
}


//////////////////////////////////////////////////////////////////////////
//
// The GPIO reads the lines: D0 port 1's Data, D1 its Clock, D2 and D3 port
// 2's.  D4-D7 are tied low.
//
//////////////////////////////////////////////////////////////////////////
uint8 CHCP65PS2Board::ReadLines(void)
{
	uint8	uiLines;
	int		i;

	uiLines = 0;
	for (i = 0; i < HCP65_PS2_NUM_PORTS; i++)
	{
		if (macPorts[i].IsDataHigh())
		{
			uiLines |= (uint8)(HCP65_PS2_DIRECT_DATA_1 << (i * 2));
		}
		if (macPorts[i].IsClockHigh())
		{
			uiLines |= (uint8)(HCP65_PS2_DIRECT_CLOCK_1 << (i * 2));
		}
	}
	return uiLines;
}


//////////////////////////////////////////////////////////////////////////
//
// The GPIO pulls a line low where its output is enabled with a low
// level, while the board is enabled.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65PS2Board::UpdateDrive(void)
{
	uint8	uiLow;
	int		i;

	uiLow = mbEnable ? (uint8)((muiGPIO >> HCP65_PS2_DIRECT_DRIVE) & ~muiGPIO & 0x0F) : 0;
	for (i = 0; i < HCP65_PS2_NUM_PORTS; i++)
	{
		macPorts[i].SetBoardDrive((uiLow & (HCP65_PS2_DIRECT_CLOCK_1 << (i * 2))) != 0, (uiLow & (HCP65_PS2_DIRECT_DATA_1 << (i * 2))) != 0);
	}
}

