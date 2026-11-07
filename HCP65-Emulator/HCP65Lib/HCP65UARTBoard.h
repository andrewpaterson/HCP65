#ifndef __HCP65_UART_BOARD_H__
#define __HCP65_UART_BOARD_H__
#include "BaseLib/ArrayUint8.h"
#include "HCP65Device.h"


#define HCP65_UART_NAME				"UART Board"
#define HCP65_UART_DRIVER			"UART.DRV"
#define HCP65_UART_REGISTERS		0x400		//$400 to $7FF: UART2's eight registers then UART1's, repeating every 16 bytes.
#define HCP65_UART_UART1			0x08		//A3 high: UART1 (the TTL module); low: UART2 (the RS232 module).
#define HCP65_UART_CLOCK_RATE		1843200		//Each module's oscillator, in Hz: a divisor of 1 is 115200 baud.
#define HCP65_UART_FIFO_SIZE		16

#define HCP65_UART_RBR				0			//Read: Receiver Buffer.  Write: Transmitter Holding.  DLAB: Divisor Latch low.
#define HCP65_UART_IER				1			//DLAB: Divisor Latch high.
#define HCP65_UART_IIR				2			//Read: Interrupt Identification.  Write: FIFO Control.
#define HCP65_UART_LCR				3
#define HCP65_UART_MCR				4
#define HCP65_UART_LSR				5
#define HCP65_UART_MSR				6
#define HCP65_UART_SCR				7

#define HCP65_UART_IER_RECEIVED		0x01
#define HCP65_UART_IER_TRANSMIT		0x02
#define HCP65_UART_IER_LINE_STATUS	0x04
#define HCP65_UART_IER_MODEM_STATUS	0x08

#define HCP65_UART_IIR_NONE			0x01
#define HCP65_UART_IIR_LINE_STATUS	0x06
#define HCP65_UART_IIR_RECEIVED		0x04
#define HCP65_UART_IIR_TIMEOUT		0x0C
#define HCP65_UART_IIR_TRANSMIT		0x02
#define HCP65_UART_IIR_MODEM_STATUS	0x00
#define HCP65_UART_IIR_FIFOS		0xC0		//Set while the FIFOs are enabled.

#define HCP65_UART_FCR_ENABLE		0x01
#define HCP65_UART_FCR_RX_RESET		0x02
#define HCP65_UART_FCR_TX_RESET		0x04
#define HCP65_UART_FCR_TRIGGER		0xC0		//1, 4, 8 or 14 bytes.

#define HCP65_UART_LCR_DATA			0x03		//5 to 8 bits.
#define HCP65_UART_LCR_STOP			0x04
#define HCP65_UART_LCR_PARITY		0x08
#define HCP65_UART_LCR_BREAK		0x40
#define HCP65_UART_LCR_DLAB			0x80

#define HCP65_UART_MCR_DTR			0x01
#define HCP65_UART_MCR_RTS			0x02
#define HCP65_UART_MCR_OUT1			0x04
#define HCP65_UART_MCR_OUT2			0x08
#define HCP65_UART_MCR_LOOP			0x10
#define HCP65_UART_MCR_AUTO_FLOW	0x20

#define HCP65_UART_LSR_DATA_READY	0x01
#define HCP65_UART_LSR_OVERRUN		0x02
#define HCP65_UART_LSR_ERRORS		0x1E
#define HCP65_UART_LSR_THR_EMPTY	0x20
#define HCP65_UART_LSR_TX_EMPTY		0x40

#define HCP65_UART_MSR_DELTAS		0x0F
#define HCP65_UART_MSR_CTS			0x10
#define HCP65_UART_MSR_DSR			0x20
#define HCP65_UART_MSR_RI			0x40
#define HCP65_UART_MSR_DCD			0x80


enum EHCP65UARTModule
{
	HUM_TTL,		//SIN, SOUT, RTS and CTS reach the connector; DSR and DCD are unconnected (read off here), DTR goes nowhere.
	HUM_RS232		//TX, RX, RTS, CTS, DTR, DSR and DCD.  Both modules tie RI on.
};


//One TL16C550D on its module.  Its time is in its oscillator's ticks.  The far end of its line is the host: it sends
//what HostSend was given, one character at a time at the UART's own baud rate and format, whenever the UART's RTS is
//on (the far end obeys RTS), and takes what the UART sends (HostReceive).  The host sets the far end's RTS and DTR,
//which are the UART's CTS, DSR and DCD (HostSetCTS ...).  Only a character's data bits get across.  Auto-RTS turns
//RTS off while the receive FIFO holds its trigger level or more.  A divisor of 0 is taken as 65536.  Parity, framing
//and break errors never happen.
class CHCP65UART
{
protected:
	EHCP65UARTModule	meModule;

	uint8		muiIER;
	uint8		muiLCR;
	uint8		muiMCR;
	uint8		muiSCR;
	uint8		muiFCR;					//Enable and trigger as last written.
	uint16		muiDivisor;
	uint8		muiLineErrors;			//LSR's error bits, until LSR is read.
	uint8		muiModemDeltas;			//MSR's D0-D3, until MSR is read.
	uint8		muiModemInputs;			//MSR's D4-D7 as last seen, for the deltas.
	bool		mbTransmitInterrupt;	//THRE pending, until IIR reports it or THR is written.

	uint8		mauiRxFIFO[HCP65_UART_FIFO_SIZE];
	uint8		muiRxFirst;
	uint8		muiRxCount;
	uint8		mauiTxFIFO[HCP65_UART_FIFO_SIZE];
	uint8		muiTxFirst;
	uint8		muiTxCount;

	bool		mbShifting;				//A character is going out of the transmit shift register.
	uint8		muiShiftByte;
	uint64		muiShiftDone;
	bool		mbReceiving;			//A character from the far end is coming in.
	uint8		muiReceiveByte;
	uint64		muiReceiveDone;
	uint64		muiRxActivity;			//When a character last arrived or RBR was read: the timeout counts from it.

	uint64		muiTick;				//Caught up to.

	CArrayUint8	macHostSend;			//What the far end is still to send, from muiHostSendIndex.
	size		muiHostSendIndex;
	CArrayUint8	macHostReceived;		//What the UART sent.
	bool		mbHostCTS;
	bool		mbHostDSR;
	bool		mbHostDCD;
	uint64		muiOverruns;

public:
	void	Init(EHCP65UARTModule eModule);
	void	Kill(void);

	void	Reset(void);
	uint8	Read(uint8 uiRegister);
	void	Write(uint8 uiRegister, uint8 uiValue);

	void	CatchUp(uint64 uiTick);
	uint64	NextEvent(void);
	bool	IsInterrupting(void);

	void	HostSend(const uint8* puiData, size uiLength);
	size	HostReceive(uint8* puiDest, size uiMaxLength);
	size	NumHostToSend(void);
	size	NumHostReceived(void);
	void	HostSetCTS(bool bOn);
	void	HostSetDSR(bool bOn);
	void	HostSetDCD(bool bOn);
	bool	IsRTSOn(void);
	bool	IsDTROn(void);
	uint32	GetBaudRate(void);
	uint8	GetLineControl(void);
	uint64	NumOverruns(void);

protected:
	uint8	Identify(void);
	uint8	ReadLineStatus(void);
	uint8	ReadModemStatus(void);
	uint8	ModemInputs(void);
	void	UpdateModemDeltas(void);
	uint8	FIFODepth(void);
	uint8	TriggerLevel(void);
	bool	IsLoopback(void);
	uint8	DataMask(void);
	bool	IsTimedOut(void);
	uint64	TimeoutTick(void);
	uint64	CharacterTicks(void);
	void	StartShifting(void);
	void	FinishShifting(void);
	void	StartReceiving(void);
	void	FinishReceiving(void);
	void	AddReceived(uint8 uiByte);
	void	ClearRx(void);
	void	ClearTx(void);
};


//The HCP65 UART Board (UART Board, UART Signals, TTL5V and RS232 sheets): two TL16C550D UARTs on modules, UART1 on
//the TTL 5V module and UART2 on the RS232 module, each with its own 1.8432MHz oscillator.  A 32KB device: the device
//ROM below $400, then the UARTs' registers (A3 picks the UART, A0-A2 the register; A4-A9 aren't decoded).  Its
//interrupt is the two UARTs' and'ed: either interrupting pulls it.  Held in reset, the UARTs are in master reset and
//their oscillators are stopped.
//
//This is the board as it should be: the V0 schematic has the device ROM's and device RAM's selects swapped at its
//Stage A (Kicad/Computer/UART/UART Board/UART Board Bugs.txt).
class CHCP65UARTBoard : public CHCP65Device
{
protected:
	CDeviceROM	mcROM;
	CHCP65UART	mcUART1;
	CHCP65UART	mcUART2;
	uint64		muiCycle;		//The CPU cycle it has been caught up to.

public:
	void	Init(void);
	void	Kill(void) override;

	uint8	Read(uint32 uiOffset) override;
	void	Write(uint32 uiOffset, uint8 uiValue) override;
	void	Reset(void) override;

	void	CatchUp(uint64 uiCycle) override;
	uint64	NextEvent(void) override;
	bool	IsInterrupting(void) override;

	CHCP65UART*	GetTTL(void);
	CHCP65UART*	GetRS232(void);

protected:
	uint64	UARTTick(uint64 uiCycle);
	uint64	CycleOfUARTTick(uint64 uiTick);
};


#endif // __HCP65_UART_BOARD_H__
