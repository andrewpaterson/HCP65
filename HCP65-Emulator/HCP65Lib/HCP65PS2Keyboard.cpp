#include <string.h>
#include "HCP65PS2Keyboard.h"


//A key's set 2 make code: an E0 first for the extended keys.  Print Screen and Pause/Break have their own codes
//(MakeCode).
struct SHCP65PS2Key
{
	const char*	szName;
	bool		bExtended;
	uint8		uiCode;
};


static const SHCP65PS2Key gasHCP65PS2Keys[] =
{
	{ "Escape", false, 0x76 }, { "F1", false, 0x05 }, { "F2", false, 0x06 }, { "F3", false, 0x04 }, { "F4", false, 0x0C },
	{ "F5", false, 0x03 }, { "F6", false, 0x0B }, { "F7", false, 0x83 }, { "F8", false, 0x0A }, { "F9", false, 0x01 },
	{ "F10", false, 0x09 }, { "F11", false, 0x78 }, { "F12", false, 0x07 }, { "Scroll Lock", false, 0x7E },
	{ "`", false, 0x0E }, { "1", false, 0x16 }, { "2", false, 0x1E }, { "3", false, 0x26 }, { "4", false, 0x25 },
	{ "5", false, 0x2E }, { "6", false, 0x36 }, { "7", false, 0x3D }, { "8", false, 0x3E }, { "9", false, 0x46 },
	{ "0", false, 0x45 }, { "-", false, 0x4E }, { "=", false, 0x55 }, { "Backspace", false, 0x66 },
	{ "Tab", false, 0x0D }, { "Q", false, 0x15 }, { "W", false, 0x1D }, { "E", false, 0x24 }, { "R", false, 0x2D },
	{ "T", false, 0x2C }, { "Y", false, 0x35 }, { "U", false, 0x3C }, { "I", false, 0x43 }, { "O", false, 0x44 },
	{ "P", false, 0x4D }, { "[", false, 0x54 }, { "]", false, 0x5B }, { "\\", false, 0x5D },
	{ "Caps Lock", false, 0x58 }, { "A", false, 0x1C }, { "S", false, 0x1B }, { "D", false, 0x23 }, { "F", false, 0x2B },
	{ "G", false, 0x34 }, { "H", false, 0x33 }, { "J", false, 0x3B }, { "K", false, 0x42 }, { "L", false, 0x4B },
	{ ";", false, 0x4C }, { "'", false, 0x52 }, { "Enter", false, 0x5A },
	{ "Left Shift", false, 0x12 }, { "Z", false, 0x1A }, { "X", false, 0x22 }, { "C", false, 0x21 }, { "V", false, 0x2A },
	{ "B", false, 0x32 }, { "N", false, 0x31 }, { "M", false, 0x3A }, { ",", false, 0x41 }, { ".", false, 0x49 },
	{ "/", false, 0x4A }, { "Right Shift", false, 0x59 },
	{ "Left Control", false, 0x14 }, { "Left Special", true, 0x1F }, { "Left Alt", false, 0x11 }, { "Space", false, 0x29 },
	{ "Right Alt", true, 0x11 }, { "Right Special", true, 0x27 }, { "Menu", true, 0x2F }, { "Right Control", true, 0x14 },
	{ "Insert", true, 0x70 }, { "Home", true, 0x6C }, { "Page Up", true, 0x7D }, { "Delete", true, 0x71 },
	{ "End", true, 0x69 }, { "Page Down", true, 0x7A },
	{ "Cursor Up", true, 0x75 }, { "Cursor Left", true, 0x6B }, { "Cursor Down", true, 0x72 }, { "Cursor Right", true, 0x74 },
	{ "Num Lock", false, 0x77 }, { "Numpad /", true, 0x4A }, { "Numpad *", false, 0x7C }, { "Numpad -", false, 0x7B },
	{ "Numpad 7", false, 0x6C }, { "Numpad 8", false, 0x75 }, { "Numpad 9", false, 0x7D }, { "Numpad +", false, 0x79 },
	{ "Numpad 4", false, 0x6B }, { "Numpad 5", false, 0x73 }, { "Numpad 6", false, 0x74 },
	{ "Numpad 1", false, 0x69 }, { "Numpad 2", false, 0x72 }, { "Numpad 3", false, 0x7A }, { "Numpad Enter", true, 0x5A },
	{ "Numpad 0", false, 0x70 }, { "Numpad .", false, 0x71 },
};


static const uint8	gauiHCP65PS2PrintScreenMake[] = { 0xE0, 0x12, 0xE0, 0x7C };
static const uint8	gauiHCP65PS2PrintScreenBreak[] = { 0xE0, 0xF0, 0x7C, 0xE0, 0xF0, 0x12 };
static const uint8	gauiHCP65PS2PauseMake[] = { 0xE1, 0x14, 0x77, 0xE1, 0xF0, 0x14, 0xF0, 0x77 };


//////////////////////////////////////////////////////////////////////////
//
// Not plugged in.  Scanning is on, the LEDs off.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65PS2Keyboard::Init(void)
{
	mpcPort = NULL;
	mcMutex.Init();
	macPending.Init();
	muiCommand = 0;
	muiLastSent = 0;
	muiLEDs = 0;
	mbScanning = true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65PS2Keyboard::Kill(void)
{
	Detach();
	macPending.Kill();
	mcMutex.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Plugged into pcPort (out of any other), as at power on: its self test
// passes, so it sends AA.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65PS2Keyboard::Attach(CHCP65PS2Port* pcPort)
{
	Detach();
	mpcPort = pcPort;
	mpcPort->SetDevice(this);
	muiCommand = 0;
	muiLEDs = 0;
	mbScanning = true;
	Send(HCP65_PS2_KEYBOARD_BAT_OK);
}


//////////////////////////////////////////////////////////////////////////
//
// Unplugged; the keys it hadn't sent are dropped.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65PS2Keyboard::Detach(void)
{
	if (mpcPort && (mpcPort->GetDevice() == this))
	{
		mpcPort->SetDevice(NULL);
	}
	mpcPort = NULL;
	mcMutex.Lock();
	macPending.ReInit();
	mcMutex.Unlock();
}


CHCP65PS2Port* CHCP65PS2Keyboard::GetPort(void) { return mpcPort; }
uint8 CHCP65PS2Keyboard::GetLEDs(void) { return muiLEDs; }
bool CHCP65PS2Keyboard::IsScanning(void) { return mbScanning; }


//////////////////////////////////////////////////////////////////////////
//
// The key szName went down (bDown) or up.  False for a name it hasn't.
// Any thread.
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65PS2Keyboard::Key(const char* szName, bool bDown)
{
	uint8	auiCode[HCP65_PS2_KEYBOARD_MAX_CODE];
	size	uiLength;

	uiLength = MakeCode(szName, bDown, auiCode);
	if (uiLength == 0)
	{
		return strcmp(szName, "Pause/Break") == 0;
	}
	mcMutex.Lock();
	macPending.Add(auiCode, uiLength);
	mcMutex.Unlock();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// szName's make code (bDown) or break code into puiCode, and its length;
// 0 for a name it hasn't, or Pause/Break going up (it has no break code).
//
//////////////////////////////////////////////////////////////////////////
size CHCP65PS2Keyboard::MakeCode(const char* szName, bool bDown, uint8* puiCode)
{
	size	uiLength;
	size	i;

	if (strcmp(szName, "Print Screen") == 0)
	{
		if (bDown)
		{
			memcpy(puiCode, gauiHCP65PS2PrintScreenMake, sizeof(gauiHCP65PS2PrintScreenMake));
			return sizeof(gauiHCP65PS2PrintScreenMake);
		}
		memcpy(puiCode, gauiHCP65PS2PrintScreenBreak, sizeof(gauiHCP65PS2PrintScreenBreak));
		return sizeof(gauiHCP65PS2PrintScreenBreak);
	}
	if (strcmp(szName, "Pause/Break") == 0)
	{
		if (!bDown)
		{
			return 0;
		}
		memcpy(puiCode, gauiHCP65PS2PauseMake, sizeof(gauiHCP65PS2PauseMake));
		return sizeof(gauiHCP65PS2PauseMake);
	}

	for (i = 0; i < sizeof(gasHCP65PS2Keys) / sizeof(SHCP65PS2Key); i++)
	{
		if (strcmp(szName, gasHCP65PS2Keys[i].szName) == 0)
		{
			uiLength = 0;
			if (gasHCP65PS2Keys[i].bExtended)
			{
				puiCode[uiLength++] = 0xE0;
			}
			if (!bDown)
			{
				puiCode[uiLength++] = 0xF0;
			}
			puiCode[uiLength++] = gasHCP65PS2Keys[i].uiCode;
			return uiLength;
		}
	}
	return 0;
}


//////////////////////////////////////////////////////////////////////////
//
// The keys queued since, while scanning.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65PS2Keyboard::CatchUp(CHCP65PS2Port* pcPort)
{
	size	i;

	mcMutex.Lock();
	if (mbScanning)
	{
		for (i = 0; i < macPending.NumElements(); i++)
		{
			Send(macPending.GetValue(i));
		}
	}
	macPending.ReInit();
	mcMutex.Unlock();
}


//////////////////////////////////////////////////////////////////////////
//
// A byte from the computer: a command, or the argument of the last one.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65PS2Keyboard::Received(CHCP65PS2Port* pcPort, uint8 uiByte)
{
	if (muiCommand != 0)
	{
		Argument(uiByte);
		return;
	}

	switch (uiByte)
	{
	case HCP65_PS2_KEYBOARD_RESET:
		muiLEDs = 0;
		mbScanning = true;
		Send(HCP65_PS2_KEYBOARD_ACK);
		Send(HCP65_PS2_KEYBOARD_BAT_OK);
		break;

	case HCP65_PS2_KEYBOARD_RESEND:
		Send(muiLastSent);
		break;

	case HCP65_PS2_KEYBOARD_IDENTIFY:
		Send(HCP65_PS2_KEYBOARD_ACK);
		Send(HCP65_PS2_KEYBOARD_ID_1);
		Send(HCP65_PS2_KEYBOARD_ID_2);
		break;

	case HCP65_PS2_KEYBOARD_ECHO:
		Send(HCP65_PS2_KEYBOARD_ECHO);
		break;

	case HCP65_PS2_KEYBOARD_SET_LEDS:
	case HCP65_PS2_KEYBOARD_TYPEMATIC:
	case HCP65_PS2_KEYBOARD_SCAN_SET:
		muiCommand = uiByte;
		Send(HCP65_PS2_KEYBOARD_ACK);
		break;

	case HCP65_PS2_KEYBOARD_ENABLE:
		mbScanning = true;
		Send(HCP65_PS2_KEYBOARD_ACK);
		break;

	case HCP65_PS2_KEYBOARD_DISABLE:
		mbScanning = false;
		Send(HCP65_PS2_KEYBOARD_ACK);
		break;

	default:
		if ((uiByte >= HCP65_PS2_KEYBOARD_DEFAULTS) && (uiByte <= 0xFD))
		{
			Send(HCP65_PS2_KEYBOARD_ACK);
		}
		else
		{
			Send(HCP65_PS2_KEYBOARD_RESEND);
		}
		break;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The argument of ED, F3 or F0.  F0 0 asks which set it is.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65PS2Keyboard::Argument(uint8 uiByte)
{
	uint8	uiCommand;

	uiCommand = muiCommand;
	muiCommand = 0;
	Send(HCP65_PS2_KEYBOARD_ACK);
	if (uiCommand == HCP65_PS2_KEYBOARD_SET_LEDS)
	{
		muiLEDs = (uint8)(uiByte & (HCP65_PS2_KEYBOARD_LED_SCROLL | HCP65_PS2_KEYBOARD_LED_NUM | HCP65_PS2_KEYBOARD_LED_CAPS));
	}
	else if ((uiCommand == HCP65_PS2_KEYBOARD_SCAN_SET) && (uiByte == 0))
	{
		Send(0x02);
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65PS2Keyboard::Send(uint8 uiByte)
{
	if (mpcPort)
	{
		muiLastSent = uiByte;
		mpcPort->HostSend(&uiByte, 1);
	}
}

