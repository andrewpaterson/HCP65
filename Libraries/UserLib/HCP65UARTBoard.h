#ifndef __HCP65_UART_BOARD_H__
#define __HCP65_UART_BOARD_H__


//The HCP65 UART Board (HCP65Lib/HCP65UARTBoard.h has the board) through its driver, \OS\Drivers\UART.DRV: open a port
//with KernalDeviceOpen(UART_BOARD_MANUFACTURER, UART_BOARD_NAME, index, unit, &handle) (HCP65User.h).  Two TL16C550D
//UARTs, each with its own 1.8432MHz oscillator.  Each port is opened on its own: two programs can have one each.
//
//A port opens at 115200 baud, 8 data bits, no parity, 1 stop bit, no flow control, with DTR and RTS on and nothing
//buffered.  KernalDeviceWrite queues bytes to send (the offset is ignored), sleeping while the driver's buffer is full;
//it gives up after UART_BOARD_WRITE_TICKS without room, KERNAL_DEVICE_TIMEOUT, having queued what *puiDone says.
//KernalDeviceRead takes what has arrived, up to the length, without waiting: *puiDone may be 0.  KernalDeviceWait waits
//for bytes to arrive or for room to send.  Closing a port waits up to UART_BOARD_CLOSE_TICKS for what is queued to go.
#define UART_BOARD_MANUFACTURER			"HCP65"
#define UART_BOARD_NAME					"UART Board"

//Units.
#define UART_BOARD_TTL					0		//UART1, the TTL 5V module: SIN, SOUT, RTS and CTS only.
#define UART_BOARD_RS232				1		//UART2, the RS232 module (DE-9): TX, RX, RTS, CTS, DTR, DSR and DCD.

#define UART_BOARD_BUFFER_SIZE			255		//Bytes the driver buffers each way, per port.
#define UART_BOARD_WRITE_TICKS			256		//A second (KERNAL_TICKS_PER_SECOND).
#define UART_BOARD_CLOSE_TICKS			256

//The baud rate is 115200 / divisor.
#define UART_BOARD_MAX_BAUD				115200
#define UART_BOARD_DIVISOR(baud)		(UART_BOARD_MAX_BAUD / (baud))

//KernalDeviceControl codes.  The SETs return KERNAL_DEVICE_BAD_ARGUMENT for a value they don't take.
#define UART_BOARD_SET_DIVISOR			0		//1 to 65535.
#define UART_BOARD_SET_FORMAT			1		//UART_FORMAT_*.
#define UART_BOARD_SET_FLOW				2		//UART_FLOW_*.
#define UART_BOARD_SET_LINES			3		//UART_LINE_DTR and UART_LINE_RTS: which outputs are on.
#define UART_BOARD_GET_LINES			4		//The inputs that are on: UART_LINE_CTS, _DSR and _DCD (those the module has).
#define UART_BOARD_GET_ERRORS			5		//UART_ERROR_* since the last GET_ERRORS (or the open), then cleared.
#define UART_BOARD_SET_BREAK			6		//1 holds the line in break; 0 lets it go.
#define UART_BOARD_FLUSH				7		//UART_FLUSH_*: drops what is buffered.
#define UART_BOARD_GET_RX_COUNT			8		//Bytes waiting to be read.
#define UART_BOARD_GET_TX_COUNT			9		//Bytes queued in the driver, plus 1 while the UART is still sending: 0 once all have gone.
#define UART_BOARD_SET_LOOPBACK			10		//1: the UART's output comes back as its input, and its outputs to the lines are off.

//SET_FORMAT: one of each, or'd (the UART's Line Control bits).
#define UART_FORMAT_DATA_5				0x00
#define UART_FORMAT_DATA_6				0x01
#define UART_FORMAT_DATA_7				0x02
#define UART_FORMAT_DATA_8				0x03
#define UART_FORMAT_STOP_1				0x00
#define UART_FORMAT_STOP_2				0x04	//1.5 with 5 data bits.
#define UART_FORMAT_PARITY_NONE			0x00
#define UART_FORMAT_PARITY_ODD			0x08
#define UART_FORMAT_PARITY_EVEN			0x18
#define UART_FORMAT_PARITY_MARK			0x28
#define UART_FORMAT_PARITY_SPACE		0x38
#define UART_FORMAT_8N1					(UART_FORMAT_DATA_8 | UART_FORMAT_STOP_1 | UART_FORMAT_PARITY_NONE)

//SET_FLOW.
#define UART_FLOW_NONE					0
#define UART_FLOW_RTS_CTS				1		//The UART holds sending while CTS is off and turns RTS off while it is full.

//SET_LINES and GET_LINES.
#define UART_LINE_DTR					0x01
#define UART_LINE_RTS					0x02
#define UART_LINE_CTS					0x10
#define UART_LINE_DSR					0x20
#define UART_LINE_DCD					0x80

//GET_ERRORS.
#define UART_ERROR_OVERRUN				0x02	//Bytes were lost: the driver's buffer and the UART's FIFO were full.
#define UART_ERROR_PARITY				0x04
#define UART_ERROR_FRAMING				0x08
#define UART_ERROR_BREAK				0x10

//FLUSH.
#define UART_FLUSH_RX					0x01
#define UART_FLUSH_TX					0x02

//KernalDeviceWait's value: what to wait for, or'd with a count from 1 to UART_BOARD_BUFFER_SIZE.
#define UART_WAIT_RX					0x00000		//That many bytes have arrived.
#define UART_WAIT_TX_SPACE				0x10000		//There is room to queue that many.
#define UART_WAIT_TX_EMPTY				0x20000		//Everything queued has been sent (no count).
#define UART_WAIT_COUNT_MASK			0x0FFFF


#endif // __HCP65_UART_BOARD_H__
