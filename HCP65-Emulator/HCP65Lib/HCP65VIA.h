#ifndef __HCP65_VIA_H__
#define __HCP65_VIA_H__
#include "HCP65Device.h"


#define HCP65_VIA_BOARD_NAME		"VIA Board"
#define HCP65_VIA_DRIVER			"VIA.DRV"
#define HCP65_VIA_REGISTERS			0x400		//$400 to $7FF: the VIAs and the interrupt register, repeating every $100.
#define HCP65_VIA_REGISTERS_END		0x800
#define HCP65_VIA_DECODE			0xFF		//A0-A7; A8 and A9 aren't decoded.
#define HCP65_VIA_INTERRUPTS		0x80		//A7 set and A6-A0 clear: the VIAs' interrupts.  A7 set otherwise decodes nothing.
#define HCP65_VIA_NUM_VIAS			8
#define HCP65_VIA_NUM_BREAKOUTS		4			//Two VIAs on each.
#define HCP65_VIA_NUM_REGISTERS		16
#define HCP65_VIA_BREAKOUT_XML_TAG	"Breakout"	//A fitted VIA Breakout in the board's <Board> tag (LoadXML).

//A W65C22's registers, RS3-RS0.
#define HCP65_VIA_ORB				0x0			//Read: IRB.
#define HCP65_VIA_ORA				0x1			//Read: IRA.  Handshakes on CA2.
#define HCP65_VIA_DDRB				0x2
#define HCP65_VIA_DDRA				0x3
#define HCP65_VIA_T1CL				0x4			//Write: T1 low latch.  Read: T1 counter low, clears the T1 flag.
#define HCP65_VIA_T1CH				0x5			//Write: T1 high latch, then the latches to the counter, which starts it.
#define HCP65_VIA_T1LL				0x6
#define HCP65_VIA_T1LH				0x7			//Write: clears the T1 flag.
#define HCP65_VIA_T2CL				0x8			//Write: T2 low latch.  Read: T2 counter low, clears the T2 flag.
#define HCP65_VIA_T2CH				0x9			//Write: T2 counter high, and the low latch to the counter low, which starts it.
#define HCP65_VIA_SR				0xA
#define HCP65_VIA_ACR				0xB
#define HCP65_VIA_PCR				0xC
#define HCP65_VIA_IFR				0xD
#define HCP65_VIA_IER				0xE
#define HCP65_VIA_ORA_NH			0xF			//ORA without the handshake.

#define HCP65_VIA_INT_CA2			0x01
#define HCP65_VIA_INT_CA1			0x02
#define HCP65_VIA_INT_SR			0x04
#define HCP65_VIA_INT_CB2			0x08
#define HCP65_VIA_INT_CB1			0x10
#define HCP65_VIA_INT_T2			0x20
#define HCP65_VIA_INT_T1			0x40
#define HCP65_VIA_INT_FLAGS			0x7F
#define HCP65_VIA_INT_ANY			0x80		//IFR: any enabled flag.  IER write: set, rather than clear, the bits.

#define HCP65_VIA_ACR_PA_LATCH		0x01
#define HCP65_VIA_ACR_PB_LATCH		0x02
#define HCP65_VIA_ACR_SR			0x1C
#define HCP65_VIA_ACR_SR_SHIFT		2
#define HCP65_VIA_ACR_T2_PULSES		0x20
#define HCP65_VIA_ACR_T1_CONTINUOUS	0x40
#define HCP65_VIA_ACR_T1_PB7		0x80

//The shift register's modes, ACR D4-D2.
#define HCP65_VIA_SR_DISABLED		0
#define HCP65_VIA_SR_IN_T2			1
#define HCP65_VIA_SR_IN_PHI2		2
#define HCP65_VIA_SR_IN_CB1			3
#define HCP65_VIA_SR_OUT_FREE		4
#define HCP65_VIA_SR_OUT_T2			5
#define HCP65_VIA_SR_OUT_PHI2		6
#define HCP65_VIA_SR_OUT_CB1		7

#define HCP65_VIA_PCR_CA1_POSITIVE	0x01
#define HCP65_VIA_PCR_CA2			0x0E		//CA2's control, D3-D1.
#define HCP65_VIA_PCR_CB1_POSITIVE	0x10
#define HCP65_VIA_PCR_CB2			0xE0		//CB2's control, D7-D5.

//A control line's mode, from CA2's or CB2's three bits of the PCR.
#define HCP65_VIA_C2_INDEPENDENT	0x1			//An input whose flag isn't cleared by the port's ORA or ORB.
#define HCP65_VIA_C2_POSITIVE		0x2			//An input's active edge is positive.
#define HCP65_VIA_C2_OUTPUT			0x4
#define HCP65_VIA_C2_HANDSHAKE		0x4
#define HCP65_VIA_C2_PULSE			0x5
#define HCP65_VIA_C2_LOW			0x6
#define HCP65_VIA_C2_HIGH			0x7


//One W65C22S.  Its time is in its PHI2 ticks.
//
//The far end of its lines is the host.  The host drives any of PA0-PA7 and PB0-PB7 (HostDrivePortA's mask) and CA1, CA2,
//CB1 and CB2; what the host drives where the VIA drives too is ignored.  The W65C22S has bus holders, not pull ups: a pin
//nothing drives keeps its last level (they start high here; on the real chip they start at anything).  The host's
//changes happen at the tick the VIA has been caught up to, so catch it up first.  A level change on CA1, CA2, CB1 or CB2
//is an edge when the line is the VIA's input, and a fall of PB6, whoever drives it, is a T2 pulse.
//
//The timers count as the W65C22 does: after T1C-H is written with N the counter reads N on the next tick, reaches 0 N
//ticks later and times out (T1's flag, PB7) the tick after that, reading $FFFF.  Continuous T1 then reloads from the latch
//on the next tick, so it times out every N+2 ticks; one shot T1 and timed T2 count on through $FFFF, with no more flags
//until they are written again.  Pulse counting T2 sets its flag on reaching 0.
//
//The shift register's internal clock is CB1, an edge every N+2 ticks under T2 (N is T2's low latch) and every tick under
//PHI2: CB2 changes on CB1's falls and is shifted in on its rises.  Shifting under T2 doesn't stop T2 counting on its own.
//With an external clock, the host's CB1 shifts.  A shift's own CB1 and CB2 edges don't set the CB1 and CB2 flags.
//
//Reset clears every register but the timers' counters and latches and the shift register, which stop while it lasts
//(SetFrozen), and leaves the timers unable to set their flags until they are written.
class CHCP65VIA
{
protected:
	uint8		muiORA;
	uint8		muiORB;
	uint8		muiDDRA;
	uint8		muiDDRB;
	uint8		muiIRA;				//Port A latched on CA1's active edge.
	uint8		muiIRB;				//Port B latched on CB1's.
	uint8		muiACR;
	uint8		muiPCR;
	uint8		muiIFR;				//D0-D6.
	uint8		muiIER;				//D0-D6.

	uint16		muiT1Latch;
	uint64		muiT1Base;			//The counter is muiT1Value at this tick...
	uint16		muiT1Value;
	bool		mbT1Reload;			//... unless it's timing out at it, to reload from the latch on the next.
	bool		mbT1Armed;			//T1 hasn't timed out since T1C-H was written: one shot T1 may set its flag.
	bool		mbPB7;				//T1's output.

	uint8		muiT2LatchLow;
	uint64		muiT2Base;			//Timed, the counter is muiT2Value at this tick.  Pulse counting, it is muiT2Value.
	uint16		muiT2Value;
	bool		mbT2Armed;

	uint8		muiSR;
	uint8		muiSRBits;			//Shifted since the shift register was read or written.
	bool		mbSRRunning;		//An internal clock is running.
	uint64		muiSRNextEdge;		//The tick of the internal clock's next edge.
	bool		mbSRClock;			//The internal clock: CB1 when it is the VIA's.
	bool		mbSRData;			//The last bit shifted out: CB2 when shifting out.

	bool		mbCA2Handshake;		//The handshake's level of CA2 and CB2.
	bool		mbCB2Handshake;
	uint64		muiCA2PulseEnd;		//The tick a pulse output ends.
	uint64		muiCB2PulseEnd;
	uint64		muiCA2Pulses;
	uint64		muiCB2Pulses;

	uint8		muiHostA;			//What the host drives on the ports' pins in its masks.
	uint8		muiHostAMask;
	uint8		muiHostB;
	uint8		muiHostBMask;
	uint8		muiHeldA;			//The pins' bus holders.
	uint8		muiHeldB;
	bool		mbHostCA1;
	bool		mbHostCA2;
	bool		mbHostCB1;
	bool		mbHostCB2;

	uint64		muiTick;			//Caught up to.
	bool		mbFrozen;

public:
	void	Init(void);
	void	Kill(void);
	void	Reset(void);
	void	SetFrozen(bool bFrozen);

	uint8	Read(uint8 uiRegister);
	void	Write(uint8 uiRegister, uint8 uiValue);
	void	CatchUp(uint64 uiTick);
	uint64	NextEvent(void);
	bool	IsInterrupting(void);

	void	HostDrivePortA(uint8 uiLevels, uint8 uiMask);
	void	HostDrivePortB(uint8 uiLevels, uint8 uiMask);
	void	HostSetCA1(bool bHigh);
	void	HostSetCA2(bool bHigh);
	void	HostSetCB1(bool bHigh);
	void	HostSetCB2(bool bHigh);
	uint8	GetPortA(void);
	uint8	GetPortB(void);
	bool	GetCA2(void);
	bool	GetCB1(void);
	bool	GetCB2(void);
	uint64	NumCA2Pulses(void);
	uint64	NumCB2Pulses(void);
	uint16	GetT1Counter(void);
	uint16	GetT2Counter(void);
	uint64	GetTick(void);

protected:
	uint8	ReadPortA(void);
	uint8	ReadPortB(void);
	void	AccessPortA(void);
	void	UpdateHeld(void);
	void	PortBChanged(uint8 uiOldPins);

	uint8	CA2Mode(void);
	uint8	CB2Mode(void);
	uint8	SRMode(void);
	bool	IsCB1Output(void);
	bool	IsCB2Output(void);
	bool	IsT1Continuous(void);
	bool	IsT2Timed(void);

	uint16	T1Value(uint64 uiTick);
	uint64	T1FirstTimeout(void);
	uint64	T1Period(void);
	uint64	T1Timeouts(uint64 uiTick);
	uint64	T1NextTimeout(void);
	void	RebaseT1(void);
	uint16	T2Value(uint64 uiTick);
	void	RebaseT2(void);

	void	StartShift(void);
	uint64	SRHalfPeriod(void);
	void	SREdge(void);
	void	ShiftOut(void);
	void	ShiftIn(void);
	uint64	SRDoneTick(void);
	void	CatchUpShift(uint64 uiTick);
};


//The HCP65 VIA Board (VIA Board, VIA Control and VIA Breakout sheets): up to eight W65C22S VIAs, named by their ports:
//AB, CD, EF, GH, XJ, KL, MN and OP.  Two are on each VIA Breakout, and the breakouts are optional: breakout 0 has AB and
//CD, 1 EF and GH, 2 XJ and KL, 3 MN and OP.  A 32KB device: the device ROM below $400, then VIA Control's decode of A7-A0
//(A8 and A9 aren't decoded, so it repeats every $100).  With A7 clear A6-A4 pick the VIA and A3-A0 are its RS3-RS0.  At
//$480 a 74LCX540 reads the VIAs' IRQs inverted, D0 AB's to D7 OP's, 1 where it is interrupting; the rest of A7 set
//decodes nothing.  A VIA that isn't fitted reads $FF (the device bus is pulled high) and its IRQ is pulled up (0 at $480).
//
//The VIAs' PHI2 is the device clock (the MPU's oscillator) halved by VIA Control.  The device clock's holds (VIA Control
//asks for them to line accesses up with PHI2) aren't emulated: here PHI2 is half the CPU's clock rate.
//
//The board's interrupt is the VIAs' and'ed: any interrupting pulls it.  Held in reset, the VIAs are in reset.  Writes to
//$480 are ignored: on the V0 board the 74LCX540 drives the data bus against them, so nothing may write it.
//
//LoadXML: no <Breakout> tags is all four breakouts; otherwise <Breakout Socket="n"/> (0 to 3) for each one fitted.
class CHCP65VIABoard : public CHCP65Device
{
protected:
	CDeviceROM	mcROM;
	CHCP65VIA	macVIAs[HCP65_VIA_NUM_VIAS];
	bool		mabBreakouts[HCP65_VIA_NUM_BREAKOUTS];

public:
	void		Init(void);
	void		Kill(void) override;
	bool		LoadXML(CMarkupTag* pcTag, const char* szDirectory, CChars* pszError) override;
	void		SaveXML(CXMLWriter* pcWriter) override;

	uint8		Read(uint32 uiOffset) override;
	void		Write(uint32 uiOffset, uint8 uiValue) override;
	void		Reset(void) override;
	void		SetHeldInReset(bool bHeld) override;
	void		CatchUp(uint64 uiCycle) override;
	uint64		NextEvent(void) override;
	bool		IsInterrupting(void) override;

	void		SetBreakout(int iBreakout, bool bFitted);
	bool		HasBreakout(int iBreakout);
	int			NumVIAs(void);
	CHCP65VIA*	GetVIA(int iVIA);
	uint8		ReadInterrupts(void);

	static const char*	GetVIAName(int iVIA);

protected:
	uint64		PHI2Rate(void);
};


#endif // __HCP65_VIA_H__

