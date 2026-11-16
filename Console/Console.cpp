#include <string.h>
#include "HCP65User.h"
#include "HCP65UARTBoard.h"
#include "HCP65PS2Board.h"


//The console: what is typed on the PS/2 keyboard (the PS2 Board's port 1) is shown on the terminal at the far end of
//the UART Board's RS232 port (115200 8N1), with a prompt.  Enter starts a new line and Backspace takes back what was
//typed on it.  The keyboard is read as a US one, in scan code set 2; Caps Lock's LED follows it.  It runs until the
//HCP65 is reset.
#define CONSOLE_BANNER			"\fHCP65 console.  Type on the PS/2 keyboard.\r\n"
#define CONSOLE_PROMPT			"> "
#define CONSOLE_WAIT_TICKS		KERNAL_DEVICE_TICKS_PER_SECOND
#define CONSOLE_MAX_LINE		72
#define CONSOLE_NO_UART			100		//Exit codes, plus why.
#define CONSOLE_NO_KEYBOARD		200

#define CONSOLE_KEY_LEFT_SHIFT	0x12
#define CONSOLE_KEY_RIGHT_SHIFT	0x59
#define CONSOLE_KEY_CAPS_LOCK	0x58
#define CONSOLE_KEY_ENTER		0x5A
#define CONSOLE_KEY_BACKSPACE	0x66


//A key's set 2 make code and the characters it types, unshifted and shifted.
struct SConsoleKey
{
	uint8	uiCode;
	char	cNormal;
	char	cShifted;
};


static const SConsoleKey	gasConsoleKeys[] =
{
	{ 0x0E, '`', '~' }, { 0x16, '1', '!' }, { 0x1E, '2', '@' }, { 0x26, '3', '#' }, { 0x25, '4', '$' }, { 0x2E, '5', '%' },
	{ 0x36, '6', '^' }, { 0x3D, '7', '&' }, { 0x3E, '8', '*' }, { 0x46, '9', '(' }, { 0x45, '0', ')' }, { 0x4E, '-', '_' },
	{ 0x55, '=', '+' }, { 0x15, 'q', 'Q' }, { 0x1D, 'w', 'W' }, { 0x24, 'e', 'E' }, { 0x2D, 'r', 'R' }, { 0x2C, 't', 'T' },
	{ 0x35, 'y', 'Y' }, { 0x3C, 'u', 'U' }, { 0x43, 'i', 'I' }, { 0x44, 'o', 'O' }, { 0x4D, 'p', 'P' }, { 0x54, '[', '{' },
	{ 0x5B, ']', '}' }, { 0x5D, '\\', '|' }, { 0x1C, 'a', 'A' }, { 0x1B, 's', 'S' }, { 0x23, 'd', 'D' }, { 0x2B, 'f', 'F' },
	{ 0x34, 'g', 'G' }, { 0x33, 'h', 'H' }, { 0x3B, 'j', 'J' }, { 0x42, 'k', 'K' }, { 0x4B, 'l', 'L' }, { 0x4C, ';', ':' },
	{ 0x52, '\'', '"' }, { 0x1A, 'z', 'Z' }, { 0x22, 'x', 'X' }, { 0x21, 'c', 'C' }, { 0x2A, 'v', 'V' }, { 0x32, 'b', 'B' },
	{ 0x31, 'n', 'N' }, { 0x3A, 'm', 'M' }, { 0x41, ',', '<' }, { 0x49, '.', '>' }, { 0x4A, '/', '?' }, { 0x29, ' ', ' ' },
	{ 0x70, '0', '0' }, { 0x69, '1', '1' }, { 0x72, '2', '2' }, { 0x7A, '3', '3' }, { 0x6B, '4', '4' }, { 0x73, '5', '5' },
	{ 0x74, '6', '6' }, { 0x6C, '7', '7' }, { 0x75, '8', '8' }, { 0x7D, '9', '9' }, { 0x71, '.', '.' }, { 0x79, '+', '+' },
	{ 0x7B, '-', '-' }, { 0x7C, '*', '*' },
};


uint16	guiConsoleUART;
uint16	guiConsoleKeyboard;
bool	gbConsoleBreak;			//F0: the next code is a key going up.
bool	gbConsoleExtended;		//E0: the next code is an extended key's.
bool	gbConsoleLeftShift;
bool	gbConsoleRightShift;
bool	gbConsoleCapsLock;
uint16	guiConsoleLineLength;
uint8	gauiConsoleBytes[16];


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void ConsoleWrite(const char* szText)
{
	KernalDeviceWrite(guiConsoleUART, 0, szText, strlen(szText), NULL);
}


//////////////////////////////////////////////////////////////////////////
//
// The keyboard's LEDs: Caps Lock's.  ED and its argument go in one write:
// the driver waits for the line to be quiet before each byte, which gives
// the keyboard time to answer ED.  Its answers (FA) are dropped with the
// keys, in the order they come: reading one here could take a key that
// was typed before them.
//
//////////////////////////////////////////////////////////////////////////
void ConsoleSetLEDs(void)
{
	uint8	auiCommand[2];

	auiCommand[0] = PS2_KEYBOARD_SET_LEDS;
	auiCommand[1] = gbConsoleCapsLock ? PS2_KEYBOARD_LED_CAPS : 0;
	KernalDeviceWrite(guiConsoleKeyboard, 0, auiCommand, 2, NULL);
}


//////////////////////////////////////////////////////////////////////////
//
// A key going down: its character, if it types one.
//
//////////////////////////////////////////////////////////////////////////
void ConsoleKey(uint8 uiCode, bool bExtended)
{
	char	szCharacter[2];
	char	c;
	bool	bShift;
	uint16	i;

	if (!bExtended && (uiCode == CONSOLE_KEY_CAPS_LOCK))
	{
		gbConsoleCapsLock = !gbConsoleCapsLock;
		ConsoleSetLEDs();
		return;
	}
	if (uiCode == CONSOLE_KEY_ENTER)
	{
		ConsoleWrite("\r\n" CONSOLE_PROMPT);
		guiConsoleLineLength = 0;
		return;
	}
	if (!bExtended && (uiCode == CONSOLE_KEY_BACKSPACE))
	{
		if (guiConsoleLineLength > 0)
		{
			ConsoleWrite("\b \b");
			guiConsoleLineLength--;
		}
		return;
	}
	//Of the other extended keys only Numpad / (E0 4A) types, as /.
	if (bExtended && (uiCode != 0x4A))
	{
		return;
	}

	for (i = 0; i < sizeof(gasConsoleKeys) / sizeof(SConsoleKey); i++)
	{
		if (gasConsoleKeys[i].uiCode == uiCode)
		{
			bShift = gbConsoleLeftShift || gbConsoleRightShift;
			c = bShift ? gasConsoleKeys[i].cShifted : gasConsoleKeys[i].cNormal;
			if (gbConsoleCapsLock && (gasConsoleKeys[i].cNormal >= 'a') && (gasConsoleKeys[i].cNormal <= 'z'))
			{
				c = bShift ? gasConsoleKeys[i].cNormal : gasConsoleKeys[i].cShifted;
			}
			if (guiConsoleLineLength < CONSOLE_MAX_LINE)
			{
				szCharacter[0] = c;
				szCharacter[1] = '\0';
				ConsoleWrite(szCharacter);
				guiConsoleLineLength++;
			}
			return;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// One byte of what the keyboard sends: E0 and F0 say what the next code
// is; the shifts are kept track of.  Answers to commands are dropped.
//
//////////////////////////////////////////////////////////////////////////
void ConsoleByte(uint8 uiByte)
{
	bool	bBreak;
	bool	bExtended;

	if (uiByte == PS2_KEYBOARD_EXTENDED)
	{
		gbConsoleExtended = true;
		return;
	}
	if (uiByte == PS2_KEYBOARD_BREAK)
	{
		gbConsoleBreak = true;
		return;
	}
	if ((uiByte == PS2_KEYBOARD_ACK) || (uiByte == PS2_KEYBOARD_BAT_OK) || (uiByte == PS2_KEYBOARD_RESEND) || (uiByte >= 0xE1))
	{
		gbConsoleBreak = false;
		gbConsoleExtended = false;
		return;
	}

	bBreak = gbConsoleBreak;
	bExtended = gbConsoleExtended;
	gbConsoleBreak = false;
	gbConsoleExtended = false;
	if (!bExtended && (uiByte == CONSOLE_KEY_LEFT_SHIFT))
	{
		gbConsoleLeftShift = !bBreak;
	}
	else if (!bExtended && (uiByte == CONSOLE_KEY_RIGHT_SHIFT))
	{
		gbConsoleRightShift = !bBreak;
	}
	else if (!bBreak)
	{
		ConsoleKey(uiByte, bExtended);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The keyboard is reset: it answers FA, then AA once its self test has
// passed.
//
//////////////////////////////////////////////////////////////////////////
uint16 ConsoleResetKeyboard(void)
{
	uint8	auiCommand[1];
	uint32	uiDone;

	auiCommand[0] = PS2_KEYBOARD_RESET;
	if (KernalDeviceWrite(guiConsoleKeyboard, 0, auiCommand, 1, NULL) != KERNAL_DEVICE_SUCCESS)
	{
		return 1;
	}
	if (KernalDeviceWait(guiConsoleKeyboard, 2, CONSOLE_WAIT_TICKS) != KERNAL_DEVICE_SUCCESS)
	{
		return 2;
	}
	uiDone = 0;
	KernalDeviceRead(guiConsoleKeyboard, 0, gauiConsoleBytes, 2, &uiDone);
	if ((uiDone != 2) || (gauiConsoleBytes[1] != PS2_KEYBOARD_BAT_OK))
	{
		return 3;
	}
	return 0;
}


//////////////////////////////////////////////////////////////////////////
//
// The console, as the card's start up program (console.exe, which
// \OS\STARTUP.TXT names).  It only exits if the UART Board or the
// PS2 Board's keyboard can't be had.
//
//////////////////////////////////////////////////////////////////////////
int main(void)
{
	uint32	uiDone;
	uint32	i;
	uint16	uiResult;

	if (KernalDeviceOpen(UART_BOARD_MANUFACTURER, UART_BOARD_NAME, 0, UART_BOARD_RS232, &guiConsoleUART) != KERNAL_DEVICE_SUCCESS)
	{
		return CONSOLE_NO_UART;
	}
	ConsoleWrite(CONSOLE_BANNER);

	if (KernalDeviceOpen(PS2_BOARD_MANUFACTURER, PS2_BOARD_NAME, 0, PS2_BOARD_PORT_1, &guiConsoleKeyboard) != KERNAL_DEVICE_SUCCESS)
	{
		ConsoleWrite("There is no PS2 Board.\r\n");
		return CONSOLE_NO_KEYBOARD;
	}
	uiResult = ConsoleResetKeyboard();
	if (uiResult != 0)
	{
		ConsoleWrite("There is no keyboard in the PS2 Board's port 1.\r\n");
		return CONSOLE_NO_KEYBOARD + uiResult;
	}

	gbConsoleBreak = false;
	gbConsoleExtended = false;
	gbConsoleLeftShift = false;
	gbConsoleRightShift = false;
	gbConsoleCapsLock = false;
	guiConsoleLineLength = 0;
	ConsoleWrite(CONSOLE_PROMPT);
	for (;;)
	{
		if (KernalDeviceWait(guiConsoleKeyboard, 1, CONSOLE_WAIT_TICKS) != KERNAL_DEVICE_SUCCESS)
		{
			continue;
		}
		uiDone = 0;
		KernalDeviceRead(guiConsoleKeyboard, 0, gauiConsoleBytes, sizeof(gauiConsoleBytes), &uiDone);
		for (i = 0; i < uiDone; i++)
		{
			ConsoleByte(gauiConsoleBytes[i]);
		}
	}
}

