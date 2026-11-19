#ifndef __HCP65_SNES_KEYS_H__
#define __HCP65_SNES_KEYS_H__
#include "BaseLib/Chars.h"
#include "BaseLib/PrimitiveTypes.h"
#include "HCP65Lib/HCP65LCD.h"


#define HCP65_SNES_KEYS_NONE		0		//The mapping that is a controller not plugged in.
#define HCP65_SNES_KEYS_BUTTONS		12		//A mapping's keys: one for each of a controller's buttons.
#define HCP65_SNES_KEYS_MAX			32		//Mappings, the one that is none among them.
#define HCP65_SNES_KEYS_XML_TAG		"SNESMappings"
#define HCP65_SNES_MAPPING_XML_TAG	"Mapping"
#define HCP65_SNES_BUTTON_XML_TAG	"Button"


//One way of driving a SNES controller with the host's keys: its name, and the key (the input's names, as the PS/2
//keyboard takes them; empty for none) for each button, in the order the controller sends them (HCP65_SNES_B first).
struct SHCP65SNESKeyMapping
{
	CChars	szName;
	CChars	aszKeys[HCP65_SNES_KEYS_BUTTONS];

	void	Init(const char* szMappingName);
	void	Kill(void);
};


//The host's keys driving the first LCD and SNES Board's two SNES controllers.
//
//The mappings are the emulator's, kept in a file of their own (Load, Save):
//	<SNESMappings>
//		<Mapping Name="Arrows">
//			<Button Name="B" Key="Z"/> ... one for each button with a key (GetButtonName's names).
//		</Mapping>
//	</SNESMappings>
//The first mapping, HCP65_SNES_KEYS_NONE, is always a controller not plugged in, and isn't in the file.  Without the
//file there are the three the emulator starts with (Arrows, WASD and Numpad).  SetMapping adds or changes one.
//
//Each controller has one of the mappings, which Attach takes from the board (by name: CHCP65LCDBoard::GetKeys) and
//SetController changes.  Key gives the board a key going down or up: a key a controller's mapping has presses or lets
//go of that button (the emulator gives the keyboard it too).  Keys are given on the window's thread; the board reads
//its buttons on the HCP65's.
class CHCP65SNESKeys
{
protected:
	CHCP65LCDBoard*			mpcLCD;
	SHCP65SNESKeyMapping	masMappings[HCP65_SNES_KEYS_MAX];
	size					muiNumMappings;
	size					mauiControllers[HCP65_SNES_CONTROLLERS];	//Each controller's mapping.
	uint16					mauiButtons[HCP65_SNES_CONTROLLERS];

public:
	void		Init(void);
	void		Kill(void);

	bool		Load(const char* szFilename);
	bool		Save(const char* szFilename);

	void		Attach(CHCP65LCDBoard* pcLCD);
	bool		HasBoard(void) const;
	bool		Key(const char* szName, bool bDown);

	size		GetController(int iController) const;
	void		SetController(int iController, size uiMapping);

	size		NumMappings(void) const;
	const char*	GetName(size uiMapping);
	const char*	GetKey(size uiMapping, int iButton);
	size		FindMapping(const char* szName);
	size		SetMapping(const char* szName, const char* const* aszKeys);

	static const char*	GetButtonName(int iButton);
	static int			FindButton(const char* szName);

protected:
	void		Clear(void);
	void		AddDefaults(void);
	void		Release(int iController);
};


#endif // __HCP65_SNES_KEYS_H__
