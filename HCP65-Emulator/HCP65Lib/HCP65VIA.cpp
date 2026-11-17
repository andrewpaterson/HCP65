#include <string.h>
#include "BaseLib/MarkupTag.h"
#include "BaseLib/XMLWriter.h"
#include "HCP65.h"
#include "HCP65Mainboard.h"
#include "HCP65VIA.h"


//The VIAs by their ports, in the order VIA Control selects them (A6-A4) and the interrupt register reads them (D0-D7).
static const char*	gaszHCP65VIANames[HCP65_VIA_NUM_VIAS] = { "AB", "CD", "EF", "GH", "XJ", "KL", "MN", "OP" };


//////////////////////////////////////////////////////////////////////////
//
// At power on the timers hold anything; here they hold 0.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65VIA::Init(void)
{
	muiT1Latch = 0;
	muiT1Base = 0;
	muiT1Value = 0;
	mbT1Reload = false;
	muiT2LatchLow = 0;
	muiT2Base = 0;
	muiT2Value = 0;
	muiSR = 0;
	mbSRData = true;

	muiHostA = 0xFF;
	muiHostAMask = 0;
	muiHostB = 0xFF;
	muiHostBMask = 0;
	muiHeldA = 0xFF;
	muiHeldB = 0xFF;
	mbHostCA1 = true;
	mbHostCA2 = true;
	mbHostCB1 = true;
	mbHostCB2 = true;
	muiCA2Pulses = 0;
	muiCB2Pulses = 0;

	muiTick = 0;
	mbFrozen = false;
	muiACR = 0;
	Reset();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65VIA::Kill(void)
{
}


//////////////////////////////////////////////////////////////////////////
//
// RESB: every register but the timers' counters and latches and the
// shift register is cleared, so every pin is an input.  The timers go on
// counting (in one shot and timed modes now) but can't set their flags
// until they are written.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65VIA::Reset(void)
{
	RebaseT1();
	RebaseT2();

	muiORA = 0;
	muiORB = 0;
	muiDDRA = 0;
	muiDDRB = 0;
	muiIRA = 0;
	muiIRB = 0;
	muiACR = 0;
	muiPCR = 0;
	muiIFR = 0;
	muiIER = 0;

	mbT1Armed = false;
	mbPB7 = true;
	mbT2Armed = false;

	muiSRBits = 0;
	mbSRRunning = false;
	muiSRNextEdge = 0;
	mbSRClock = true;

	mbCA2Handshake = true;
	mbCB2Handshake = true;
	muiCA2PulseEnd = 0;
	muiCB2PulseEnd = 0;
}


//////////////////////////////////////////////////////////////////////////
//
// While RESB is held nothing in the VIA moves: CatchUp only moves time
// on.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65VIA::SetFrozen(bool bFrozen)
{
	mbFrozen = bFrozen;
}


//////////////////////////////////////////////////////////////////////////
//
// At the tick the VIA has been caught up to.
//
//////////////////////////////////////////////////////////////////////////
uint8 CHCP65VIA::Read(uint8 uiRegister)
{
	uint16	uiValue;

	switch (uiRegister & 0xF)
	{
	case HCP65_VIA_ORB:
		muiIFR &= ~HCP65_VIA_INT_CB1;
		if ((SRMode() != HCP65_VIA_SR_DISABLED) || (CB2Mode() & HCP65_VIA_C2_OUTPUT) || !(CB2Mode() & HCP65_VIA_C2_INDEPENDENT))
		{
			muiIFR &= ~HCP65_VIA_INT_CB2;
		}
		return ReadPortB();

	case HCP65_VIA_ORA:
		AccessPortA();
		return ReadPortA();

	case HCP65_VIA_DDRB:
		return muiDDRB;

	case HCP65_VIA_DDRA:
		return muiDDRA;

	case HCP65_VIA_T1CL:
		muiIFR &= ~HCP65_VIA_INT_T1;
		return (uint8)T1Value(muiTick);

	case HCP65_VIA_T1CH:
		return (uint8)(T1Value(muiTick) >> 8);

	case HCP65_VIA_T1LL:
		return (uint8)muiT1Latch;

	case HCP65_VIA_T1LH:
		return (uint8)(muiT1Latch >> 8);

	case HCP65_VIA_T2CL:
		muiIFR &= ~HCP65_VIA_INT_T2;
		return (uint8)T2Value(muiTick);

	case HCP65_VIA_T2CH:
		uiValue = T2Value(muiTick);
		return (uint8)(uiValue >> 8);

	case HCP65_VIA_SR:
		uiValue = muiSR;
		StartShift();
		return (uint8)uiValue;

	case HCP65_VIA_ACR:
		return muiACR;

	case HCP65_VIA_PCR:
		return muiPCR;

	case HCP65_VIA_IFR:
		return (uint8)(muiIFR | (IsInterrupting() ? HCP65_VIA_INT_ANY : 0));

	case HCP65_VIA_IER:
		return (uint8)(muiIER | HCP65_VIA_INT_ANY);

	default:  //HCP65_VIA_ORA_NH
		return ReadPortA();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// At the tick the VIA has been caught up to.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65VIA::Write(uint8 uiRegister, uint8 uiValue)
{
	uint8	uiOldB;
	uint8	uiOldMode;

	switch (uiRegister & 0xF)
	{
	case HCP65_VIA_ORB:
		uiOldB = GetPortB();
		muiORB = uiValue;
		muiIFR &= ~HCP65_VIA_INT_CB1;
		if (SRMode() == HCP65_VIA_SR_DISABLED)
		{
			if ((CB2Mode() & HCP65_VIA_C2_OUTPUT) || !(CB2Mode() & HCP65_VIA_C2_INDEPENDENT))
			{
				muiIFR &= ~HCP65_VIA_INT_CB2;
			}
			if (CB2Mode() == HCP65_VIA_C2_HANDSHAKE)
			{
				mbCB2Handshake = false;
			}
			else if (CB2Mode() == HCP65_VIA_C2_PULSE)
			{
				muiCB2PulseEnd = muiTick + 1;
				muiCB2Pulses++;
			}
		}
		else
		{
			muiIFR &= ~HCP65_VIA_INT_CB2;
		}
		PortBChanged(uiOldB);
		break;

	case HCP65_VIA_ORA:
		muiORA = uiValue;
		AccessPortA();
		UpdateHeld();
		break;

	case HCP65_VIA_DDRB:
		uiOldB = GetPortB();
		muiDDRB = uiValue;
		PortBChanged(uiOldB);
		break;

	case HCP65_VIA_DDRA:
		muiDDRA = uiValue;
		UpdateHeld();
		break;

	case HCP65_VIA_T1CL:
	case HCP65_VIA_T1LL:
		muiT1Latch = (uint16)((muiT1Latch & 0xFF00) | uiValue);
		RebaseT1();
		break;

	case HCP65_VIA_T1CH:
		muiT1Latch = (uint16)((muiT1Latch & 0x00FF) | (uiValue << 8));
		muiT1Base = muiTick + 1;
		muiT1Value = muiT1Latch;
		mbT1Reload = false;
		mbT1Armed = true;
		muiIFR &= ~HCP65_VIA_INT_T1;
		if (muiACR & HCP65_VIA_ACR_T1_PB7)
		{
			mbPB7 = false;
			UpdateHeld();
		}
		break;

	case HCP65_VIA_T1LH:
		muiT1Latch = (uint16)((muiT1Latch & 0x00FF) | (uiValue << 8));
		RebaseT1();
		muiIFR &= ~HCP65_VIA_INT_T1;
		break;

	case HCP65_VIA_T2CL:
		muiT2LatchLow = uiValue;
		break;

	case HCP65_VIA_T2CH:
		muiT2Base = muiTick + 1;
		muiT2Value = (uint16)((uiValue << 8) | muiT2LatchLow);
		mbT2Armed = true;
		muiIFR &= ~HCP65_VIA_INT_T2;
		break;

	case HCP65_VIA_SR:
		muiSR = uiValue;
		StartShift();
		break;

	case HCP65_VIA_ACR:
		RebaseT1();
		RebaseT2();
		uiOldMode = SRMode();
		muiACR = uiValue;
		if (SRMode() != uiOldMode)
		{
			mbSRRunning = false;
			mbSRClock = true;
			muiSRBits = 0;
			if (SRMode() == HCP65_VIA_SR_DISABLED)
			{
				muiIFR &= ~HCP65_VIA_INT_SR;
			}
		}
		UpdateHeld();
		break;

	case HCP65_VIA_PCR:
		muiPCR = uiValue;
		break;

	case HCP65_VIA_IFR:
		muiIFR &= ~(uiValue & HCP65_VIA_INT_FLAGS);
		break;

	case HCP65_VIA_IER:
		if (uiValue & HCP65_VIA_INT_ANY)
		{
			muiIER |= uiValue & HCP65_VIA_INT_FLAGS;
		}
		else
		{
			muiIER &= ~(uiValue & HCP65_VIA_INT_FLAGS);
		}
		break;

	default:  //HCP65_VIA_ORA_NH
		muiORA = uiValue;
		UpdateHeld();
		break;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The timers' time outs and the shift register's internal clock up to
// uiTick.  The timers are worked out, not counted.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65VIA::CatchUp(uint64 uiTick)
{
	uint64	uiTimeouts;
	uint64	uiDelta;
	uint64	uiT2Timeout;

	if (uiTick <= muiTick)
	{
		return;
	}

	if (mbFrozen)
	{
		uiDelta = uiTick - muiTick;
		muiT1Base += uiDelta;
		muiT2Base += uiDelta;
		muiSRNextEdge += uiDelta;
		muiTick = uiTick;
		return;
	}

	uiTimeouts = T1Timeouts(uiTick) - T1Timeouts(muiTick);
	if (uiTimeouts > 0)
	{
		if (IsT1Continuous())
		{
			muiIFR |= HCP65_VIA_INT_T1;
			mbT1Armed = false;
			if (uiTimeouts & 1)
			{
				mbPB7 = !mbPB7;
			}
		}
		else if (mbT1Armed)
		{
			muiIFR |= HCP65_VIA_INT_T1;
			mbT1Armed = false;
			mbPB7 = true;
		}
	}

	if (IsT2Timed() && mbT2Armed)
	{
		uiT2Timeout = muiT2Base + muiT2Value + 1;
		if ((uiT2Timeout > muiTick) && (uiT2Timeout <= uiTick))
		{
			muiIFR |= HCP65_VIA_INT_T2;
			mbT2Armed = false;
		}
	}

	CatchUpShift(uiTick);
	muiTick = uiTick;
	UpdateHeld();
}


//////////////////////////////////////////////////////////////////////////
//
// The tick at which the VIA's IRQ will next change on its own: an enabled
// timer or shift register flag that isn't set yet.
//
//////////////////////////////////////////////////////////////////////////
uint64 CHCP65VIA::NextEvent(void)
{
	uint64	uiNext;
	uint64	uiEvent;
	uint8	uiWaiting;

	if (mbFrozen)
	{
		return HCP65_NEVER;
	}

	uiWaiting = muiIER & ~muiIFR;
	uiNext = HCP65_NEVER;
	if ((uiWaiting & HCP65_VIA_INT_T1) && (IsT1Continuous() || mbT1Armed))
	{
		uiNext = T1NextTimeout();
	}
	if ((uiWaiting & HCP65_VIA_INT_T2) && IsT2Timed() && mbT2Armed)
	{
		uiEvent = muiT2Base + muiT2Value + 1;
		if (uiEvent < uiNext)
		{
			uiNext = uiEvent;
		}
	}
	if (uiWaiting & HCP65_VIA_INT_SR)
	{
		uiEvent = SRDoneTick();
		if (uiEvent < uiNext)
		{
			uiNext = uiEvent;
		}
	}
	return uiNext;
}


//////////////////////////////////////////////////////////////////////////
//
// IRQB is low.
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65VIA::IsInterrupting(void) const
{
	return (muiIFR & muiIER & HCP65_VIA_INT_FLAGS) != 0;
}


//////////////////////////////////////////////////////////////////////////
//
// The host drives the pins in uiMask at uiLevels and lets go of the
// rest, which keep their levels.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65VIA::HostDrivePortA(uint8 uiLevels, uint8 uiMask)
{
	muiHostA = uiLevels;
	muiHostAMask = uiMask;
	UpdateHeld();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65VIA::HostDrivePortB(uint8 uiLevels, uint8 uiMask)
{
	uint8	uiOld;

	uiOld = GetPortB();
	muiHostB = uiLevels;
	muiHostBMask = uiMask;
	PortBChanged(uiOld);
}


//////////////////////////////////////////////////////////////////////////
//
// CA1 is only ever an input.  Its active edge sets its flag, latches port
// A if latching is on and ends CA2's handshake.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65VIA::HostSetCA1(bool bHigh)
{
	if (bHigh == mbHostCA1)
	{
		return;
	}
	mbHostCA1 = bHigh;
	if (bHigh == ((muiPCR & HCP65_VIA_PCR_CA1_POSITIVE) != 0))
	{
		muiIFR |= HCP65_VIA_INT_CA1;
		if (muiACR & HCP65_VIA_ACR_PA_LATCH)
		{
			muiIRA = GetPortA();
		}
		if (CA2Mode() == HCP65_VIA_C2_HANDSHAKE)
		{
			mbCA2Handshake = true;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65VIA::HostSetCA2(bool bHigh)
{
	uint8	uiMode;

	if (bHigh == mbHostCA2)
	{
		return;
	}
	mbHostCA2 = bHigh;
	uiMode = CA2Mode();
	if (!(uiMode & HCP65_VIA_C2_OUTPUT) && (bHigh == ((uiMode & HCP65_VIA_C2_POSITIVE) != 0)))
	{
		muiIFR |= HCP65_VIA_INT_CA2;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Ignored while CB1 is the shift register's clock.  Otherwise its active
// edge sets its flag, latches port B if latching is on and ends CB2's
// handshake; with the shift register on an external clock CB2 changes on
// a fall (shifting out) and is shifted in on a rise.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65VIA::HostSetCB1(bool bHigh)
{
	uint8	uiMode;

	if (bHigh == mbHostCB1)
	{
		return;
	}
	mbHostCB1 = bHigh;
	if (IsCB1Output())
	{
		return;
	}

	if (bHigh == ((muiPCR & HCP65_VIA_PCR_CB1_POSITIVE) != 0))
	{
		muiIFR |= HCP65_VIA_INT_CB1;
		if (muiACR & HCP65_VIA_ACR_PB_LATCH)
		{
			muiIRB = GetPortB();
		}
		if ((SRMode() == HCP65_VIA_SR_DISABLED) && (CB2Mode() == HCP65_VIA_C2_HANDSHAKE))
		{
			mbCB2Handshake = true;
		}
	}

	uiMode = SRMode();
	if ((uiMode == HCP65_VIA_SR_IN_CB1) || (uiMode == HCP65_VIA_SR_OUT_CB1))
	{
		if (!bHigh)
		{
			if (uiMode == HCP65_VIA_SR_OUT_CB1)
			{
				ShiftOut();
			}
		}
		else
		{
			if (uiMode == HCP65_VIA_SR_IN_CB1)
			{
				ShiftIn();
			}
			muiSRBits++;
			if (muiSRBits == 8)
			{
				muiSRBits = 0;
				muiIFR |= HCP65_VIA_INT_SR;
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// CB2 is the shift register's when it is enabled: its data in, or the
// host's level is ignored.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65VIA::HostSetCB2(bool bHigh)
{
	uint8	uiMode;

	if (bHigh == mbHostCB2)
	{
		return;
	}
	mbHostCB2 = bHigh;
	uiMode = CB2Mode();
	if ((SRMode() == HCP65_VIA_SR_DISABLED) && !(uiMode & HCP65_VIA_C2_OUTPUT) && (bHigh == ((uiMode & HCP65_VIA_C2_POSITIVE) != 0)))
	{
		muiIFR |= HCP65_VIA_INT_CB2;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The pins' levels: the VIA's where it drives them, the host's where it
// does, and the bus holders' elsewhere.
//
//////////////////////////////////////////////////////////////////////////
uint8 CHCP65VIA::GetPortA(void) const
{
	uint8	uiInputs;

	uiInputs = (uint8)((muiHostA & muiHostAMask) | (muiHeldA & ~muiHostAMask));
	return (uint8)((muiORA & muiDDRA) | (uiInputs & ~muiDDRA));
}


//////////////////////////////////////////////////////////////////////////
//
// With T1's output on, PB7 is T1's whatever DDRB says.
//
//////////////////////////////////////////////////////////////////////////
uint8 CHCP65VIA::GetPortB(void) const
{
	uint8	uiInputs;
	uint8	uiPins;

	uiInputs = (uint8)((muiHostB & muiHostBMask) | (muiHeldB & ~muiHostBMask));
	uiPins = (uint8)((muiORB & muiDDRB) | (uiInputs & ~muiDDRB));
	if (muiACR & HCP65_VIA_ACR_T1_PB7)
	{
		uiPins = (uint8)((uiPins & 0x7F) | (mbPB7 ? 0x80 : 0));
	}
	return uiPins;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65VIA::GetCA2(void)
{
	switch (CA2Mode())
	{
	case HCP65_VIA_C2_HANDSHAKE:
		return mbCA2Handshake;
	case HCP65_VIA_C2_PULSE:
		return muiTick >= muiCA2PulseEnd;
	case HCP65_VIA_C2_LOW:
		return false;
	case HCP65_VIA_C2_HIGH:
		return true;
	default:
		return mbHostCA2;
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65VIA::GetCB1(void)
{
	return IsCB1Output() ? mbSRClock : mbHostCB1;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65VIA::GetCB2(void)
{
	if (SRMode() != HCP65_VIA_SR_DISABLED)
	{
		return IsCB2Output() ? mbSRData : mbHostCB2;
	}
	switch (CB2Mode())
	{
	case HCP65_VIA_C2_HANDSHAKE:
		return mbCB2Handshake;
	case HCP65_VIA_C2_PULSE:
		return muiTick >= muiCB2PulseEnd;
	case HCP65_VIA_C2_LOW:
		return false;
	case HCP65_VIA_C2_HIGH:
		return true;
	default:
		return mbHostCB2;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A pulse output lasts one tick, which the host may not be looking at:
// these count them.
//
//////////////////////////////////////////////////////////////////////////
uint64 CHCP65VIA::NumCA2Pulses(void) const { return muiCA2Pulses; }
uint64 CHCP65VIA::NumCB2Pulses(void) const { return muiCB2Pulses; }
uint16 CHCP65VIA::GetT1Counter(void) { return T1Value(muiTick); }
uint16 CHCP65VIA::GetT2Counter(void) { return T2Value(muiTick); }
uint64 CHCP65VIA::GetTick(void) const { return muiTick; }


//////////////////////////////////////////////////////////////////////////
//
// With latching on, IRA as CA1 last latched it.  Port A reads its pins,
// outputs too.
//
//////////////////////////////////////////////////////////////////////////
uint8 CHCP65VIA::ReadPortA(void)
{
	return (muiACR & HCP65_VIA_ACR_PA_LATCH) ? muiIRA : GetPortA();
}


//////////////////////////////////////////////////////////////////////////
//
// Port B reads ORB where it is an output (PB7 T1's, with its output on),
// and its pins, or IRB with latching on, where it is an input.
//
//////////////////////////////////////////////////////////////////////////
uint8 CHCP65VIA::ReadPortB(void)
{
	uint8	uiInputs;
	uint8	uiValue;

	uiInputs = (muiACR & HCP65_VIA_ACR_PB_LATCH) ? muiIRB : GetPortB();
	uiValue = (uint8)((muiORB & muiDDRB) | (uiInputs & ~muiDDRB));
	if (muiACR & HCP65_VIA_ACR_T1_PB7)
	{
		uiValue = (uint8)((uiValue & 0x7F) | (mbPB7 ? 0x80 : 0));
	}
	return uiValue;
}


//////////////////////////////////////////////////////////////////////////
//
// A read or write of ORA (not ORA without handshake): CA1's flag is
// cleared, and CA2's unless it is an independent input; CA2 handshakes.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65VIA::AccessPortA(void)
{
	uint8	uiMode;

	uiMode = CA2Mode();
	muiIFR &= ~HCP65_VIA_INT_CA1;
	if ((uiMode & HCP65_VIA_C2_OUTPUT) || !(uiMode & HCP65_VIA_C2_INDEPENDENT))
	{
		muiIFR &= ~HCP65_VIA_INT_CA2;
	}
	if (uiMode == HCP65_VIA_C2_HANDSHAKE)
	{
		mbCA2Handshake = false;
	}
	else if (uiMode == HCP65_VIA_C2_PULSE)
	{
		muiCA2PulseEnd = muiTick + 1;
		muiCA2Pulses++;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The bus holders take the pins' levels.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65VIA::UpdateHeld(void)
{
	muiHeldA = GetPortA();
	muiHeldB = GetPortB();
}


//////////////////////////////////////////////////////////////////////////
//
// Port B's pins were uiOldPins: a fall of PB6 is a pulse for T2's
// counting.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65VIA::PortBChanged(uint8 uiOldPins)
{
	if (!IsT2Timed() && (uiOldPins & 0x40) && !(GetPortB() & 0x40))
	{
		muiT2Value--;
		if ((muiT2Value == 0) && mbT2Armed)
		{
			muiIFR |= HCP65_VIA_INT_T2;
			mbT2Armed = false;
		}
	}
	UpdateHeld();
}


uint8 CHCP65VIA::CA2Mode(void) { return (uint8)((muiPCR & HCP65_VIA_PCR_CA2) >> 1); }
uint8 CHCP65VIA::CB2Mode(void) { return (uint8)((muiPCR & HCP65_VIA_PCR_CB2) >> 5); }
uint8 CHCP65VIA::SRMode(void) { return (uint8)((muiACR & HCP65_VIA_ACR_SR) >> HCP65_VIA_ACR_SR_SHIFT); }
bool CHCP65VIA::IsT1Continuous(void) const { return (muiACR & HCP65_VIA_ACR_T1_CONTINUOUS) != 0; }
bool CHCP65VIA::IsT2Timed(void) const { return (muiACR & HCP65_VIA_ACR_T2_PULSES) == 0; }


//////////////////////////////////////////////////////////////////////////
//
// The shift register's internal clock drives CB1.
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65VIA::IsCB1Output(void)
{
	uint8	uiMode;

	uiMode = SRMode();
	return (uiMode != HCP65_VIA_SR_DISABLED) && (uiMode != HCP65_VIA_SR_IN_CB1) && (uiMode != HCP65_VIA_SR_OUT_CB1);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65VIA::IsCB2Output(void)
{
	if (SRMode() != HCP65_VIA_SR_DISABLED)
	{
		return (SRMode() & 4) != 0;
	}
	return (CB2Mode() & HCP65_VIA_C2_OUTPUT) != 0;
}


//////////////////////////////////////////////////////////////////////////
//
// T1's counter at uiTick (not before the tick it was caught up to).
//
//////////////////////////////////////////////////////////////////////////
uint16 CHCP65VIA::T1Value(uint64 uiTick)
{
	uint64	uiFirst;
	uint64	uiSince;
	uint64	uiInto;

	if (uiTick <= muiT1Base)
	{
		return mbT1Reload ? 0xFFFF : muiT1Value;
	}
	uiFirst = T1FirstTimeout();
	if (uiTick < uiFirst)
	{
		return (uint16)(muiT1Value - (uiTick - muiT1Base));
	}
	uiSince = uiTick - uiFirst;
	if (uiSince == 0)
	{
		return 0xFFFF;
	}
	if (IsT1Continuous())
	{
		uiInto = (uiSince - 1) % ((uint64)muiT1Latch + 2);
		return (uiInto <= muiT1Latch) ? (uint16)(muiT1Latch - uiInto) : 0xFFFF;
	}
	return (uint16)(0xFFFF - uiSince);
}


//////////////////////////////////////////////////////////////////////////
//
// The tick T1 first times out at (reads $FFFF from 0) from its base.
//
//////////////////////////////////////////////////////////////////////////
uint64 CHCP65VIA::T1FirstTimeout(void)
{
	return mbT1Reload ? muiT1Base : muiT1Base + muiT1Value + 1;
}


//////////////////////////////////////////////////////////////////////////
//
// The ticks between T1's time outs after the first.
//
//////////////////////////////////////////////////////////////////////////
uint64 CHCP65VIA::T1Period(void)
{
	return IsT1Continuous() ? (uint64)muiT1Latch + 2 : 0x10000;
}


//////////////////////////////////////////////////////////////////////////
//
// How many times T1 has timed out from its base up to uiTick.
//
//////////////////////////////////////////////////////////////////////////
uint64 CHCP65VIA::T1Timeouts(uint64 uiTick)
{
	uint64	uiFirst;

	uiFirst = T1FirstTimeout();
	if (uiTick < uiFirst)
	{
		return 0;
	}
	return (uiTick - uiFirst) / T1Period() + 1;
}


//////////////////////////////////////////////////////////////////////////
//
// The first tick after the one caught up to that T1 times out at.
//
//////////////////////////////////////////////////////////////////////////
uint64 CHCP65VIA::T1NextTimeout(void)
{
	uint64	uiFirst;
	uint64	uiPeriod;

	uiFirst = T1FirstTimeout();
	if (uiFirst > muiTick)
	{
		return uiFirst;
	}
	uiPeriod = T1Period();
	return uiFirst + ((muiTick - uiFirst) / uiPeriod + 1) * uiPeriod;
}


//////////////////////////////////////////////////////////////////////////
//
// T1's counter is described from the tick caught up to, before its latch
// or mode changes.  Timing out at it, continuous T1 still reloads on the
// next tick.  A counter just written hasn't started.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65VIA::RebaseT1(void)
{
	uint64	uiFirst;
	uint16	uiValue;
	bool	bTimeout;

	if (muiTick < muiT1Base)
	{
		return;
	}
	uiValue = T1Value(muiTick);
	uiFirst = T1FirstTimeout();
	bTimeout = (muiTick >= uiFirst) && (((muiTick - uiFirst) % T1Period()) == 0);
	muiT1Base = muiTick;
	mbT1Reload = bTimeout && IsT1Continuous();
	muiT1Value = mbT1Reload ? 0xFFFF : uiValue;
}


//////////////////////////////////////////////////////////////////////////
//
// T2's counter at uiTick.  Counting pulses, it only moves on PB6's falls.
//
//////////////////////////////////////////////////////////////////////////
uint16 CHCP65VIA::T2Value(uint64 uiTick)
{
	if (!IsT2Timed() || (uiTick <= muiT2Base))
	{
		return muiT2Value;
	}
	return (uint16)(muiT2Value - (uiTick - muiT2Base));
}


//////////////////////////////////////////////////////////////////////////
//
// T2's counter from the tick caught up to, before its mode changes.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65VIA::RebaseT2(void)
{
	if (muiTick < muiT2Base)
	{
		return;
	}
	muiT2Value = T2Value(muiTick);
	muiT2Base = muiTick;
}


//////////////////////////////////////////////////////////////////////////
//
// Reading or writing the shift register clears its flag and starts eight
// bits again; an internal clock starts with CB1 high.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65VIA::StartShift(void)
{
	muiIFR &= ~HCP65_VIA_INT_SR;
	muiSRBits = 0;
	if (IsCB1Output())
	{
		mbSRRunning = true;
		mbSRClock = true;
		muiSRNextEdge = muiTick + SRHalfPeriod();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The ticks from one edge of the internal clock to the next.
//
//////////////////////////////////////////////////////////////////////////
uint64 CHCP65VIA::SRHalfPeriod(void)
{
	uint8	uiMode;

	uiMode = SRMode();
	if ((uiMode == HCP65_VIA_SR_IN_PHI2) || (uiMode == HCP65_VIA_SR_OUT_PHI2))
	{
		return 1;
	}
	return (uint64)muiT2LatchLow + 2;
}


//////////////////////////////////////////////////////////////////////////
//
// The internal clock's edge at muiSRNextEdge.  Eight rises are a byte:
// the flag is set and the clock stops, but for shifting out free running,
// which never sets it.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65VIA::SREdge(void)
{
	uint8	uiMode;

	uiMode = SRMode();
	if (mbSRClock)
	{
		mbSRClock = false;
		if (uiMode & 4)
		{
			ShiftOut();
		}
	}
	else
	{
		mbSRClock = true;
		if (!(uiMode & 4))
		{
			ShiftIn();
		}
		muiSRBits++;
		if (muiSRBits == 8)
		{
			muiSRBits = 0;
			if (uiMode != HCP65_VIA_SR_OUT_FREE)
			{
				muiIFR |= HCP65_VIA_INT_SR;
				mbSRRunning = false;
				return;
			}
		}
	}
	muiSRNextEdge += SRHalfPeriod();
}


//////////////////////////////////////////////////////////////////////////
//
// D7 goes out on CB2 and round into D0.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65VIA::ShiftOut(void)
{
	mbSRData = (muiSR & 0x80) != 0;
	muiSR = (uint8)((muiSR << 1) | (muiSR >> 7));
}


//////////////////////////////////////////////////////////////////////////
//
// CB2 comes in at D0.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65VIA::ShiftIn(void)
{
	muiSR = (uint8)((muiSR << 1) | (mbHostCB2 ? 1 : 0));
}


//////////////////////////////////////////////////////////////////////////
//
// The tick the internal clock's eighth rise will set the flag at.
//
//////////////////////////////////////////////////////////////////////////
uint64 CHCP65VIA::SRDoneTick(void)
{
	uint64	uiEdges;

	if (!mbSRRunning || (SRMode() == HCP65_VIA_SR_OUT_FREE))
	{
		return HCP65_NEVER;
	}
	uiEdges = (uint64)(8 - muiSRBits) * 2 - (mbSRClock ? 0 : 1);
	return muiSRNextEdge + (uiEdges - 1) * SRHalfPeriod();
}


//////////////////////////////////////////////////////////////////////////
//
// The internal clock's edges up to uiTick.  Shifting out free running,
// sixteen edges put everything back as it was, so they are skipped.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65VIA::CatchUpShift(uint64 uiTick)
{
	uint64	uiHalf;
	uint64	uiEdges;

	if (!mbSRRunning)
	{
		return;
	}
	uiHalf = SRHalfPeriod();
	if ((SRMode() == HCP65_VIA_SR_OUT_FREE) && (muiSRNextEdge <= uiTick))
	{
		uiEdges = (uiTick - muiSRNextEdge) / uiHalf + 1;
		if (uiEdges > 16)
		{
			muiSRNextEdge += ((uiEdges - 16) / 16) * 16 * uiHalf;
		}
	}
	while (mbSRRunning && (muiSRNextEdge <= uiTick))
	{
		SREdge();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Every breakout is fitted.  The board starts held in reset.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65VIABoard::Init(void)
{
	int		i;

	CHCP65Device::Init(HCP65_SMALL_SLOT_SIZE);

	mcROM.Init(HCP65_VIA_BOARD_NAME, HCP65_DEVICE_MANUFACTURER, 0, DEVICE_BUS_32KB, 0, HCP65_VIA_DRIVER);
	mcROM.AddRegisters(HCP65_VIA_REGISTERS, HCP65_VIA_NUM_VIAS * HCP65_VIA_NUM_REGISTERS, DEVICE_REGISTER_READ);
	mcROM.AddRegisters(HCP65_VIA_REGISTERS, HCP65_VIA_NUM_VIAS * HCP65_VIA_NUM_REGISTERS, DEVICE_REGISTER_WRITE);
	mcROM.AddRegister(HCP65_VIA_REGISTERS + HCP65_VIA_INTERRUPTS, DEVICE_REGISTER_READ);

	for (i = 0; i < HCP65_VIA_NUM_VIAS; i++)
	{
		macVIAs[i].Init();
		macVIAs[i].SetFrozen(true);
	}
	for (i = 0; i < HCP65_VIA_NUM_BREAKOUTS; i++)
	{
		mabBreakouts[i] = true;
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65VIABoard::Kill(void)
{
	int		i;

	for (i = 0; i < HCP65_VIA_NUM_VIAS; i++)
	{
		macVIAs[i].Kill();
	}
	CHCP65Device::Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// No <Breakout> is every breakout fitted; otherwise only those listed
// are.
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65VIABoard::LoadXML(CMarkupTag* pcTag, const char* szDirectory, CChars* pszError)
{
	CMarkupTag*		pcBreakout;
	STagIterator	sIterator;
	bool			abSeen[HCP65_VIA_NUM_BREAKOUTS];
	bool			bAny;
	uint32			uiSocket;
	int				i;

	for (i = 0; i < HCP65_VIA_NUM_BREAKOUTS; i++)
	{
		abSeen[i] = false;
	}
	bAny = false;
	for (pcBreakout = pcTag->GetTag(&sIterator); pcBreakout; pcBreakout = pcTag->GetNextTag(&sIterator))
	{
		if (strcmp(pcBreakout->GetName(), HCP65_VIA_BREAKOUT_XML_TAG) != 0)
		{
			return HCP65XMLError(pszError, pcBreakout, "Expected <" HCP65_VIA_BREAKOUT_XML_TAG ">", NULL);
		}
		if (!HCP65XMLNumber(pcBreakout, "Socket", true, HCP65_VIA_NUM_BREAKOUTS - 1, 0, &uiSocket, pszError))
		{
			return false;
		}
		if (abSeen[uiSocket])
		{
			return HCP65XMLError(pszError, pcBreakout, "A breakout is in this socket already", NULL);
		}
		abSeen[uiSocket] = true;
		bAny = true;
	}

	for (i = 0; i < HCP65_VIA_NUM_BREAKOUTS; i++)
	{
		mabBreakouts[i] = !bAny || abSeen[i];
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Nothing when every breakout is fitted, else a <Breakout> for each one
// that is.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65VIABoard::SaveXML(CXMLWriter* pcWriter)
{
	int		i;

	for (i = 0; i < HCP65_VIA_NUM_BREAKOUTS; i++)
	{
		if (!mabBreakouts[i])
		{
			break;
		}
	}
	if (i == HCP65_VIA_NUM_BREAKOUTS)
	{
		return;
	}

	for (i = 0; i < HCP65_VIA_NUM_BREAKOUTS; i++)
	{
		if (mabBreakouts[i])
		{
			pcWriter->Begin(HCP65_VIA_BREAKOUT_XML_TAG);
			pcWriter->Attribute("Socket", (int32)i);
			pcWriter->End();
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Below $400 the device ROM; from $400 to $7FF VIA Control's decode of
// A7-A0.  Nothing else drives the bus.
//
//////////////////////////////////////////////////////////////////////////
uint8 CHCP65VIABoard::Read(uint32 uiOffset)
{
	CHCP65VIA*	pcVIA;
	uint32		uiDecode;

	if (uiOffset < HCP65_VIA_REGISTERS)
	{
		return mcROM.Read(uiOffset);
	}
	if (uiOffset >= HCP65_VIA_REGISTERS_END)
	{
		return 0xFF;
	}

	uiDecode = uiOffset & HCP65_VIA_DECODE;
	if (uiDecode & HCP65_VIA_INTERRUPTS)
	{
		return (uiDecode == HCP65_VIA_INTERRUPTS) ? ReadInterrupts() : 0xFF;
	}
	pcVIA = GetVIA((int)(uiDecode >> 4));
	return pcVIA ? pcVIA->Read((uint8)uiDecode) : 0xFF;
}


//////////////////////////////////////////////////////////////////////////
//
// The device ROM is flash and the interrupt register can't be written;
// the VIAs are in reset while the board is held.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65VIABoard::Write(uint32 uiOffset, uint8 uiValue)
{
	CHCP65VIA*	pcVIA;
	uint32		uiDecode;

	if (mbHeldInReset || (uiOffset < HCP65_VIA_REGISTERS) || (uiOffset >= HCP65_VIA_REGISTERS_END))
	{
		return;
	}
	uiDecode = uiOffset & HCP65_VIA_DECODE;
	if (uiDecode & HCP65_VIA_INTERRUPTS)
	{
		return;
	}
	pcVIA = GetVIA((int)(uiDecode >> 4));
	if (pcVIA)
	{
		pcVIA->Write((uint8)uiDecode, uiValue);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The VIAs' RESB is the device's reset.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65VIABoard::Reset(void)
{
	int		i;

	for (i = 0; i < HCP65_VIA_NUM_VIAS; i++)
	{
		macVIAs[i].Reset();
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65VIABoard::SetHeldInReset(bool bHeld)
{
	int		i;

	CHCP65Device::SetHeldInReset(bHeld);
	for (i = 0; i < HCP65_VIA_NUM_VIAS; i++)
	{
		macVIAs[i].SetFrozen(bHeld);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Every VIA, fitted or not, so one fitted later starts from now.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65VIABoard::CatchUp(uint64 uiCycle)
{
	uint64	uiTick;
	int		i;

	if (!mpcHCP65)
	{
		return;
	}
	uiTick = mpcHCP65->DeviceTick(uiCycle, PHI2Rate());
	for (i = 0; i < HCP65_VIA_NUM_VIAS; i++)
	{
		macVIAs[i].CatchUp(uiTick);
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint64 CHCP65VIABoard::NextEvent(void)
{
	uint64		uiNext;
	uint64		uiEvent;
	CHCP65VIA*	pcVIA;
	int			i;

	if (!mpcHCP65 || mbHeldInReset)
	{
		return HCP65_NEVER;
	}
	uiNext = HCP65_NEVER;
	for (i = 0; i < HCP65_VIA_NUM_VIAS; i++)
	{
		pcVIA = GetVIA(i);
		if (pcVIA)
		{
			uiEvent = pcVIA->NextEvent();
			if (uiEvent < uiNext)
			{
				uiNext = uiEvent;
			}
		}
	}
	return (uiNext == HCP65_NEVER) ? HCP65_NEVER : mpcHCP65->CycleOfDeviceTick(uiNext, PHI2Rate());
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65VIABoard::IsInterrupting(void)
{
	return !mbHeldInReset && (ReadInterrupts() != 0);
}


//////////////////////////////////////////////////////////////////////////
//
// A VIA's breakout taken out leaves the VIA as it was, to come back with
// the breakout.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65VIABoard::SetBreakout(int iBreakout, bool bFitted)
{
	if ((iBreakout >= 0) && (iBreakout < HCP65_VIA_NUM_BREAKOUTS))
	{
		mabBreakouts[iBreakout] = bFitted;
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65VIABoard::HasBreakout(int iBreakout)
{
	return (iBreakout >= 0) && (iBreakout < HCP65_VIA_NUM_BREAKOUTS) && mabBreakouts[iBreakout];
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
int CHCP65VIABoard::NumVIAs(void) const
{
	int		i;
	int		iCount;

	iCount = 0;
	for (i = 0; i < HCP65_VIA_NUM_BREAKOUTS; i++)
	{
		if (mabBreakouts[i])
		{
			iCount += 2;
		}
	}
	return iCount;
}


//////////////////////////////////////////////////////////////////////////
//
// VIA iVIA (0 is AB to 7 OP), NULL if its breakout isn't fitted.
//
//////////////////////////////////////////////////////////////////////////
CHCP65VIA* CHCP65VIABoard::GetVIA(int iVIA)
{
	if ((iVIA < 0) || (iVIA >= HCP65_VIA_NUM_VIAS) || !mabBreakouts[iVIA / 2])
	{
		return NULL;
	}
	return &macVIAs[iVIA];
}


//////////////////////////////////////////////////////////////////////////
//
// The interrupt register: bit n is 1 where VIA n is interrupting.
//
//////////////////////////////////////////////////////////////////////////
uint8 CHCP65VIABoard::ReadInterrupts(void)
{
	CHCP65VIA*	pcVIA;
	uint8		uiInterrupts;
	int			i;

	uiInterrupts = 0;
	for (i = 0; i < HCP65_VIA_NUM_VIAS; i++)
	{
		pcVIA = GetVIA(i);
		if (pcVIA && pcVIA->IsInterrupting())
		{
			uiInterrupts |= (uint8)(1 << i);
		}
	}
	return uiInterrupts;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
const char* CHCP65VIABoard::GetVIAName(int iVIA)
{
	if ((iVIA < 0) || (iVIA >= HCP65_VIA_NUM_VIAS))
	{
		return NULL;
	}
	return gaszHCP65VIANames[iVIA];
}


//////////////////////////////////////////////////////////////////////////
//
// The VIAs' PHI2: the device clock halved.
//
//////////////////////////////////////////////////////////////////////////
uint64 CHCP65VIABoard::PHI2Rate(void)
{
	return mpcHCP65->GetClockRate() / 2;
}

