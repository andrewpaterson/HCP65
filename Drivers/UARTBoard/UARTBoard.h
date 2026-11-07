#ifndef __UART_BOARD_H__
#define __UART_BOARD_H__
#include "KernalDriver.h"
#include "UserLib/HCP65UARTBoard.h"


//A TL16C550D's registers (A0-A2), from its first.  UART2 (the RS232 module) is at $400 in the slot, UART1 (the TTL
//module) at $408; A4-A9 aren't decoded, so they repeat every 16 bytes.
#define UART_RBR					0		//Read: Receiver Buffer.
#define UART_THR					0		//Write: Transmitter Holding.
#define UART_DLL					0		//With LCR's DLAB: Divisor Latch low.
#define UART_IER					1
#define UART_DLM					1		//With LCR's DLAB: Divisor Latch high.
#define UART_IIR					2		//Read: Interrupt Identification.
#define UART_FCR					2		//Write: FIFO Control.
#define UART_LCR					3
#define UART_MCR					4
#define UART_LSR					5
#define UART_MSR					6
#define UART_SCR					7

#define UART1_REGISTERS				0x08	//From the slot's device registers ($400).
#define UART2_REGISTERS				0x00

#define UART_IER_RECEIVED			0x01	//Data available or the receive timeout.
#define UART_IER_TRANSMIT			0x02	//Transmitter Holding Register empty.
#define UART_IER_LINE_STATUS		0x04
#define UART_IER_MODEM_STATUS		0x08

#define UART_IIR_NONE				0x01	//No interrupt pending.
#define UART_IIR_ID					0x0E
#define UART_IIR_LINE_STATUS		0x06
#define UART_IIR_RECEIVED			0x04
#define UART_IIR_TIMEOUT			0x0C
#define UART_IIR_TRANSMIT			0x02
#define UART_IIR_MODEM_STATUS		0x00

#define UART_FCR_ENABLE				0x01
#define UART_FCR_RX_RESET			0x02
#define UART_FCR_TX_RESET			0x04
#define UART_FCR_TRIGGER_8			0x80	//The receive FIFO interrupts (and auto-RTS stops the sender) at 8 bytes.
#define UART_FIFO_SIZE				16

#define UART_LCR_FORMAT				0x3F	//Data bits, stop bits and parity (UART_FORMAT_*).
#define UART_LCR_BREAK				0x40
#define UART_LCR_DLAB				0x80

#define UART_MCR_DTR				0x01
#define UART_MCR_RTS				0x02
#define UART_MCR_LOOP				0x10
#define UART_MCR_AUTO_FLOW			0x20	//With RTS: auto-RTS and auto-CTS.

#define UART_LSR_DATA_READY			0x01
#define UART_LSR_ERRORS				0x1E	//Overrun, parity, framing and break: cleared by reading LSR.
#define UART_LSR_THR_EMPTY			0x20
#define UART_LSR_TX_EMPTY			0x40	//The transmitter's FIFO and shift register are both empty.

#define UART_TTL_LINES				UART_LINE_CTS									//The TTL module's DSR and DCD are unconnected.
#define UART_RS232_LINES			(UART_LINE_CTS | UART_LINE_DSR | UART_LINE_DCD)	//Its RI is tied on.

#define UART_RING_SIZE				256		//A ring holds one less (UART_BOARD_BUFFER_SIZE); its uint8 indexes wrap on their own.


//One of the board's UARTs.  Its received bytes and those to send are in rings in MPU RAM, which the interrupt fills
//and empties: UART_RING_SIZE bytes each, one kept empty, indexed by uint8s that wrap on their own.  The device calls
//run with I set, so the interrupt only runs while one waits (KernalWaitForInterrupt) and the rings need no locking.
class CKernalUARTPort
{
protected:
	volatile uint8*	mpuiRegisters;
	uint8			muiLines;			//The inputs the module has (UART_LINE_*).
	uint8			muiOwner;			//The process that has the port open, KERNAL_DRIVER_NO_OWNER if none.
	uint8			muiIER;				//What was last written to the registers that can't be read back the same.
	uint8			muiLCR;
	uint8			muiMCR;
	volatile uint8	muiErrors;			//UART_ERROR_* since they were last taken.
	volatile uint8	muiRxHead;
	volatile uint8	muiRxTail;
	volatile uint8	muiTxHead;
	volatile uint8	muiTxTail;
	uint8			mauiRx[UART_RING_SIZE];
	uint8			mauiTx[UART_RING_SIZE];

public:
	void	Init(volatile uint8* puiRegisters, uint8 uiLines);
	void	Kill(void);

	uint16	Open(uint8 uiProcess);
	void	Close(void);
	uint8	GetOwner(void);

	void	Interrupt(void);

	uint16	Read(uint32 uiUserAddress, uint32 uiLength, uint32* puiDone);
	uint16	Write(uint32 uiUserAddress, uint32 uiLength, uint32* puiDone);
	uint16	Control(uint16 uiCode, uint32 uiValue, uint32* puiResult);
	uint16	Wait(uint32 uiValue, uint16 uiTicks);

protected:
	void	Reset(void);
	void	SetDivisor(uint16 uiDivisor);
	void	SetIER(uint8 uiIER);
	void	SetLCR(uint8 uiLCR);
	void	SetMCR(uint8 uiMCR);
	void	Flush(uint8 uiWhich);
	uint8	ReadLineStatus(void);
	void	Receive(void);
	void	Transmit(void);
	uint8	NumReceived(void);
	uint8	NumQueued(void);
	bool	IsSent(void);
	bool	IsWaitOver(uint32 uiValue);
};


//The HCP65 UART Board's driver: its units are its two UARTs (UART_BOARD_TTL and UART_BOARD_RS232), each opened by its
//own process.  The board has no Flags & Interrupt board, so its interrupt is trusted: it only touches the UARTs and
//the rings.
class CKernalUARTBoard : public CKernalDriver
{
protected:
	CKernalUARTPort	mcTTL;
	CKernalUARTPort	mcRS232;

public:
	uint16	Init(SKernalDevice* psDevice, uint16 uiSlot) override;
	void	Kill(void) override;

	uint16	NumUnits(void) override;
	void	Interrupt(void) override;

	uint16	Open(uint16 uiUnit, uint8 uiProcess) override;
	void	Close(uint16 uiUnit, uint8 uiProcess) override;
	bool	IsOpen(void) override;
	uint16	Read(uint16 uiUnit, uint32 uiOffset, uint32 uiUserAddress, uint32 uiLength, uint32* puiDone) override;
	uint16	Write(uint16 uiUnit, uint32 uiOffset, uint32 uiUserAddress, uint32 uiLength, uint32* puiDone) override;
	uint16	Control(uint16 uiUnit, uint16 uiCode, uint32 uiValue, uint32* puiResult) override;
	uint16	Wait(uint16 uiUnit, uint32 uiValue, uint16 uiTicks) override;

protected:
	CKernalUARTPort*	GetPort(uint16 uiUnit);
	void				ClosePort(CKernalUARTPort* pcPort, uint8 uiProcess);
};


#define UART_BOARD_MAX_BOARDS		2		//UART boards it drives.


extern "C" CKernalDriver*	KernalCreateDriver(uint16 uiIndex);


#endif // __UART_BOARD_H__
