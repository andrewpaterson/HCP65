#ifndef __HCP65_SNES_KEYS_H__
#define __HCP65_SNES_KEYS_H__
#include "BaseLib/PrimitiveTypes.h"
#include "HCP65Lib/HCP65LCD.h"


#define HCP65_SNES_KEYS_NONE		0		//The mapping that is a controller not plugged in.
#define HCP65_SNES_KEYS_BUTTONS		12		//A mapping's keys: one for each of a controller's buttons.


//One way of driving a SNES controller with the host's keys: the key (the input's names, as the PS/2 keyboard takes
//them) for each button, in the order the controller sends them (HCP65_SNES_B first).
struct SHCP65SNESKeyMapping
{
	const char*	szName;			//As the machine description keeps it (CHCP65LCDBoard::SetKeys).
	const char*	szDescription;	//As the SNES Controllers dialog shows it.
	const char*	aszKeys[HCP65_SNES_KEYS_BUTTONS];
};


//The host's keys driving the first LCD and SNES Board's two SNES controllers: each controller has one of the mappings
//(HCP65_SNES_KEYS_NONE for none), which Attach takes from the board and SetMapping changes.  Key gives the board a
//key going down or up: a key a controller's mapping has presses or lets go of that button (the emulator gives the
//keyboard it too).  Keys are given on the window's thread; the board reads its buttons on the HCP65's.
class CHCP65SNESKeys
{
protected:
	CHCP65LCDBoard*	mpcLCD;
	size			mauiMappings[HCP65_SNES_CONTROLLERS];
	uint16			mauiButtons[HCP65_SNES_CONTROLLERS];

public:
	void		Init(void);
	void		Kill(void);

	void		Attach(CHCP65LCDBoard* pcLCD);
	bool		HasBoard(void) const;
	bool		Key(const char* szName, bool bDown);

	size		GetMapping(int iController) const;
	void		SetMapping(int iController, size uiMapping);

	static size							NumMappings(void);
	static const SHCP65SNESKeyMapping*	GetMappingInfo(size uiMapping);
	static size							FindMapping(const char* szName);

protected:
	void		Release(int iController);
};


#endif // __HCP65_SNES_KEYS_H__
