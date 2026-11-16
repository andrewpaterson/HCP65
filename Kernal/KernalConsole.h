#ifndef __KERNAL_CONSOLE_H__
#define __KERNAL_CONSOLE_H__
#include "BaseLib/PrimitiveTypes.h"
#include "KernalDriver.h"


#define KERNAL_CONSOLE_OWNER			1		//The process ID the console opens its boards' units as: no process has it.
#define KERNAL_CONSOLE_BUFFER_SIZE		64		//Bytes the console moves at a time.
#define KERNAL_CONSOLE_UART_COLUMNS		80		//What a terminal on the UART is taken to be.
#define KERNAL_CONSOLE_UART_ROWS		24


//The console: what every process's standard handles reach (KERNAL_STANDARD_INPUT and KERNAL_STANDARD_OUTPUT in
//HCP65User.h), through the device calls.  Its output is the Video Board's text, or else the UART Board's TTL port; its
//input the PS2 Board's keyboard, or else that port.  It takes the boards' units when a process first uses it (so a
//program that opens the UART Board's ports itself can, if it does so first) and keeps them, as KERNAL_CONSOLE_OWNER.
//Without a board for one it is quiet: what is written is dropped, and nothing is read.
//
//Output is text: '\n' starts the next line (a terminal on the UART is sent "\r\n"), '\r' goes back to the line's start,
//'\b' back a character and '\t' on to the next multiple of 8.  Input is bytes as they come: the UART's as a terminal
//sends them (Enter is '\r'), the keyboard's as characters (HCP65User.h's KERNAL_KEY_*).  Nothing is echoed: a program
//that wants what is typed shown writes it.
class CKernalConsole
{
protected:
	CKernalDriver*	mpcOutput;			//NULL until taken.
	uint16			muiOutputUnit;
	CKernalDriver*	mpcInput;
	uint16			muiInputUnit;
	uint8			mauiBuffer[KERNAL_CONSOLE_BUFFER_SIZE * 2];

public:
	void	Init(void);

	uint16	Read(uint32 uiUserAddress, uint32 uiLength, uint32* puiDone);
	uint16	Write(uint32 uiUserAddress, uint32 uiLength, uint32* puiDone);
	uint16	Control(uint16 uiCode, uint32 uiValue, uint32* puiResult);
	uint16	Wait(uint32 uiValue, uint16 uiTicks);

protected:
	void	Take(void);
	uint16	Send(const uint8* puiBytes, uint32 uiLength);
	uint16	SendText(const char* szText);
};


extern CKernalConsole	gcKernalConsole;


#endif // __KERNAL_CONSOLE_H__
