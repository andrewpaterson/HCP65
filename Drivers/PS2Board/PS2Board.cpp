#include "KernalInterrupts.h"
#include "KernalProcess.h"
#include "PS2Board.h"


//////////////////////////////////////////////////////////////////////////
//
// The lines let go, what the FIFOs hold taken, the board enabled with its
// interrupt on.  The interrupt is trusted.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalPS2Board::Init(SKernalDevice* psDevice, uint16 uiSlot)
{
	uint16	uiResult;
	uint16	i;

	uiResult = CKernalDriver::Init(psDevice, uiSlot);
	mbSeparateUnits = true;
	muiDirect = 0;
	mpuiRegisters[PS2_DIRECT] = 0;
	for (i = 0; i < PS2_NUM_PORTS; i++)
	{
		Flush(i);
		mpuiRegisters[PS2_FIFO + i] = 0;
		Take(i, false);
		Flush(i);
	}
	SetInterrupt(&mpuiRegisters[PS2_FLAGS], true);
	mpuiRegisters[PS2_FLAGS] = PS2_FLAG_ENABLE | PS2_FLAG_INTERRUPT;
	return uiResult;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CKernalPS2Board::Kill(void)
{
	mpuiRegisters[PS2_FLAGS] = 0;
	mpuiRegisters[PS2_DIRECT] = 0;
	CKernalDriver::Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalPS2Board::NumUnits(void)
{
	return PS2_NUM_PORTS;
}


//////////////////////////////////////////////////////////////////////////
//
// A port with a packet waiting has its waiting cleared, which quietens
// the board, then what its FIFO holds taken.  Cleared first, a packet
// that ends meanwhile interrupts again.  Trusted: only the board and the
// rings.
//
//////////////////////////////////////////////////////////////////////////
void CKernalPS2Board::Interrupt(void)
{
	uint8	uiState;
	uint16	i;

	uiState = mpuiRegisters[PS2_STATE];
	for (i = 0; i < PS2_NUM_PORTS; i++)
	{
		if (!(uiState & (uint8)(PS2_STATE_NO_PACKET << (i * PS2_STATE_PORT_SHIFT))))
		{
			mpuiRegisters[PS2_FIFO + i] = 0;
			Take(i, false);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A process has opened the port: what was buffered is dropped.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalPS2Board::OpenUnit(uint16 uiUnit)
{
	Flush(uiUnit);
	masPorts[uiUnit].uiOverruns = 0;
	return KERNAL_DRIVER_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////
//
// What has arrived, up to uiLength bytes, to the running process's memory
// at uiUserAddress, without waiting.  The offset is ignored.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalPS2Board::Read(uint16 uiUnit, uint32 uiOffset, uint32 uiUserAddress, uint32 uiLength, uint32* puiDone)
{
	SKernalPS2Port*	psPort;
	uint16			uiPart;
	uint16			uiToEnd;

	psPort = &masPorts[uiUnit];
	while (uiLength > 0)
	{
		uiPart = NumReceived(uiUnit);
		if (uiPart == 0)
		{
			break;
		}
		uiToEnd = (uint16)(PS2_RING_SIZE - psPort->uiTail);
		uiPart = (uiPart < uiToEnd) ? uiPart : uiToEnd;
		uiPart = (uiPart < uiLength) ? uiPart : (uint16)uiLength;
		if (!KernalCopyToUser(uiUserAddress, &psPort->auiRing[psPort->uiTail], uiPart))
		{
			return KERNAL_DRIVER_BAD_ARGUMENT;
		}
		psPort->uiTail = (uint8)(psPort->uiTail + uiPart);
		uiUserAddress += uiPart;
		uiLength -= uiPart;
		*puiDone += uiPart;
	}
	return KERNAL_DRIVER_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////
//
// The running process's bytes sent to the device, one at a time: it fails
// at the first the device doesn't take.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalPS2Board::Write(uint16 uiUnit, uint32 uiOffset, uint32 uiUserAddress, uint32 uiLength, uint32* puiDone)
{
	uint8	uiByte;
	uint32	i;

	for (i = 0; i < uiLength; i++)
	{
		if (!KernalCopyFromUser(&uiByte, uiUserAddress + i, 1))
		{
			return KERNAL_DRIVER_BAD_ARGUMENT;
		}
		if (!Send(uiUnit, uiByte))
		{
			return KERNAL_DRIVER_FAILED;
		}
		*puiDone += 1;
	}
	return KERNAL_DRIVER_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////
//
// HCP65PS2Board.h's PS2_BOARD_* codes.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalPS2Board::Control(uint16 uiUnit, uint16 uiCode, uint32 uiValue, uint32* puiResult)
{
	switch (uiCode)
	{
	case PS2_BOARD_GET_RX_COUNT:
		*puiResult = NumReceived(uiUnit);
		return KERNAL_DRIVER_SUCCESS;

	case PS2_BOARD_FLUSH:
		Flush(uiUnit);
		return KERNAL_DRIVER_SUCCESS;

	case PS2_BOARD_GET_OVERRUNS:
		*puiResult = masPorts[uiUnit].uiOverruns;
		masPorts[uiUnit].uiOverruns = 0;
		return KERNAL_DRIVER_SUCCESS;

	default:
		return KERNAL_DRIVER_NOT_SUPPORTED;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Sleeps up to uiTicks for uiValue bytes to have arrived.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalPS2Board::Wait(uint16 uiUnit, uint32 uiValue, uint16 uiTicks)
{
	if ((uiValue == 0) || (uiValue > PS2_BOARD_BUFFER_SIZE))
	{
		return KERNAL_DRIVER_BAD_ARGUMENT;
	}
	return WaitFor(uiUnit, uiValue, uiTicks);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CKernalPS2Board::IsWaitOver(uint16 uiUnit, uint32 uiValue)
{
	return NumReceived(uiUnit) >= uiValue;
}


//////////////////////////////////////////////////////////////////////////
//
// The bytes in the port's FIFO into its ring, but the first if
// bDropFirst.
//
//////////////////////////////////////////////////////////////////////////
void CKernalPS2Board::Take(uint16 uiPort, bool bDropFirst)
{
	uint8	uiReady;
	uint8	uiByte;

	uiReady = (uint8)(PS2_STATE_READY << (uiPort * PS2_STATE_PORT_SHIFT));
	while (mpuiRegisters[PS2_STATE] & uiReady)
	{
		uiByte = mpuiRegisters[PS2_FIFO + uiPort];
		if (bDropFirst)
		{
			bDropFirst = false;
		}
		else
		{
			Add(uiPort, uiByte);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A full ring loses the byte.
//
//////////////////////////////////////////////////////////////////////////
void CKernalPS2Board::Add(uint16 uiPort, uint8 uiByte)
{
	SKernalPS2Port*	psPort;

	psPort = &masPorts[uiPort];
	if (NumReceived(uiPort) >= PS2_BOARD_BUFFER_SIZE)
	{
		psPort->uiOverruns++;
		return;
	}
	psPort->auiRing[psPort->uiHead] = uiByte;
	psPort->uiHead = (uint8)(psPort->uiHead + 1);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint8 CKernalPS2Board::NumReceived(uint16 uiPort)
{
	return (uint8)(masPorts[uiPort].uiHead - masPorts[uiPort].uiTail);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CKernalPS2Board::Flush(uint16 uiPort)
{
	masPorts[uiPort].uiTail = masPorts[uiPort].uiHead;
}


//////////////////////////////////////////////////////////////////////////
//
// uiByte to the device: start (the request to send), eight data bits from
// D0, odd parity and stop.  Afterwards the packet the clocks made is
// thrown away.
//
//////////////////////////////////////////////////////////////////////////
bool CKernalPS2Board::Send(uint16 uiPort, uint8 uiByte)
{
	uint16	uiFrame;
	uint16	uiOnes;
	uint16	i;
	bool	bSent;

	uiOnes = 0;
	for (i = 0; i < 8; i++)
	{
		uiOnes += (uint16)((uiByte >> i) & 1);
	}
	uiFrame = (uint16)(uiByte | ((uiOnes & 1) ? 0 : 0x100) | 0x200);

	WaitQuiet(uiPort);
	bSent = SendBits(uiPort, uiFrame);
	SetLow(uiPort, PS2_DIRECT_DATA, false);
	SetLow(uiPort, PS2_DIRECT_CLOCK, false);
	TakePacket(uiPort);
	return bSent;
}


//////////////////////////////////////////////////////////////////////////
//
// The request to send, the ten bits on the device's clocks, and its
// acknowledge on the eleventh: Data low.
//
//////////////////////////////////////////////////////////////////////////
bool CKernalPS2Board::SendBits(uint16 uiPort, uint16 uiFrame)
{
	uint32	uiStart;
	uint16	i;
	bool	bAcknowledged;

	SetLow(uiPort, PS2_DIRECT_CLOCK, true);
	for (i = 0; i < PS2_INHIBIT_READS; i++)
	{
		IsLineHigh(uiPort, PS2_DIRECT_CLOCK);
	}
	SetLow(uiPort, PS2_DIRECT_DATA, true);
	SetLow(uiPort, PS2_DIRECT_CLOCK, false);

	uiStart = KernalTicks();
	for (i = 0; i < 10; i++)
	{
		if (!WaitClock(uiPort, false, uiStart))
		{
			return false;
		}
		SetLow(uiPort, PS2_DIRECT_DATA, ((uiFrame >> i) & 1) == 0);
		if (!WaitClock(uiPort, true, uiStart))
		{
			return false;
		}
	}
	if (!WaitClock(uiPort, false, uiStart))
	{
		return false;
	}
	bAcknowledged = !IsLineHigh(uiPort, PS2_DIRECT_DATA);
	return WaitClock(uiPort, true, uiStart) && bAcknowledged;
}


//////////////////////////////////////////////////////////////////////////
//
// Until no packet can be under way, then the bytes the FIFO holds into the
// ring: holding Clock low while a packet hasn't ended would run into it,
// losing its byte.  With the port's waiting cleared, either a packet ends
// (the device's last frame, a byte that is now in the FIFO), or Clock is
// high for PS2_QUIET_TICKS, longer than any packet's quiet.  A device
// that keeps sending ends a packet after each frame, so the send can go
// then: a frame the device has started by the time Clock is held low is
// given up and sent again.  It is given PS2_BOARD_SEND_TICKS in all.
//
//////////////////////////////////////////////////////////////////////////
void CKernalPS2Board::WaitQuiet(uint16 uiPort)
{
	uint32	uiStart;
	uint32	uiQuiet;
	uint8	uiNoPacket;

	uiNoPacket = (uint8)(PS2_STATE_NO_PACKET << (uiPort * PS2_STATE_PORT_SHIFT));
	mpuiRegisters[PS2_FIFO + uiPort] = 0;
	Take(uiPort, false);

	uiStart = KernalTicks();
	uiQuiet = uiStart;
	while ((KernalTicks() - uiQuiet) < PS2_QUIET_TICKS)
	{
		if (!(mpuiRegisters[PS2_STATE] & uiNoPacket))
		{
			break;
		}
		if (!IsLineHigh(uiPort, PS2_DIRECT_CLOCK))
		{
			uiQuiet = KernalTicks();
		}
		if ((KernalTicks() - uiStart) > PS2_BOARD_SEND_TICKS)
		{
			break;
		}
	}
	mpuiRegisters[PS2_FIFO + uiPort] = 0;
	Take(uiPort, false);
}


//////////////////////////////////////////////////////////////////////////
//
// The packet a send made, once it has ended: its byte is thrown away.
//
//////////////////////////////////////////////////////////////////////////
void CKernalPS2Board::TakePacket(uint16 uiPort)
{
	uint32	uiStart;
	uint8	uiNoPacket;

	uiNoPacket = (uint8)(PS2_STATE_NO_PACKET << (uiPort * PS2_STATE_PORT_SHIFT));
	uiStart = KernalTicks();
	while ((mpuiRegisters[PS2_STATE] & uiNoPacket) && ((KernalTicks() - uiStart) <= PS2_PACKET_TICKS))
	{
	}
	mpuiRegisters[PS2_FIFO + uiPort] = 0;
	Take(uiPort, true);
}


//////////////////////////////////////////////////////////////////////////
//
// Until the port's Clock is bHigh; false if that takes past
// PS2_BOARD_SEND_TICKS from uiStart.
//
//////////////////////////////////////////////////////////////////////////
bool CKernalPS2Board::WaitClock(uint16 uiPort, bool bHigh, uint32 uiStart)
{
	while (IsLineHigh(uiPort, PS2_DIRECT_CLOCK) != bHigh)
	{
		if ((KernalTicks() - uiStart) > PS2_BOARD_SEND_TICKS)
		{
			return false;
		}
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CKernalPS2Board::IsLineHigh(uint16 uiPort, uint8 uiLine)
{
	return (mpuiRegisters[PS2_DIRECT] & (uint8)(uiLine << (uiPort * 2))) != 0;
}


//////////////////////////////////////////////////////////////////////////
//
// The GPIO pulls the port's line low (its output enabled with a low
// level), or lets it go.
//
//////////////////////////////////////////////////////////////////////////
void CKernalPS2Board::SetLow(uint16 uiPort, uint8 uiLine, bool bLow)
{
	uint8	uiBit;

	uiBit = (uint8)(uiLine << (uiPort * 2));
	muiDirect = (uint8)(muiDirect & ~uiBit);
	if (bLow)
	{
		muiDirect = (uint8)(muiDirect | (uiBit << PS2_DIRECT_DRIVE));
	}
	else
	{
		muiDirect = (uint8)(muiDirect & ~(uiBit << PS2_DIRECT_DRIVE));
	}
	mpuiRegisters[PS2_DIRECT] = muiDirect;
}


//The drivers of the PS2 boards, in slot order.
static CKernalPS2Board	gacPS2Boards[PS2_BOARD_MAX_BOARDS];


//////////////////////////////////////////////////////////////////////////
//
// The kernal's loader calls it, once the driver is in place and its
// globals are constructed, for each board it drives: the driver of the
// uiIndex'th, NULL if there are too many.
//
//////////////////////////////////////////////////////////////////////////
extern "C" CKernalDriver* KernalCreateDriver(uint16 uiIndex)
{
	return (uiIndex < PS2_BOARD_MAX_BOARDS) ? &gacPS2Boards[uiIndex] : NULL;
}

