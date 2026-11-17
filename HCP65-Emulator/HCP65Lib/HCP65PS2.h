#ifndef __HCP65_PS2_H__
#define __HCP65_PS2_H__
#include "BaseLib/ArrayUint8.h"
#include "HCP65Device.h"


#define HCP65_PS2_BOARD_NAME			"PS2 Board"
#define HCP65_PS2_DRIVER				"PS2.DRV"
#define HCP65_PS2_REGISTERS				0x400
#define HCP65_PS2_REGISTERS_END			0x800
#define HCP65_PS2_DECODE				0x1F		//A0-A4: Stage B is enabled by A3 and A4 low.  A5-A9 aren't decoded.
#define HCP65_PS2_NUM_PORTS				2
#define HCP65_PS2_FIFO_SIZE				16

//Registers, from $400.  They repeat every 32 bytes; $405-$41F decode nothing.
#define HCP65_PS2_FLAGS					0x00	//R/W: Flags & Interrupt.
#define HCP65_PS2_FIFO_1				0x01	//R: port 1's next byte.  W: clears port 1's Packet and Code Waiting.
#define HCP65_PS2_FIFO_2				0x02	//R: port 2's.  W: port 2's.
#define HCP65_PS2_DIRECT				0x03	//W: the GPIO, D0-D3 levels and D4-D7 output enables.  R: D0-D3 the lines.
#define HCP65_PS2_STATE					0x04	//R.

#define HCP65_PS2_FLAG_ENABLE			0x40
#define HCP65_PS2_FLAG_INTERRUPT		0x80	//W: interrupt enable.  R: interrupting.

#define HCP65_PS2_DIRECT_DATA_1			0x01	//GPIO0.
#define HCP65_PS2_DIRECT_CLOCK_1		0x02	//GPIO1.
#define HCP65_PS2_DIRECT_DATA_2			0x04	//GPIO2.
#define HCP65_PS2_DIRECT_CLOCK_2		0x08	//GPIO3.
#define HCP65_PS2_DIRECT_DRIVE			4		//A line's output enable is its level's bit shifted by this.

#define HCP65_PS2_STATE_NO_PACKET_1		0x01	//S0, ~Packet Waiting: 0 while port 1 has a packet waiting.
#define HCP65_PS2_STATE_NO_CODE_1		0x02	//S1, ~Code Waiting.
#define HCP65_PS2_STATE_READY_1			0x04	//S2, the FIFO's Data-Out Ready: it has a byte.
#define HCP65_PS2_STATE_PORT_SHIFT		4		//Port 2's are S4-S6.  S3 and S7 are tied low.

//The port's time is in microseconds.
#define HCP65_PS2_TICK_RATE				1000000
#define HCP65_PS2_PACKET_QUIET			630		//Clock high this long ends a packet (R3 4.7K, C3 0.1uF, the VHCT14's VN about 1V).
#define HCP65_PS2_CODE_QUIET			6700	//No packet this long after one ends a code (R1 || R2 5K, C4 1uF).
#define HCP65_PS2_BIT_TICKS				80		//The far end's clock: 12.5kHz.
#define HCP65_PS2_SETUP_TICKS			20		//Data to clock low.
#define HCP65_PS2_HALF_TICKS			40		//Clock low, and clock high.
#define HCP65_PS2_IDLE_TICKS			50		//Both lines high this long before the far end sends.
#define HCP65_PS2_BYTE_GAP				1000	//Between the far end's frames: longer than a packet's quiet.
#define HCP65_PS2_RTS_TICKS				100		//From the computer's request to send to the far end's first clock.


class CHCP65PS2Port;


//What is plugged into a port (SetDevice), when it is more than the host's calls: a keyboard.  Received is each byte the
//computer sends it, at the tick it arrives; CatchUp is called as the port is caught up, before it moves on, so the
//device can send (HostSend) what it has been given from another thread.  Both are on the thread the HCP65 runs on.
class CHCP65PS2Device
{
public:
	virtual void	Received(CHCP65PS2Port* pcPort, uint8 uiByte) = 0;
	virtual void	CatchUp(CHCP65PS2Port* pcPort) = 0;
};


enum EHCP65PS2Device
{
	HPD_Idle,
	HPD_Sending,
	HPD_Receiving,
};


//One of the board's ports (a PS2 Timing and a PS2 FIFO), and what is plugged into it (the host).
//
//The Clock and Data lines are pulled up and wired-and: low while the board's GPIO or the far end pulls them low.  (The
//GPIO's 74AHC125s drive high too; here a high is taken as letting go.)
//
//The board's receiver, as it is meant to work (the PS2 FIFO sheet has faults: see its notes): every fall of Clock shifts
//Data into a 16 bit shift register, whoever makes it.  Clock low starts a packet (Packet Pulse); Clock high for
//HCP65_PS2_PACKET_QUIET ends it, which sets Packet Waiting and puts the byte the last eleven bits make (start, eight data
//bits, parity and stop) into the 16 byte FIFO, if it isn't full; parity and the start and stop bits aren't looked at.
//No packet for HCP65_PS2_CODE_QUIET after one ends sets Code Waiting: a code is a key's bytes (E0 F0 74).  Reading the
//FIFO takes its first byte; empty, it reads its last again.  Reset clears the shift register, the flags and the FIFO, and
//while it is held nothing is shifted, set or kept.
//
//The far end is a PS/2 device.  It sends what HostSend was given, a frame at a time at 12.5kHz, with
//HCP65_PS2_BYTE_GAP between frames, once both lines have been high for HCP65_PS2_IDLE_TICKS.  If Clock is held low
//under it (the computer inhibiting it) it gives the frame up and sends it again later.  When the computer requests to
//send (it lets Clock go with Data low) the far end clocks in eight data bits, parity and stop, and acknowledges; the
//bytes that arrive with odd parity and a stop bit are HostReceive's (or the device's, SetDevice), the rest counted
//(NumErrors).  Unplugged (SetPlugged), there's no far end: nothing is sent and nothing clocks the computer's bytes in.
//The host's calls happen at the tick the port has been caught up to.
class CHCP65PS2Port
{
protected:
	bool			mbBoardClockLow;	//The GPIO pulls the line low.
	bool			mbBoardDataLow;
	bool			mbDeviceClockLow;	//The far end does.
	bool			mbDeviceDataLow;
	uint64			muiTick;			//Caught up to.

	bool			mbHeld;				//In reset.
	uint16			muiShift;			//The last bit in at D0.
	bool			mbPacketPulse;
	uint64			muiPacketEnd;		//When the packet ends, HCP65_NEVER while Clock is low or there's none.
	uint64			muiCodeEnd;
	bool			mbPacketWaiting;
	bool			mbCodeWaiting;
	uint8			mauiFIFO[HCP65_PS2_FIFO_SIZE];
	uint8			muiFIFOFirst;
	uint8			muiFIFOCount;
	uint8			muiFIFOOutput;		//The byte last read.

	EHCP65PS2Device	meDevice;
	uint8			muiDeviceBit;		//Sending, the frame's bit (0 to 10); receiving, the clock (1 to 11).
	uint8			muiDevicePhase;
	uint16			muiDeviceFrame;
	uint64			muiDeviceEvent;
	uint64			muiIdleSince;		//Both lines have been high since.
	uint64			muiFrameEnd;		//The far end's last frame ended.
	CArrayUint8		macHostSend;		//What the far end is still to send, from muiHostSendIndex.
	size			muiHostSendIndex;
	CArrayUint8		macHostReceived;
	uint64			muiAborts;
	uint64			muiErrors;
	CHCP65PS2Device*	mpcDevice;
	bool			mbPlugged;

public:
	void	Init(void);
	void	Kill(void);
	void	Reset(void);
	void	SetHeld(bool bHeld);
	void	SetBoardDrive(bool bClockLow, bool bDataLow);
	void	SetDevice(CHCP65PS2Device* pcDevice);
	void	SetPlugged(bool bPlugged);
	bool	IsPlugged(void) const;
	CHCP65PS2Device*	GetDevice(void) const;

	void	CatchUp(uint64 uiTick);
	uint64	NextEvent(void);

	uint8	ReadFIFO(void);
	void	ClearWaiting(void);
	bool	IsPacketWaiting(void) const;
	bool	IsCodeWaiting(void) const;
	bool	IsOutputReady(void) const;
	bool	IsClockHigh(void) const;
	bool	IsDataHigh(void) const;

	void	HostSend(const uint8* puiData, size uiLength);
	size	HostReceive(uint8* puiDest, size uiMaxLength);
	size	NumHostToSend(void) const;
	size	NumHostReceived(void) const;
	uint64	NumAborts(void) const;
	uint64	NumErrors(void) const;
	uint64	GetTick(void) const;

protected:
	void	SetDeviceDrive(bool bClockLow, bool bDataLow);
	void	LinesChanged(bool bOldClock, bool bOldData);
	void	EndPacket(void);
	void	EndCode(void);
	void	DeviceStep(void);
	void	StepSending(void);
	void	StepReceiving(void);
	void	ScheduleSend(void);
	void	StartFrame(void);
	void	AbortFrame(void);
	void	FinishReceiving(void);
	uint8	LatchedByte(void);
};


//The HCP65 PS2 Board (PS2 Board, PS2 Timing and PS2 FIFO sheets): two PS/2 ports that take what a keyboard or mouse
//sends, a byte at a time into a FIFO each.  A 32KB device: the device ROM below $400, then a Device Decode Stage B of
//five registers.  Flags (a Flags & Interrupt board): D0-D5 are kept, D6 enables the board (and the GPIO's drivers) and
//D7 its interrupt, which is on while either port has a packet waiting; reset turns Enable off.  Writing a port's FIFO
//register clears its Packet and Code Waiting.  Direct is a GPIO 4bit on the ports' lines, so a driver can inhibit a
//device or send it a byte by hand.  State reads both ports' flags and whether their FIFOs have a byte.
class CHCP65PS2Board : public CHCP65Device
{
protected:
	CDeviceROM		mcROM;
	CHCP65PS2Port	macPorts[HCP65_PS2_NUM_PORTS];
	uint8			muiFlags;		//D0-D5 and the interrupt enable as written.
	bool			mbEnable;
	uint8			muiGPIO;

public:
	void			Init(void);
	void			Kill(void) override;

	uint8			Read(uint32 uiOffset) override;
	void			Write(uint32 uiOffset, uint8 uiValue) override;
	void			Reset(void) override;
	void			SetHeldInReset(bool bHeld) override;
	void			CatchUp(uint64 uiCycle) override;
	uint64			NextEvent(void) override;
	bool			IsInterrupting(void) override;

	CHCP65PS2Port*	GetPort(int iPort);

protected:
	uint8			ReadState(void);
	uint8			ReadLines(void);
	void			UpdateDrive(void);
};


#endif // __HCP65_PS2_H__

