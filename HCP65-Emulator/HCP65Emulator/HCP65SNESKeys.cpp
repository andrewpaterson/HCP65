#include <string.h>
#include "HCP65SNESKeys.h"


//The mappings, the first not plugged in.  Each's keys are B, Y, Select, Start, Up, Down, Left, Right, A, X, L and R.
static const SHCP65SNESKeyMapping gasHCP65SNESKeyMappings[] =
{
	{ "", "Not plugged in", { NULL } },
	{ "Arrows", "Arrows; B Z, A X, Y A, X S, L Q, R W, Select Right Shift, Start Enter",
		{ "Z", "A", "Right Shift", "Enter", "Cursor Up", "Cursor Down", "Cursor Left", "Cursor Right", "X", "S", "Q", "W" } },
	{ "WASD", "WASD; B J, A K, Y U, X I, L Q, R E, Select Tab, Start Space",
		{ "J", "U", "Tab", "Space", "W", "S", "A", "D", "K", "I", "Q", "E" } },
	{ "Numpad", "Numpad 8 4 2 6; B 1, A 3, Y 7, X 9, L /, R *, Select -, Start +",
		{ "Numpad 1", "Numpad 7", "Numpad -", "Numpad +", "Numpad 8", "Numpad 2", "Numpad 4", "Numpad 6", "Numpad 3", "Numpad 9", "Numpad /", "Numpad *" } },
};


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65SNESKeys::Init(void)
{
	int		i;

	mpcLCD = NULL;
	for (i = 0; i < HCP65_SNES_CONTROLLERS; i++)
	{
		mauiMappings[i] = HCP65_SNES_KEYS_NONE;
		mauiButtons[i] = 0;
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65SNESKeys::Kill(void)
{
	mpcLCD = NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The board the keys drive, NULL for none; each controller's mapping is
// the one the board names (none if it names one there isn't), with
// nothing pressed.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65SNESKeys::Attach(CHCP65LCDBoard* pcLCD)
{
	int		i;

	mpcLCD = pcLCD;
	for (i = 0; i < HCP65_SNES_CONTROLLERS; i++)
	{
		mauiMappings[i] = pcLCD ? FindMapping(pcLCD->GetKeys(i)) : HCP65_SNES_KEYS_NONE;
		Release(i);
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65SNESKeys::HasBoard(void) const
{
	return mpcLCD != NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// szName going down or up: true if a controller's mapping has it, and it
// pressed or let go of that button.
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65SNESKeys::Key(const char* szName, bool bDown)
{
	const SHCP65SNESKeyMapping*	psMapping;
	bool						bTaken;
	int							iController;
	int							iButton;

	if (!mpcLCD || !szName)
	{
		return false;
	}
	bTaken = false;
	for (iController = 0; iController < HCP65_SNES_CONTROLLERS; iController++)
	{
		psMapping = GetMappingInfo(mauiMappings[iController]);
		for (iButton = 0; iButton < HCP65_SNES_KEYS_BUTTONS; iButton++)
		{
			if (psMapping->aszKeys[iButton] && (strcmp(psMapping->aszKeys[iButton], szName) == 0))
			{
				if (bDown)
				{
					mauiButtons[iController] |= (uint16)(1 << iButton);
				}
				else
				{
					mauiButtons[iController] &= (uint16)~(1 << iButton);
				}
				mpcLCD->SetButtons(iController, mauiButtons[iController]);
				bTaken = true;
			}
		}
	}
	return bTaken;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
size CHCP65SNESKeys::GetMapping(int iController) const
{
	return ((iController >= 0) && (iController < HCP65_SNES_CONTROLLERS)) ? mauiMappings[iController] : HCP65_SNES_KEYS_NONE;
}


//////////////////////////////////////////////////////////////////////////
//
// iController driven by uiMapping's keys from now, with nothing pressed,
// and the board keeps the mapping's name for the machine description.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65SNESKeys::SetMapping(int iController, size uiMapping)
{
	if ((iController < 0) || (iController >= HCP65_SNES_CONTROLLERS) || (uiMapping >= NumMappings()))
	{
		return;
	}
	mauiMappings[iController] = uiMapping;
	Release(iController);
	if (mpcLCD)
	{
		mpcLCD->SetKeys(iController, GetMappingInfo(uiMapping)->szName);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Nothing pressed on iController.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65SNESKeys::Release(int iController)
{
	mauiButtons[iController] = 0;
	if (mpcLCD)
	{
		mpcLCD->SetButtons(iController, 0);
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
size CHCP65SNESKeys::NumMappings(void)
{
	return sizeof(gasHCP65SNESKeyMappings) / sizeof(SHCP65SNESKeyMapping);
}


const SHCP65SNESKeyMapping* CHCP65SNESKeys::GetMappingInfo(size uiMapping)
{
	return &gasHCP65SNESKeyMappings[(uiMapping < NumMappings()) ? uiMapping : HCP65_SNES_KEYS_NONE];
}


//////////////////////////////////////////////////////////////////////////
//
// The mapping named szName, HCP65_SNES_KEYS_NONE for NULL, "" or one
// there isn't.
//
//////////////////////////////////////////////////////////////////////////
size CHCP65SNESKeys::FindMapping(const char* szName)
{
	size	i;

	if (!szName || (szName[0] == '\0'))
	{
		return HCP65_SNES_KEYS_NONE;
	}
	for (i = 1; i < NumMappings(); i++)
	{
		if (strcmp(gasHCP65SNESKeyMappings[i].szName, szName) == 0)
		{
			return i;
		}
	}
	return HCP65_SNES_KEYS_NONE;
}
