#ifndef __KERNAL_KEYBOARD_H__
#define __KERNAL_KEYBOARD_H__
#include "BaseLib/PrimitiveTypes.h"


#define KERNAL_KEYBOARD_MAX_CHARS		4		//The most characters one scan code byte gives (an arrow key's escape sequence).


//A PS/2 keyboard's scan codes (set 2), a byte at a time, as the characters a terminal would send for the keys (US
//layout): letters and symbols with Shift and Caps Lock, Ctrl with a letter its control character, Enter '\r',
//Backspace 8, Tab 9, Escape 27 and the arrows, Home, End and Delete their VT100 escape sequences.  Other keys, and keys
//going up, give nothing.
class CKernalKeyboard
{
protected:
	bool	mbBreak;		//F0 came: the next code is a key going up.
	bool	mbExtended;		//E0 came.
	bool	mbLeftShift;
	bool	mbRightShift;
	bool	mbControl;
	bool	mbCapsLock;

public:
	void	Init(void);
	uint8	Decode(uint8 uiCode, char* szChars);
	bool	IsCapsLock(void) const;
};


#endif // __KERNAL_KEYBOARD_H__
