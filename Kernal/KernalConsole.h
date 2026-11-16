#ifndef __KERNAL_CONSOLE_H__
#define __KERNAL_CONSOLE_H__
#include "BaseLib/PrimitiveTypes.h"
#include "KernalDriver.h"
#include "KernalKeyboard.h"


#define KERNAL_CONSOLE_OWNER			1		//The process ID the console opens its boards' units as: no process has it.
#define KERNAL_CONSOLE_BUFFER_SIZE		64		//Bytes the console moves at a time.
#define KERNAL_CONSOLE_UART_COLUMNS		80		//What a terminal on the UART is taken to be.
#define KERNAL_CONSOLE_UART_ROWS		24
#define KERNAL_CONSOLE_FONT_FILE		"\\OS\\Fonts\\CONSOLE.FNT"	//256 8x16 glyphs, Latin-1 (HCP65/Fonts).
#define KERNAL_CONSOLE_FONT_SIZE		4096
#define KERNAL_CONSOLE_TYPED_SIZE		32		//Characters typed and not yet read.


//The console: what every process's standard handles reach (KERNAL_STANDARD_INPUT and KERNAL_STANDARD_OUTPUT in
//HCP65User.h), through the device calls.  Its output is the Video Board's text (in 640x480, 80 by 30, with the font
//KERNAL_CONSOLE_FONT_FILE) or else the UART Board's TTL port; its input the PS2 Board's keyboard in port 1, or else
//that port.  It takes the boards' units when a process first uses it (so a program that opens one itself can, if it
//does so first) and keeps them, as KERNAL_CONSOLE_OWNER.  Without a board for one it is quiet: what is written is
//dropped, and nothing is read.
//
//Output is text.  '\n' starts the next line (a terminal on the UART is sent "\r\n"), '\r' goes back to the line's
//start, '\b' back a character, '\t' on to the next multiple of 8 and '\f' clears the screen.  On the Video Board a
//line that reaches the right edge goes on on the next, and the text scrolls up from the bottom; the cursor blinks where
//the next character goes.  Input is bytes as they come: the UART's as a terminal sends them (Enter is '\r'), the
//keyboard's as the characters a terminal would send (CKernalKeyboard).  Nothing is echoed: a program that wants what
//is typed shown writes it.
class CKernalConsole
{
protected:
	CKernalDriver*	mpcOutput;			//NULL until taken.
	uint16			muiOutputUnit;
	bool			mbVideo;			//The output is the Video Board's text.
	CKernalDriver*	mpcInput;
	uint16			muiInputUnit;
	bool			mbKeyboard;			//The input is the PS2 Board's keyboard.
	uint16			muiColumn;			//The Video Board's cursor.
	uint16			muiRow;
	uint16			muiColumns;
	uint16			muiRows;
	CKernalKeyboard	mcKeyboard;
	bool			mbCapsLock;			//What the keyboard's LED shows.
	char			macTyped[KERNAL_CONSOLE_TYPED_SIZE];
	uint16			muiTyped;
	uint8			mauiBuffer[KERNAL_CONSOLE_BUFFER_SIZE * 2];

public:
	void	Init(void);

	uint16	Read(uint32 uiUserAddress, uint32 uiLength, uint32* puiDone);
	uint16	Write(uint32 uiUserAddress, uint32 uiLength, uint32* puiDone);
	uint16	Control(uint16 uiCode, uint32 uiValue, uint32* puiResult);
	uint16	Wait(uint32 uiValue, uint16 uiTicks);

protected:
	void	Take(void);
	bool	TakeVideo(void);
	bool	TakeKeyboard(void);
	uint16	Send(const uint8* puiBytes, uint32 uiLength);
	uint16	SendText(const char* szText);
	uint16	VideoControl(uint16 uiCode, uint32 uiValue, uint32* puiResult);
	uint16	VideoText(const uint8* puiChars, uint32 uiLength);
	void	VideoNewLine(void);
	void	VideoWrite(const uint8* puiBytes, uint32 uiLength);
	void	VideoCursor(void);
	uint16	VideoMode(uint32 uiMode);
	void	ReadKeyboard(void);
};


extern CKernalConsole	gcKernalConsole;


#endif // __KERNAL_CONSOLE_H__
