#ifndef __HCP65_PS2_BOARD_H__
#define __HCP65_PS2_BOARD_H__


//The HCP65 PS2 Board (HCP65Lib/HCP65PS2.h has the board) through its driver, \OS\Drivers\PS2.DRV: open a port with
//KernalDeviceOpen(PS2_BOARD_MANUFACTURER, PS2_BOARD_NAME, index, unit, &handle) (HCP65User.h).  Two PS/2 ports, for a
//keyboard or a mouse.  Each port is opened on its own: two programs can have one each.
//
//A port is a stream of the bytes its device sends, as it sends them: a keyboard's scan codes (set 2: 1C is A down,
//F0 1C A up), and its answers to commands (FA ...).  The driver doesn't interpret them.  KernalDeviceRead takes what
//has arrived, up to the length, without waiting (the offset is ignored): *puiDone may be 0.  KernalDeviceWait waits for
//bytes to arrive.  A port opens with nothing buffered; bytes that arrive while it is closed are kept until it is opened.
//
//KernalDeviceWrite sends bytes to the device, one at a time, each acknowledged by the device on the line: a command
//(FF reset, ED set LEDs ...) or a command's argument.  What the device answers (FA) arrives to be read, as any byte
//does: wait for it before sending a command's argument.  Write fails (KERNAL_DEVICE_FAILED, having sent what *puiDone
//says) if the device doesn't clock a byte in, or doesn't acknowledge it, in PS2_BOARD_SEND_TICKS: nothing is plugged
//in, say.  While a byte is being sent the device can't send, and anything it was sending is sent again afterwards.
#define PS2_BOARD_MANUFACTURER			"HCP65"
#define PS2_BOARD_NAME					"PS2 Board"

//Units.
#define PS2_BOARD_PORT_1				0
#define PS2_BOARD_PORT_2				1

#define PS2_BOARD_BUFFER_SIZE			255		//Bytes the driver buffers, per port.
#define PS2_BOARD_SEND_TICKS			8		//About 30ms (KERNAL_DEVICE_TICKS_PER_SECOND): a device clocks within 15ms.

//KernalDeviceControl codes.
#define PS2_BOARD_GET_RX_COUNT			0		//Bytes waiting to be read.
#define PS2_BOARD_FLUSH					1		//Drops the bytes waiting to be read.
#define PS2_BOARD_GET_OVERRUNS			2		//Bytes lost since the last GET_OVERRUNS (or the open) because the buffer was full, then cleared.

//KernalDeviceWait's value: how many bytes, 1 to PS2_BOARD_BUFFER_SIZE, to wait for.

//Keyboard commands and answers (set 2).
#define PS2_KEYBOARD_SET_LEDS			0xED	//Argument: PS2_KEYBOARD_LED_*.
#define PS2_KEYBOARD_ECHO				0xEE
#define PS2_KEYBOARD_IDENTIFY			0xF2
#define PS2_KEYBOARD_ENABLE				0xF4
#define PS2_KEYBOARD_DISABLE			0xF5
#define PS2_KEYBOARD_RESET				0xFF
#define PS2_KEYBOARD_ACK				0xFA
#define PS2_KEYBOARD_RESEND				0xFE
#define PS2_KEYBOARD_BAT_OK				0xAA	//After a reset (and at power on): its self test passed.
#define PS2_KEYBOARD_EXTENDED			0xE0	//Before an extended key's code.
#define PS2_KEYBOARD_BREAK				0xF0	//Before a key's code when it goes up.

#define PS2_KEYBOARD_LED_SCROLL			0x01
#define PS2_KEYBOARD_LED_NUM			0x02
#define PS2_KEYBOARD_LED_CAPS			0x04


#endif // __HCP65_PS2_BOARD_H__

