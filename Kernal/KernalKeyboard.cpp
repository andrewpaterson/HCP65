#include <string.h>
#include "KernalKeyboard.h"


#define KEY_LEFT_SHIFT		0x12
#define KEY_RIGHT_SHIFT		0x59
#define KEY_CONTROL			0x14		//E0 14 is the right one.
#define KEY_CAPS_LOCK		0x58
#define KEY_EXTENDED		0xE0
#define KEY_BREAK			0xF0


//A key's character, and with Shift.
struct SKernalKey
{
	uint8	uiCode;
	char	cNormal;
	char	cShifted;
};


static const SKernalKey	gasKernalKeys[] =
{
	{ 0x0E, '`', '~' },		{ 0x16, '1', '!' },		{ 0x1E, '2', '@' },		{ 0x26, '3', '#' },
	{ 0x25, '4', '$' },		{ 0x2E, '5', '%' },		{ 0x36, '6', '^' },		{ 0x3D, '7', '&' },
	{ 0x3E, '8', '*' },		{ 0x46, '9', '(' },		{ 0x45, '0', ')' },		{ 0x4E, '-', '_' },
	{ 0x55, '=', '+' },		{ 0x66, 0x08, 0x08 },	{ 0x0D, '\t', '\t' },	{ 0x15, 'q', 'Q' },
	{ 0x1D, 'w', 'W' },		{ 0x24, 'e', 'E' },		{ 0x2D, 'r', 'R' },		{ 0x2C, 't', 'T' },
	{ 0x35, 'y', 'Y' },		{ 0x3C, 'u', 'U' },		{ 0x43, 'i', 'I' },		{ 0x44, 'o', 'O' },
	{ 0x4D, 'p', 'P' },		{ 0x54, '[', '{' },		{ 0x5B, ']', '}' },		{ 0x5D, '\\', '|' },
	{ 0x1C, 'a', 'A' },		{ 0x1B, 's', 'S' },		{ 0x23, 'd', 'D' },		{ 0x2B, 'f', 'F' },
	{ 0x34, 'g', 'G' },		{ 0x33, 'h', 'H' },		{ 0x3B, 'j', 'J' },		{ 0x42, 'k', 'K' },
	{ 0x4B, 'l', 'L' },		{ 0x4C, ';', ':' },		{ 0x52, '\'', '"' },	{ 0x5A, '\r', '\r' },
	{ 0x1A, 'z', 'Z' },		{ 0x22, 'x', 'X' },		{ 0x21, 'c', 'C' },		{ 0x2A, 'v', 'V' },
	{ 0x32, 'b', 'B' },		{ 0x31, 'n', 'N' },		{ 0x3A, 'm', 'M' },		{ 0x41, ',', '<' },
	{ 0x49, '.', '>' },		{ 0x4A, '/', '?' },		{ 0x29, ' ', ' ' },		{ 0x76, 0x1B, 0x1B },
	{ 0x70, '0', '0' },		{ 0x69, '1', '1' },		{ 0x72, '2', '2' },		{ 0x7A, '3', '3' },
	{ 0x6B, '4', '4' },		{ 0x73, '5', '5' },		{ 0x74, '6', '6' },		{ 0x6C, '7', '7' },
	{ 0x75, '8', '8' },		{ 0x7D, '9', '9' },		{ 0x71, '.', '.' },		{ 0x7C, '*', '*' },
	{ 0x7B, '-', '-' },		{ 0x79, '+', '+' },
	{ 0, 0, 0 }
};


//The extended keys: what a VT100 sends for them.
struct SKernalExtendedKey
{
	uint8		uiCode;
	const char*	szChars;
};


static const SKernalExtendedKey	gasKernalExtendedKeys[] =
{
	{ 0x75, "\x1B[A" },		//Up.
	{ 0x72, "\x1B[B" },		//Down.
	{ 0x74, "\x1B[C" },		//Right.
	{ 0x6B, "\x1B[D" },		//Left.
	{ 0x6C, "\x1B[H" },		//Home.
	{ 0x69, "\x1B[F" },		//End.
	{ 0x71, "\x1B[3~" },	//Delete.
	{ 0x4A, "/" },			//The keypad's.
	{ 0x5A, "\r" },
	{ 0, NULL }
};


//////////////////////////////////////////////////////////////////////////
//
// No keys down, Caps Lock off.
//
//////////////////////////////////////////////////////////////////////////
void CKernalKeyboard::Init(void)
{
	mbBreak = false;
	mbExtended = false;
	mbLeftShift = false;
	mbRightShift = false;
	mbControl = false;
	mbCapsLock = false;
}


//////////////////////////////////////////////////////////////////////////
//
// The next byte the keyboard sent: what it types into szChars
// (KERNAL_KEYBOARD_MAX_CHARS), and how many characters that is.
//
//////////////////////////////////////////////////////////////////////////
uint8 CKernalKeyboard::Decode(uint8 uiCode, char* szChars)
{
	const SKernalKey*			psKey;
	const SKernalExtendedKey*	psExtended;
	bool						bBreak;
	bool						bExtended;
	bool						bShift;
	char						c;

	if (uiCode == KEY_BREAK)
	{
		mbBreak = true;
		return 0;
	}
	if (uiCode == KEY_EXTENDED)
	{
		mbExtended = true;
		return 0;
	}
	bBreak = mbBreak;
	bExtended = mbExtended;
	mbBreak = false;
	mbExtended = false;

	//The keys that change what the others type.
	if (uiCode == KEY_CONTROL)
	{
		mbControl = !bBreak;
		return 0;
	}
	if (!bExtended && (uiCode == KEY_LEFT_SHIFT))
	{
		mbLeftShift = !bBreak;
		return 0;
	}
	if (!bExtended && (uiCode == KEY_RIGHT_SHIFT))
	{
		mbRightShift = !bBreak;
		return 0;
	}
	if (bBreak)
	{
		return 0;
	}
	if (!bExtended && (uiCode == KEY_CAPS_LOCK))
	{
		mbCapsLock = !mbCapsLock;
		return 0;
	}

	if (bExtended)
	{
		for (psExtended = gasKernalExtendedKeys; psExtended->szChars; psExtended++)
		{
			if (psExtended->uiCode == uiCode)
			{
				strcpy(szChars, psExtended->szChars);
				return (uint8)strlen(szChars);
			}
		}
		return 0;
	}

	for (psKey = gasKernalKeys; psKey->uiCode; psKey++)
	{
		if (psKey->uiCode == uiCode)
		{
			bShift = mbLeftShift || mbRightShift;
			c = bShift ? psKey->cShifted : psKey->cNormal;
			if (mbCapsLock && (psKey->cNormal >= 'a') && (psKey->cNormal <= 'z'))
			{
				c = bShift ? psKey->cNormal : psKey->cShifted;
			}
			if (mbControl && (psKey->cNormal >= 'a') && (psKey->cNormal <= 'z'))
			{
				c = (char)(psKey->cNormal - 'a' + 1);
			}
			szChars[0] = c;
			return 1;
		}
	}
	return 0;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CKernalKeyboard::IsCapsLock(void) const
{
	return mbCapsLock;
}
