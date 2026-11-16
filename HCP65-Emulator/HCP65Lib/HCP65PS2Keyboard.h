#ifndef __HCP65_PS2_KEYBOARD_H__
#define __HCP65_PS2_KEYBOARD_H__
#include "BaseLib/ArrayUint8.h"
#include "BaseLib/Mutex.h"
#include "HCP65PS2.h"


#define HCP65_PS2_KEYBOARD_ACK			0xFA
#define HCP65_PS2_KEYBOARD_RESEND		0xFE
#define HCP65_PS2_KEYBOARD_BAT_OK		0xAA	//The self test passed: sent at power on and after a reset.
#define HCP65_PS2_KEYBOARD_ECHO			0xEE
#define HCP65_PS2_KEYBOARD_ID_1			0xAB	//An MF2 keyboard's ID.
#define HCP65_PS2_KEYBOARD_ID_2			0x83

#define HCP65_PS2_KEYBOARD_SET_LEDS		0xED
#define HCP65_PS2_KEYBOARD_SCAN_SET		0xF0
#define HCP65_PS2_KEYBOARD_IDENTIFY		0xF2
#define HCP65_PS2_KEYBOARD_TYPEMATIC	0xF3
#define HCP65_PS2_KEYBOARD_ENABLE		0xF4
#define HCP65_PS2_KEYBOARD_DISABLE		0xF5
#define HCP65_PS2_KEYBOARD_DEFAULTS		0xF6
#define HCP65_PS2_KEYBOARD_RESET		0xFF

#define HCP65_PS2_KEYBOARD_LED_SCROLL	0x01
#define HCP65_PS2_KEYBOARD_LED_NUM		0x02
#define HCP65_PS2_KEYBOARD_LED_CAPS		0x04

#define HCP65_PS2_KEYBOARD_MAX_CODE		8		//Pause's make code is the longest.


//A PS/2 keyboard (an MF2, scan code set 2) plugged into a port (Attach).  It sends AA when it is plugged in, as at power on.
//
//Keys are sent by name (Key), the names of InputLib's keyboards ("A", "Left Shift", "Cursor Up", "Numpad 7", "Pause/Break"):
//down sends the key's make code, again for each repeat; up its break code (F0 before its last byte).  Key is the only call
//that can come from another thread (the window's): the codes are queued under a lock and the port sends them when it is
//next caught up.  While scanning is disabled (F5) they are dropped.
//
//The commands it answers: FF reset (FA AA; scanning on, LEDs off), FE resend, F2 identify (FA AB 83), EE echo (EE),
//ED set LEDs and F3 typematic rate (FA, then FA for the argument), F0 scan code set (FA, FA; with argument 0, the set,
//2), F4 enable, F5 disable, F6 defaults and F7 to FD (FA).  Anything else gets FE.  Typematic rates aren't kept to:
//repeats are the host's.
class CHCP65PS2Keyboard : public CHCP65PS2Device
{
protected:
	CHCP65PS2Port*	mpcPort;
	CMutex			mcMutex;
	CArrayUint8		macPending;		//Codes from Key, under mcMutex.
	uint8			muiCommand;		//The command whose argument is due, 0 if none.
	uint8			muiLastSent;
	uint8			muiLEDs;
	bool			mbScanning;

public:
	void	Init(void);
	void	Kill(void);

	void	Attach(CHCP65PS2Port* pcPort);
	void	Detach(void);
	CHCP65PS2Port*	GetPort(void);

	bool	Key(const char* szName, bool bDown);

	uint8	GetLEDs(void);
	bool	IsScanning(void);

	void	Received(CHCP65PS2Port* pcPort, uint8 uiByte) override;
	void	CatchUp(CHCP65PS2Port* pcPort) override;

	static size	MakeCode(const char* szName, bool bDown, uint8* puiCode);

protected:
	void	Send(uint8 uiByte);
	void	Argument(uint8 uiByte);
};


#endif // __HCP65_PS2_KEYBOARD_H__

