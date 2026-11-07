#include <string.h>
#include "HCP65.h"
#include "HCP65UART.h"


//////////////////////////////////////////////////////////////////////////
//
// Power on: master reset, the divisor latch 1 and the far end's lines off.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65UART::Init(EHCP65UARTModule eModule)
{
	meModule = eModule;
	muiDivisor = 1;
	muiSCR = 0;
	muiTick = 0;
	macHostSend.Init();
	muiHostSendIndex = 0;
	macHostReceived.Init();
	mbHostCTS = false;
	mbHostDSR = false;
	mbHostDCD = false;
	muiOverruns = 0;
	Reset();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65UART::Kill(void)
{
	macHostReceived.Kill();
	macHostSend.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Master reset: IER, FCR, LCR and MCR cleared, the FIFOs emptied and the
// transmitter and receiver idle.  The divisor latch and SCR are kept.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65UART::Reset(void)
{
	muiIER = 0;
	muiLCR = 0;
	muiMCR = 0;
	muiFCR = 0;
	muiLineErrors = 0;
	mbTransmitInterrupt = false;
	ClearRx();
	ClearTx();
	mbShifting = false;
	mbReceiving = false;
	muiRxActivity = muiTick;
	muiModemInputs = ModemInputs();
	muiModemDeltas = 0;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint8 CHCP65UART::Read(uint8 uiRegister)
{
	uint8	uiValue;

	uiValue = 0xFF;
	switch (uiRegister)
	{
	case HCP65_UART_RBR:
		if (muiLCR & HCP65_UART_LCR_DLAB)
		{
			uiValue = (uint8)muiDivisor;
		}
		else
		{
			uiValue = 0;
			if (muiRxCount > 0)
			{
				uiValue = mauiRxFIFO[muiRxFirst];
				muiRxFirst = (uint8)((muiRxFirst + 1) % HCP65_UART_FIFO_SIZE);
				muiRxCount--;
			}
			muiRxActivity = muiTick;
			StartReceiving();
		}
		break;

	case HCP65_UART_IER:
		uiValue = (muiLCR & HCP65_UART_LCR_DLAB) ? (uint8)(muiDivisor >> 8) : muiIER;
		break;

	case HCP65_UART_IIR:
		uiValue = Identify();
		if (uiValue == HCP65_UART_IIR_TRANSMIT)
		{
			mbTransmitInterrupt = false;
		}
		if (muiFCR & HCP65_UART_FCR_ENABLE)
		{
			uiValue |= HCP65_UART_IIR_FIFOS;
		}
		break;

	case HCP65_UART_LCR:
		uiValue = muiLCR;
		break;

	case HCP65_UART_MCR:
		uiValue = muiMCR;
		break;

	case HCP65_UART_LSR:
		uiValue = ReadLineStatus();
		muiLineErrors = 0;
		break;

	case HCP65_UART_MSR:
		uiValue = ReadModemStatus();
		muiModemDeltas = 0;
		break;

	case HCP65_UART_SCR:
		uiValue = muiSCR;
		break;
	}
	return uiValue;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65UART::Write(uint8 uiRegister, uint8 uiValue)
{
	uint8	uiOldIER;
	uint8	uiIndex;

	switch (uiRegister)
	{
	case HCP65_UART_RBR:
		if (muiLCR & HCP65_UART_LCR_DLAB)
		{
			muiDivisor = (uint16)((muiDivisor & 0xFF00) | uiValue);
		}
		else
		{
			if (muiTxCount < FIFODepth())
			{
				uiIndex = (uint8)((muiTxFirst + muiTxCount) % HCP65_UART_FIFO_SIZE);
				mauiTxFIFO[uiIndex] = uiValue;
				muiTxCount++;
			}
			mbTransmitInterrupt = false;
			StartShifting();
		}
		break;

	case HCP65_UART_IER:
		if (muiLCR & HCP65_UART_LCR_DLAB)
		{
			muiDivisor = (uint16)((muiDivisor & 0x00FF) | (uiValue << 8));
		}
		else
		{
			//Turning the transmit interrupt on with THR empty interrupts at once.
			uiOldIER = muiIER;
			muiIER = (uint8)(uiValue & 0x0F);
			if ((muiIER & HCP65_UART_IER_TRANSMIT) && !(uiOldIER & HCP65_UART_IER_TRANSMIT) && (muiTxCount == 0))
			{
				mbTransmitInterrupt = true;
			}
		}
		break;

	case HCP65_UART_IIR:
		//FCR.  Turning the FIFOs on or off empties them.
		if ((uiValue ^ muiFCR) & HCP65_UART_FCR_ENABLE)
		{
			uiValue |= HCP65_UART_FCR_RX_RESET | HCP65_UART_FCR_TX_RESET;
		}
		muiFCR = (uint8)(uiValue & (HCP65_UART_FCR_ENABLE | HCP65_UART_FCR_TRIGGER));
		if (uiValue & HCP65_UART_FCR_RX_RESET)
		{
			ClearRx();
		}
		if (uiValue & HCP65_UART_FCR_TX_RESET)
		{
			ClearTx();
			mbTransmitInterrupt = true;
		}
		StartReceiving();
		break;

	case HCP65_UART_LCR:
		muiLCR = uiValue;
		break;

	case HCP65_UART_MCR:
		muiMCR = (uint8)(uiValue & 0x3F);
		UpdateModemDeltas();
		StartShifting();
		StartReceiving();
		break;

	case HCP65_UART_SCR:
		muiSCR = uiValue;
		break;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The highest priority interrupt pending, as IIR's D0-D3.
//
//////////////////////////////////////////////////////////////////////////
uint8 CHCP65UART::Identify(void)
{
	UpdateModemDeltas();
	if ((muiIER & HCP65_UART_IER_LINE_STATUS) && muiLineErrors)
	{
		return HCP65_UART_IIR_LINE_STATUS;
	}
	if (muiIER & HCP65_UART_IER_RECEIVED)
	{
		if (muiRxCount >= TriggerLevel())
		{
			return HCP65_UART_IIR_RECEIVED;
		}
		if ((muiRxCount > 0) && IsTimedOut())
		{
			return HCP65_UART_IIR_TIMEOUT;
		}
	}
	if ((muiIER & HCP65_UART_IER_TRANSMIT) && mbTransmitInterrupt)
	{
		return HCP65_UART_IIR_TRANSMIT;
	}
	if ((muiIER & HCP65_UART_IER_MODEM_STATUS) && muiModemDeltas)
	{
		return HCP65_UART_IIR_MODEM_STATUS;
	}
	return HCP65_UART_IIR_NONE;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint8 CHCP65UART::ReadLineStatus(void)
{
	uint8	uiStatus;

	uiStatus = muiLineErrors;
	if (muiRxCount > 0)
	{
		uiStatus |= HCP65_UART_LSR_DATA_READY;
	}
	if (muiTxCount == 0)
	{
		uiStatus |= HCP65_UART_LSR_THR_EMPTY;
		if (!mbShifting)
		{
			uiStatus |= HCP65_UART_LSR_TX_EMPTY;
		}
	}
	return uiStatus;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint8 CHCP65UART::ReadModemStatus(void)
{
	UpdateModemDeltas();
	return (uint8)(muiModemDeltas | muiModemInputs);
}


//////////////////////////////////////////////////////////////////////////
//
// MSR's D4-D7.  In loopback they are MCR's outputs; otherwise the far
// end's, those the module has, and RI, which both modules tie on.
//
//////////////////////////////////////////////////////////////////////////
uint8 CHCP65UART::ModemInputs(void)
{
	uint8	uiInputs;

	uiInputs = 0;
	if (IsLoopback())
	{
		if (muiMCR & HCP65_UART_MCR_RTS) uiInputs |= HCP65_UART_MSR_CTS;
		if (muiMCR & HCP65_UART_MCR_DTR) uiInputs |= HCP65_UART_MSR_DSR;
		if (muiMCR & HCP65_UART_MCR_OUT1) uiInputs |= HCP65_UART_MSR_RI;
		if (muiMCR & HCP65_UART_MCR_OUT2) uiInputs |= HCP65_UART_MSR_DCD;
		return uiInputs;
	}

	uiInputs = HCP65_UART_MSR_RI;
	if (mbHostCTS)
	{
		uiInputs |= HCP65_UART_MSR_CTS;
	}
	if (meModule == HUM_RS232)
	{
		if (mbHostDSR) uiInputs |= HCP65_UART_MSR_DSR;
		if (mbHostDCD) uiInputs |= HCP65_UART_MSR_DCD;
	}
	return uiInputs;
}


//////////////////////////////////////////////////////////////////////////
//
// MSR's deltas for the inputs' changes since they were last looked at:
// CTS, DSR and DCD changing, RI going off.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65UART::UpdateModemDeltas(void)
{
	uint8	uiInputs;
	uint8	uiChanged;

	uiInputs = ModemInputs();
	uiChanged = (uint8)(uiInputs ^ muiModemInputs);
	if (uiChanged & HCP65_UART_MSR_CTS) muiModemDeltas |= 0x01;
	if (uiChanged & HCP65_UART_MSR_DSR) muiModemDeltas |= 0x02;
	if ((uiChanged & HCP65_UART_MSR_RI) && !(uiInputs & HCP65_UART_MSR_RI)) muiModemDeltas |= 0x04;
	if (uiChanged & HCP65_UART_MSR_DCD) muiModemDeltas |= 0x08;
	muiModemInputs = uiInputs;
}


//////////////////////////////////////////////////////////////////////////
//
// 16 with the FIFOs on, otherwise 1 (the holding registers).
//
//////////////////////////////////////////////////////////////////////////
uint8 CHCP65UART::FIFODepth(void)
{
	return (muiFCR & HCP65_UART_FCR_ENABLE) ? HCP65_UART_FIFO_SIZE : 1;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint8 CHCP65UART::TriggerLevel(void)
{
	static const uint8	auiLevels[4] = { 1, 4, 8, 14 };

	if (!(muiFCR & HCP65_UART_FCR_ENABLE))
	{
		return 1;
	}
	return auiLevels[muiFCR >> 6];
}


bool CHCP65UART::IsLoopback(void) { return (muiMCR & HCP65_UART_MCR_LOOP) != 0; }
uint8 CHCP65UART::DataMask(void) { return (uint8)(0xFF >> (3 - (muiLCR & HCP65_UART_LCR_DATA))); }


//////////////////////////////////////////////////////////////////////////
//
// The receive timeout: four characters' time with no character arriving
// and none read, and bytes in the FIFO (the FIFOs on).
//
//////////////////////////////////////////////////////////////////////////
uint64 CHCP65UART::TimeoutTick(void)
{
	return muiRxActivity + 4 * CharacterTicks();
}


bool CHCP65UART::IsTimedOut(void)
{
	return (muiFCR & HCP65_UART_FCR_ENABLE) && (muiTick >= TimeoutTick());
}


//////////////////////////////////////////////////////////////////////////
//
// Oscillator ticks per character: 16 per bit, times the divisor; a start
// bit, the data bits, parity and 1, 1.5 or 2 stop bits.
//
//////////////////////////////////////////////////////////////////////////
uint64 CHCP65UART::CharacterTicks(void)
{
	uint64	uiDivisor;
	uint64	uiDataBits;
	uint64	uiHalfBits;

	uiDivisor = (muiDivisor == 0) ? 0x10000 : muiDivisor;
	uiDataBits = 5 + (muiLCR & HCP65_UART_LCR_DATA);
	uiHalfBits = 2 * (1 + uiDataBits + ((muiLCR & HCP65_UART_LCR_PARITY) ? 1 : 0));
	if (muiLCR & HCP65_UART_LCR_STOP)
	{
		uiHalfBits += (uiDataBits == 5) ? 3 : 4;
	}
	else
	{
		uiHalfBits += 2;
	}
	return 8 * uiDivisor * uiHalfBits;
}


//////////////////////////////////////////////////////////////////////////
//
// The next byte from the FIFO into the shift register, if it is free and
// auto-CTS lets it go.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65UART::StartShifting(void)
{
	if (mbShifting || (muiTxCount == 0))
	{
		return;
	}
	if ((muiMCR & HCP65_UART_MCR_AUTO_FLOW) && !(ModemInputs() & HCP65_UART_MSR_CTS))
	{
		return;
	}
	muiShiftByte = mauiTxFIFO[muiTxFirst];
	muiTxFirst = (uint8)((muiTxFirst + 1) % HCP65_UART_FIFO_SIZE);
	muiTxCount--;
	if (muiTxCount == 0)
	{
		mbTransmitInterrupt = true;
	}
	mbShifting = true;
	muiShiftDone = muiTick + CharacterTicks();
}


//////////////////////////////////////////////////////////////////////////
//
// The character has gone, its data bits: to the far end, or in loopback
// to the receiver.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65UART::FinishShifting(void)
{
	mbShifting = false;
	if (IsLoopback())
	{
		AddReceived((uint8)(muiShiftByte & DataMask()));
	}
	else
	{
		macHostReceived.Add((uint8)(muiShiftByte & DataMask()));
	}
	StartShifting();
}


//////////////////////////////////////////////////////////////////////////
//
// The far end starts its next character, if it has one and RTS is on.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65UART::StartReceiving(void)
{
	if (mbReceiving || (muiHostSendIndex >= macHostSend.NumElements()) || !IsRTSOn())
	{
		return;
	}
	muiReceiveByte = macHostSend.GetValue(muiHostSendIndex);
	muiHostSendIndex++;
	if (muiHostSendIndex == macHostSend.NumElements())
	{
		macHostSend.Kill();
		macHostSend.Init();
		muiHostSendIndex = 0;
	}
	mbReceiving = true;
	muiReceiveDone = muiTick + CharacterTicks();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65UART::FinishReceiving(void)
{
	mbReceiving = false;
	if (!IsLoopback())
	{
		AddReceived((uint8)(muiReceiveByte & DataMask()));
	}
	StartReceiving();
}


//////////////////////////////////////////////////////////////////////////
//
// A character into the receive FIFO, or lost (an overrun) if it is full.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65UART::AddReceived(uint8 uiByte)
{
	uint8	uiIndex;

	if (muiRxCount < FIFODepth())
	{
		uiIndex = (uint8)((muiRxFirst + muiRxCount) % HCP65_UART_FIFO_SIZE);
		mauiRxFIFO[uiIndex] = uiByte;
		muiRxCount++;
	}
	else
	{
		muiLineErrors |= HCP65_UART_LSR_OVERRUN;
		muiOverruns++;
	}
	muiRxActivity = muiTick;
}


void CHCP65UART::ClearRx(void) { muiRxFirst = 0; muiRxCount = 0; }
void CHCP65UART::ClearTx(void) { muiTxFirst = 0; muiTxCount = 0; }


//////////////////////////////////////////////////////////////////////////
//
// Characters finish in the order they would, up to uiTick.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65UART::CatchUp(uint64 uiTick)
{
	uint64	uiNext;

	for (;;)
	{
		uiNext = HCP65_NEVER;
		if (mbShifting)
		{
			uiNext = muiShiftDone;
		}
		if (mbReceiving && (muiReceiveDone < uiNext))
		{
			uiNext = muiReceiveDone;
		}
		if (uiNext > uiTick)
		{
			break;
		}

		muiTick = uiNext;
		if (mbShifting && (muiShiftDone == uiNext))
		{
			FinishShifting();
		}
		else
		{
			FinishReceiving();
		}
	}
	if (uiTick > muiTick)
	{
		muiTick = uiTick;
	}
	StartShifting();
	StartReceiving();
}


//////////////////////////////////////////////////////////////////////////
//
// The tick of the next character finishing or the receive timeout, while
// the UART has interrupts on.
//
//////////////////////////////////////////////////////////////////////////
uint64 CHCP65UART::NextEvent(void)
{
	uint64	uiNext;
	uint64	uiTimeout;

	if (muiIER == 0)
	{
		return HCP65_NEVER;
	}
	uiNext = HCP65_NEVER;
	if (mbShifting)
	{
		uiNext = muiShiftDone;
	}
	if (mbReceiving && (muiReceiveDone < uiNext))
	{
		uiNext = muiReceiveDone;
	}
	if ((muiIER & HCP65_UART_IER_RECEIVED) && (muiFCR & HCP65_UART_FCR_ENABLE) && (muiRxCount > 0))
	{
		uiTimeout = TimeoutTick();
		if ((uiTimeout > muiTick) && (uiTimeout < uiNext))
		{
			uiNext = uiTimeout;
		}
	}
	return uiNext;
}


//////////////////////////////////////////////////////////////////////////
//
// Its INTRPT pin.
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65UART::IsInterrupting(void)
{
	return Identify() != HCP65_UART_IIR_NONE;
}


//////////////////////////////////////////////////////////////////////////
//
// The far end will send uiLength more bytes.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65UART::HostSend(const uint8* puiData, size uiLength)
{
	macHostSend.Add((uint8*)puiData, uiLength);
	StartReceiving();
}


//////////////////////////////////////////////////////////////////////////
//
// Takes up to uiMaxLength of the bytes the UART has sent.
//
//////////////////////////////////////////////////////////////////////////
size CHCP65UART::HostReceive(uint8* puiDest, size uiMaxLength)
{
	size	uiCount;

	uiCount = macHostReceived.NumElements();
	uiCount = (uiCount < uiMaxLength) ? uiCount : uiMaxLength;
	if (uiCount > 0)
	{
		memcpy(puiDest, macHostReceived.GetData(), uiCount);
		macHostReceived.RemoveRange(0, uiCount);
	}
	return uiCount;
}


//////////////////////////////////////////////////////////////////////////
//
// The far end's outputs.  The UART's MSR sees the change; auto-CTS may let
// the transmitter go.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65UART::HostSetCTS(bool bOn)
{
	mbHostCTS = bOn;
	UpdateModemDeltas();
	StartShifting();
}


void CHCP65UART::HostSetDSR(bool bOn)
{
	mbHostDSR = bOn;
	UpdateModemDeltas();
}


void CHCP65UART::HostSetDCD(bool bOn)
{
	mbHostDCD = bOn;
	UpdateModemDeltas();
}


//////////////////////////////////////////////////////////////////////////
//
// The RTS the far end sees: MCR's, off while auto-RTS holds it off (the
// receive FIFO at its trigger level), and off in loopback.
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65UART::IsRTSOn(void)
{
	if (IsLoopback() || !(muiMCR & HCP65_UART_MCR_RTS))
	{
		return false;
	}
	return !((muiMCR & HCP65_UART_MCR_AUTO_FLOW) && (muiRxCount >= TriggerLevel()));
}


bool CHCP65UART::IsDTROn(void) { return !IsLoopback() && ((muiMCR & HCP65_UART_MCR_DTR) != 0); }
size CHCP65UART::NumHostToSend(void) { return macHostSend.NumElements() - muiHostSendIndex; }
size CHCP65UART::NumHostReceived(void) { return macHostReceived.NumElements(); }
uint8 CHCP65UART::GetLineControl(void) { return muiLCR; }
uint64 CHCP65UART::NumOverruns(void) { return muiOverruns; }


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint32 CHCP65UART::GetBaudRate(void)
{
	return HCP65_UART_CLOCK_RATE / (16 * ((muiDivisor == 0) ? 0x10000 : (uint32)muiDivisor));
}


//////////////////////////////////////////////////////////////////////////
//
// Both UARTs' registers are listed, each read then write, as DeviceROM/
// Boards/UARTBoard.xml has them.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65UARTBoard::Init(void)
{
	uint16	i;

	CHCP65Device::Init(HCP65_SMALL_SLOT_SIZE);

	mcROM.Init(HCP65_UART_NAME, HCP65_DEVICE_MANUFACTURER, 0, DEVICE_BUS_32KB, 0, HCP65_UART_DRIVER);
	for (i = 0; i < 2 * HCP65_UART_NUM_REGISTERS; i++)
	{
		mcROM.AddRegister((uint16)(HCP65_UART_REGISTERS + i), DEVICE_REGISTER_READ);
		mcROM.AddRegister((uint16)(HCP65_UART_REGISTERS + i), DEVICE_REGISTER_WRITE);
	}

	mcUART1.Init(HUM_TTL);
	mcUART2.Init(HUM_RS232);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65UARTBoard::Kill(void)
{
	mcUART2.Kill();
	mcUART1.Kill();
	CHCP65Device::Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Below $400 the device ROM; $400 to $7FF the UARTs.
//
//////////////////////////////////////////////////////////////////////////
uint8 CHCP65UARTBoard::Read(uint32 uiOffset)
{
	if (uiOffset < HCP65_UART_REGISTERS)
	{
		return mcROM.Read(uiOffset);
	}
	if (uiOffset < HCP65_UART_REGISTERS_END)
	{
		return GetUART(uiOffset)->Read((uint8)(uiOffset & HCP65_UART_REGISTER));
	}
	return 0xFF;
}


//////////////////////////////////////////////////////////////////////////
//
// The device ROM is flash; writes to it are ignored, as are writes while
// the UARTs are held in reset.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65UARTBoard::Write(uint32 uiOffset, uint8 uiValue)
{
	if (mbHeldInReset || (uiOffset < HCP65_UART_REGISTERS) || (uiOffset >= HCP65_UART_REGISTERS_END))
	{
		return;
	}
	GetUART(uiOffset)->Write((uint8)(uiOffset & HCP65_UART_REGISTER), uiValue);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65UARTBoard::Reset(void)
{
	mcUART1.Reset();
	mcUART2.Reset();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65UARTBoard::CatchUp(uint64 uiCycle)
{
	uint64	uiTick;

	if (!mpcHCP65)
	{
		return;
	}
	uiTick = UARTTick(uiCycle);
	mcUART1.CatchUp(uiTick);
	mcUART2.CatchUp(uiTick);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint64 CHCP65UARTBoard::NextEvent(void)
{
	uint64	uiNext;
	uint64	uiEvent;

	if (!mpcHCP65 || mbHeldInReset)
	{
		return HCP65_NEVER;
	}
	uiNext = mcUART1.NextEvent();
	uiEvent = mcUART2.NextEvent();
	if (uiEvent < uiNext)
	{
		uiNext = uiEvent;
	}
	return (uiNext == HCP65_NEVER) ? HCP65_NEVER : CycleOfUARTTick(uiNext);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65UARTBoard::IsInterrupting(void)
{
	return !mbHeldInReset && (mcUART1.IsInterrupting() || mcUART2.IsInterrupting());
}


//////////////////////////////////////////////////////////////////////////
//
// The oscillators' tick at CPU cycle uiCycle, and the first cycle by which
// a tick has happened.
//
//////////////////////////////////////////////////////////////////////////
uint64 CHCP65UARTBoard::UARTTick(uint64 uiCycle)
{
	return (uiCycle * HCP65_UART_CLOCK_RATE) / mpcHCP65->GetClockRate();
}


uint64 CHCP65UARTBoard::CycleOfUARTTick(uint64 uiTick)
{
	return (uiTick * mpcHCP65->GetClockRate() + HCP65_UART_CLOCK_RATE - 1) / HCP65_UART_CLOCK_RATE;
}


CHCP65UART* CHCP65UARTBoard::GetUART(uint32 uiOffset) { return (uiOffset & HCP65_UART_UART1) ? &mcUART1 : &mcUART2; }
CHCP65UART* CHCP65UARTBoard::GetTTL(void) { return &mcUART1; }
CHCP65UART* CHCP65UARTBoard::GetRS232(void) { return &mcUART2; }
