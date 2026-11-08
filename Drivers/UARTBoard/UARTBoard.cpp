#include "KernalInterrupts.h"
#include "KernalProcess.h"
#include "UARTBoard.h"


#define UART_DEFAULT_IER	(UART_IER_RECEIVED | UART_IER_LINE_STATUS)
#define UART_DEFAULT_FCR	(UART_FCR_ENABLE | UART_FCR_TRIGGER_8)

//UART_WAIT_*'s kinds, its high word; the low word is the count.  Shifted as 32 bits: UART_WAIT_RX is a 16 bit int on the
//W65C816, which can't be shifted by 16.
#define UART_WAIT_KIND_RX			((uint32)UART_WAIT_RX >> 16)
#define UART_WAIT_KIND_TX_SPACE		((uint32)UART_WAIT_TX_SPACE >> 16)
#define UART_WAIT_KIND_TX_EMPTY		((uint32)UART_WAIT_TX_EMPTY >> 16)


//////////////////////////////////////////////////////////////////////////
//
// The UART at puiRegisters, whose module has the inputs uiLines.  Quiet
// until it is started.
//
//////////////////////////////////////////////////////////////////////////
void CKernalUARTPort::Init(volatile uint8* puiRegisters, uint8 uiLines)
{
	mpuiRegisters = puiRegisters;
	muiLines = uiLines;
	Reset();
}


//////////////////////////////////////////////////////////////////////////
//
// Interrupts off, the outputs off, 115200 8N1, the FIFOs and rings
// emptied and nothing pending.
//
//////////////////////////////////////////////////////////////////////////
void CKernalUARTPort::Reset(void)
{
	SetIER(0);
	SetMCR(0);
	muiLCR = UART_FORMAT_8N1;
	SetDivisor(1);
	mpuiRegisters[UART_FCR] = UART_DEFAULT_FCR | UART_FCR_RX_RESET | UART_FCR_TX_RESET;
	muiRxHead = 0;
	muiRxTail = 0;
	muiTxHead = 0;
	muiTxTail = 0;
	ReadLineStatus();
	Discard(UART_RBR);
	Discard(UART_MSR);
	Discard(UART_IIR);
	muiErrors = 0;
}


//////////////////////////////////////////////////////////////////////////
//
// Ready for a process, as HCP65UARTBoard.h says a port starts.
//
//////////////////////////////////////////////////////////////////////////
void CKernalUARTPort::Start(void)
{
	Reset();
	SetMCR(UART_MCR_DTR | UART_MCR_RTS);
	SetIER(UART_DEFAULT_IER);
}


//////////////////////////////////////////////////////////////////////////
//
// Until the UART has nothing pending.  Trusted: only its registers and the
// rings.
//
//////////////////////////////////////////////////////////////////////////
void CKernalUARTPort::Interrupt(void)
{
	uint8	uiIdentity;

	for (;;)
	{
		uiIdentity = mpuiRegisters[UART_IIR];
		if (uiIdentity & UART_IIR_NONE)
		{
			return;
		}
		uiIdentity &= UART_IIR_ID;
		if (uiIdentity == UART_IIR_LINE_STATUS)
		{
			ReadLineStatus();
		}
		else if ((uiIdentity == UART_IIR_RECEIVED) || (uiIdentity == UART_IIR_TIMEOUT))
		{
			Receive();
		}
		else if (uiIdentity == UART_IIR_TRANSMIT)
		{
			Transmit();
		}
		else
		{
			Discard(UART_MSR);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The line status, its errors kept (reading it clears them): every read
// of LSR comes here, or an error could be lost.
//
//////////////////////////////////////////////////////////////////////////
uint8 CKernalUARTPort::ReadLineStatus(void)
{
	uint8	uiStatus;

	uiStatus = mpuiRegisters[UART_LSR];
	muiErrors = (uint8)(muiErrors | (uiStatus & UART_LSR_ERRORS));
	return uiStatus;
}


//////////////////////////////////////////////////////////////////////////
//
// The FIFO's bytes into the ring.  When the ring fills the received
// interrupt is turned off, so the FIFO fills (and with RTS/CTS the UART
// stops the sender); Read turns it on again.
//
//////////////////////////////////////////////////////////////////////////
void CKernalUARTPort::Receive(void)
{
	uint8	uiNext;

	while (ReadLineStatus() & UART_LSR_DATA_READY)
	{
		uiNext = (uint8)(muiRxHead + 1);
		if (uiNext == muiRxTail)
		{
			SetIER((uint8)(muiIER & ~UART_IER_RECEIVED));
			return;
		}
		mauiRx[muiRxHead] = mpuiRegisters[UART_RBR];
		muiRxHead = uiNext;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The FIFO is empty: up to 16 bytes from the ring into it.  With the ring
// empty the transmit interrupt is turned off; Queue turns it on again.
//
//////////////////////////////////////////////////////////////////////////
void CKernalUARTPort::Transmit(void)
{
	uint8	uiSpace;

	for (uiSpace = UART_FIFO_SIZE; (uiSpace > 0) && (muiTxTail != muiTxHead); uiSpace--)
	{
		mpuiRegisters[UART_THR] = mauiTx[muiTxTail];
		muiTxTail = (uint8)(muiTxTail + 1);
	}
	if (muiTxTail == muiTxHead)
	{
		SetIER((uint8)(muiIER & ~UART_IER_TRANSMIT));
	}
}


//////////////////////////////////////////////////////////////////////////
//
// What has arrived, up to uiLength bytes, to the running process's memory
// at uiUserAddress, without waiting.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalUARTPort::Read(uint32 uiUserAddress, uint32 uiLength, uint32* puiDone)
{
	uint16	uiPart;
	uint16	uiToEnd;

	while (uiLength > 0)
	{
		uiPart = NumReceived();
		if (uiPart == 0)
		{
			break;
		}
		uiToEnd = (uint16)(UART_RING_SIZE - muiRxTail);
		uiPart = (uiPart < uiToEnd) ? uiPart : uiToEnd;
		uiPart = (uiPart < uiLength) ? uiPart : (uint16)uiLength;
		if (!KernalCopyToUser(uiUserAddress, &mauiRx[muiRxTail], uiPart))
		{
			return KERNAL_DRIVER_BAD_ARGUMENT;
		}
		muiRxTail = (uint8)(muiRxTail + uiPart);
		uiUserAddress += uiPart;
		uiLength -= uiPart;
		*puiDone += uiPart;
	}

	//There is room again: the bytes the FIFO held on to come in.
	if (!(muiIER & UART_IER_RECEIVED) && (NumReceived() < UART_BOARD_BUFFER_SIZE))
	{
		SetIER((uint8)(muiIER | UART_IER_RECEIVED));
	}
	return KERNAL_DRIVER_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////
//
// What fits in the ring of uiLength bytes from the running process's
// memory at uiUserAddress, queued to send without waiting: *puiQueued
// is how many.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalUARTPort::Queue(uint32 uiUserAddress, uint32 uiLength, uint32* puiQueued)
{
	uint16	uiPart;
	uint16	uiToEnd;

	*puiQueued = 0;
	while (uiLength > 0)
	{
		uiPart = (uint16)(UART_BOARD_BUFFER_SIZE - NumQueued());
		if (uiPart == 0)
		{
			break;
		}
		uiToEnd = (uint16)(UART_RING_SIZE - muiTxHead);
		uiPart = (uiPart < uiToEnd) ? uiPart : uiToEnd;
		uiPart = (uiPart < uiLength) ? uiPart : (uint16)uiLength;
		if (!KernalCopyFromUser(&mauiTx[muiTxHead], uiUserAddress, uiPart))
		{
			return KERNAL_DRIVER_BAD_ARGUMENT;
		}
		muiTxHead = (uint8)(muiTxHead + uiPart);
		uiUserAddress += uiPart;
		uiLength -= uiPart;
		*puiQueued += uiPart;

		//The UART interrupts at once if its FIFO is empty.
		if (!(muiIER & UART_IER_TRANSMIT))
		{
			SetIER((uint8)(muiIER | UART_IER_TRANSMIT));
		}
	}
	return KERNAL_DRIVER_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////
//
// HCP65UARTBoard.h's UART_BOARD_* codes.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalUARTPort::Control(uint16 uiCode, uint32 uiValue, uint32* puiResult)
{
	switch (uiCode)
	{
	case UART_BOARD_SET_DIVISOR:
		if ((uiValue == 0) || (uiValue > 0xFFFF))
		{
			return KERNAL_DRIVER_BAD_ARGUMENT;
		}
		SetDivisor((uint16)uiValue);
		return KERNAL_DRIVER_SUCCESS;

	case UART_BOARD_SET_FORMAT:
		if (uiValue & ~(uint32)UART_LCR_FORMAT)
		{
			return KERNAL_DRIVER_BAD_ARGUMENT;
		}
		SetLCR((uint8)((muiLCR & UART_LCR_BREAK) | uiValue));
		return KERNAL_DRIVER_SUCCESS;

	case UART_BOARD_SET_FLOW:
		if (uiValue == UART_FLOW_NONE)
		{
			SetMCR((uint8)(muiMCR & ~UART_MCR_AUTO_FLOW));
		}
		else if (uiValue == UART_FLOW_RTS_CTS)
		{
			SetMCR((uint8)(muiMCR | UART_MCR_AUTO_FLOW | UART_MCR_RTS));
		}
		else
		{
			return KERNAL_DRIVER_BAD_ARGUMENT;
		}
		return KERNAL_DRIVER_SUCCESS;

	case UART_BOARD_SET_LINES:
		if (uiValue & ~(uint32)(UART_LINE_DTR | UART_LINE_RTS))
		{
			return KERNAL_DRIVER_BAD_ARGUMENT;
		}
		SetMCR((uint8)((muiMCR & ~(UART_MCR_DTR | UART_MCR_RTS)) | uiValue));
		return KERNAL_DRIVER_SUCCESS;

	case UART_BOARD_GET_LINES:
		*puiResult = mpuiRegisters[UART_MSR] & muiLines;
		return KERNAL_DRIVER_SUCCESS;

	case UART_BOARD_GET_ERRORS:
		ReadLineStatus();
		*puiResult = muiErrors;
		muiErrors = 0;
		return KERNAL_DRIVER_SUCCESS;

	case UART_BOARD_SET_BREAK:
		if (uiValue > 1)
		{
			return KERNAL_DRIVER_BAD_ARGUMENT;
		}
		SetLCR((uint8)(uiValue ? (muiLCR | UART_LCR_BREAK) : (muiLCR & ~UART_LCR_BREAK)));
		return KERNAL_DRIVER_SUCCESS;

	case UART_BOARD_FLUSH:
		if ((uiValue == 0) || (uiValue & ~(uint32)(UART_FLUSH_RX | UART_FLUSH_TX)))
		{
			return KERNAL_DRIVER_BAD_ARGUMENT;
		}
		Flush((uint8)uiValue);
		return KERNAL_DRIVER_SUCCESS;

	case UART_BOARD_GET_RX_COUNT:
		*puiResult = NumReceived();
		return KERNAL_DRIVER_SUCCESS;

	case UART_BOARD_GET_TX_COUNT:
		*puiResult = (uint32)NumQueued() + (IsSent() ? 0 : 1);
		return KERNAL_DRIVER_SUCCESS;

	case UART_BOARD_SET_LOOPBACK:
		if (uiValue > 1)
		{
			return KERNAL_DRIVER_BAD_ARGUMENT;
		}
		SetMCR((uint8)(uiValue ? (muiMCR | UART_MCR_LOOP) : (muiMCR & ~UART_MCR_LOOP)));
		return KERNAL_DRIVER_SUCCESS;
	}
	return KERNAL_DRIVER_NOT_SUPPORTED;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether uiValue is a UART_WAIT_* with a count it takes.
//
//////////////////////////////////////////////////////////////////////////
bool CKernalUARTPort::IsWaitValid(uint32 uiValue)
{
	uint16	uiCount;

	uiCount = (uint16)uiValue;
	switch ((uint16)(uiValue >> 16))
	{
	case UART_WAIT_KIND_RX:
	case UART_WAIT_KIND_TX_SPACE:
		return (uiCount > 0) && (uiCount <= UART_BOARD_BUFFER_SIZE);
	case UART_WAIT_KIND_TX_EMPTY:
		return uiCount == 0;
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether what uiValue waits for (UART_WAIT_*, IsWaitValid) has happened.
//
//////////////////////////////////////////////////////////////////////////
bool CKernalUARTPort::IsWaitOver(uint32 uiValue)
{
	uint16	uiCount;

	uiCount = (uint16)uiValue;
	switch ((uint16)(uiValue >> 16))
	{
	case UART_WAIT_KIND_RX:
		return NumReceived() >= uiCount;
	case UART_WAIT_KIND_TX_SPACE:
		return (UART_BOARD_BUFFER_SIZE - NumQueued()) >= uiCount;
	}
	return (NumQueued() == 0) && IsSent();
}


//////////////////////////////////////////////////////////////////////////
//
// Drops what is buffered (UART_FLUSH_*), the UART's FIFOs included.
//
//////////////////////////////////////////////////////////////////////////
void CKernalUARTPort::Flush(uint8 uiWhich)
{
	uint8	uiFCR;

	uiFCR = UART_DEFAULT_FCR;
	if (uiWhich & UART_FLUSH_RX)
	{
		muiRxTail = muiRxHead;
		uiFCR |= UART_FCR_RX_RESET;
	}
	if (uiWhich & UART_FLUSH_TX)
	{
		muiTxTail = muiTxHead;
		uiFCR |= UART_FCR_TX_RESET;
	}
	mpuiRegisters[UART_FCR] = uiFCR;
	if (!(muiIER & UART_IER_RECEIVED))
	{
		SetIER((uint8)(muiIER | UART_IER_RECEIVED));
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The baud rate is 115200 / uiDivisor.
//
//////////////////////////////////////////////////////////////////////////
void CKernalUARTPort::SetDivisor(uint16 uiDivisor)
{
	mpuiRegisters[UART_LCR] = (uint8)(muiLCR | UART_LCR_DLAB);
	mpuiRegisters[UART_DLL] = (uint8)uiDivisor;
	mpuiRegisters[UART_DLM] = (uint8)(uiDivisor >> 8);
	mpuiRegisters[UART_LCR] = muiLCR;
}


//////////////////////////////////////////////////////////////////////////
//
// Reads the register for what reading it does (clears what it reports);
// the value isn't wanted.
//
//////////////////////////////////////////////////////////////////////////
void CKernalUARTPort::Discard(uint8 uiRegister)
{
	mpuiRegisters[uiRegister];
}


void CKernalUARTPort::SetIER(uint8 uiIER) { muiIER = uiIER; mpuiRegisters[UART_IER] = uiIER; }
void CKernalUARTPort::SetLCR(uint8 uiLCR) { muiLCR = uiLCR; mpuiRegisters[UART_LCR] = uiLCR; }
void CKernalUARTPort::SetMCR(uint8 uiMCR) { muiMCR = uiMCR; mpuiRegisters[UART_MCR] = uiMCR; }
uint8 CKernalUARTPort::NumReceived(void) { return (uint8)(muiRxHead - muiRxTail); }
uint8 CKernalUARTPort::NumQueued(void) { return (uint8)(muiTxHead - muiTxTail); }
bool CKernalUARTPort::IsSent(void) { return (ReadLineStatus() & UART_LSR_TX_EMPTY) != 0; }


//////////////////////////////////////////////////////////////////////////
//
// Both UARTs quiet until opened; the interrupt is trusted.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalUARTBoard::Init(SKernalDevice* psDevice, uint16 uiSlot)
{
	uint16	uiResult;

	uiResult = CKernalDriver::Init(psDevice, uiSlot);
	mbSeparateUnits = true;
	mcTTL.Init(&mpuiRegisters[UART_TTL_REGISTERS], UART_TTL_LINES);
	mcRS232.Init(&mpuiRegisters[UART_RS232_REGISTERS], UART_RS232_LINES);
	SetInterrupt(NULL, true);
	return uiResult;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CKernalUARTBoard::Kill(void)
{
	mcRS232.Reset();
	mcTTL.Reset();
	CKernalDriver::Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalUARTBoard::NumUnits(void)
{
	return 2;
}


//////////////////////////////////////////////////////////////////////////
//
// The two UARTs' interrupts are and'ed onto the board's.
//
//////////////////////////////////////////////////////////////////////////
void CKernalUARTBoard::Interrupt(void)
{
	mcTTL.Interrupt();
	mcRS232.Interrupt();
}


//////////////////////////////////////////////////////////////////////////
//
// A process has opened the port.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalUARTBoard::OpenUnit(uint16 uiUnit)
{
	GetPort(uiUnit)->Start();
	return KERNAL_DRIVER_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////
//
// Waits up to UART_BOARD_CLOSE_TICKS for what is queued to be sent, then
// quietens the port.
//
//////////////////////////////////////////////////////////////////////////
void CKernalUARTBoard::CloseUnit(uint16 uiUnit)
{
	WaitFor(uiUnit, UART_WAIT_TX_EMPTY, UART_BOARD_CLOSE_TICKS);
	GetPort(uiUnit)->Reset();
}


//////////////////////////////////////////////////////////////////////////
//
// The offset is ignored: a port is a stream.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalUARTBoard::Read(uint16 uiUnit, uint32 uiOffset, uint32 uiUserAddress, uint32 uiLength, uint32* puiDone)
{
	return GetPort(uiUnit)->Read(uiUserAddress, uiLength, puiDone);
}


//////////////////////////////////////////////////////////////////////////
//
// uiLength bytes queued to send, sleeping while the ring is full:
// KERNAL_DRIVER_TIMEOUT after UART_BOARD_WRITE_TICKS without room.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalUARTBoard::Write(uint16 uiUnit, uint32 uiOffset, uint32 uiUserAddress, uint32 uiLength, uint32* puiDone)
{
	CKernalUARTPort*	pcPort;
	uint32				uiQueued;
	uint16				uiResult;

	pcPort = GetPort(uiUnit);
	for (;;)
	{
		uiResult = pcPort->Queue(uiUserAddress, uiLength, &uiQueued);
		*puiDone += uiQueued;
		if ((uiResult != KERNAL_DRIVER_SUCCESS) || (uiQueued == uiLength))
		{
			return uiResult;
		}
		uiUserAddress += uiQueued;
		uiLength -= uiQueued;
		if (WaitFor(uiUnit, UART_WAIT_TX_SPACE | 1, UART_BOARD_WRITE_TICKS) != KERNAL_DRIVER_SUCCESS)
		{
			return KERNAL_DRIVER_TIMEOUT;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalUARTBoard::Control(uint16 uiUnit, uint16 uiCode, uint32 uiValue, uint32* puiResult)
{
	return GetPort(uiUnit)->Control(uiCode, uiValue, puiResult);
}


//////////////////////////////////////////////////////////////////////////
//
// Sleeps up to uiTicks for what uiValue says (UART_WAIT_*).
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalUARTBoard::Wait(uint16 uiUnit, uint32 uiValue, uint16 uiTicks)
{
	if (!GetPort(uiUnit)->IsWaitValid(uiValue))
	{
		return KERNAL_DRIVER_BAD_ARGUMENT;
	}
	return WaitFor(uiUnit, uiValue, uiTicks);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CKernalUARTBoard::IsWaitOver(uint16 uiUnit, uint32 uiValue)
{
	return GetPort(uiUnit)->IsWaitOver(uiValue);
}


//////////////////////////////////////////////////////////////////////////
//
// The kernal only gives the driver units below NumUnits.
//
//////////////////////////////////////////////////////////////////////////
CKernalUARTPort* CKernalUARTBoard::GetPort(uint16 uiUnit)
{
	return (uiUnit == UART_BOARD_TTL) ? &mcTTL : &mcRS232;
}


//The drivers of the UART boards, in slot order.
static CKernalUARTBoard	gacUARTBoards[UART_BOARD_MAX_BOARDS];


//////////////////////////////////////////////////////////////////////////
//
// The kernal's loader calls it, once the driver is in place and its
// globals are constructed, for each board it drives: the driver of the
// uiIndex'th, NULL if there are too many.
//
//////////////////////////////////////////////////////////////////////////
extern "C" CKernalDriver* KernalCreateDriver(uint16 uiIndex)
{
	return (uiIndex < UART_BOARD_MAX_BOARDS) ? &gacUARTBoards[uiIndex] : NULL;
}
