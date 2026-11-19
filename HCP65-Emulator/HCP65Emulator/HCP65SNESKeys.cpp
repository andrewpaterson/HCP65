#include <string.h>
#include "BaseLib/FileUtil.h"
#include "BaseLib/MarkupTag.h"
#include "BaseLib/XMLFile.h"
#include "BaseLib/XMLWriter.h"
#include "HCP65SNESKeys.h"


#define HCP65_SNES_KEYS_NONE_NAME	"Not plugged in"


//The buttons' names, in the order a controller sends them.
static const char* gaszHCP65SNESButtons[HCP65_SNES_KEYS_BUTTONS] = { "B", "Y", "Select", "Start", "Up", "Down", "Left", "Right", "A", "X", "L", "R" };

//The mappings there are without a file.  Each's keys are B, Y, Select, Start, Up, Down, Left, Right, A, X, L and R.
struct SHCP65SNESDefaultMapping
{
	const char*	szName;
	const char*	aszKeys[HCP65_SNES_KEYS_BUTTONS];
};

static const SHCP65SNESDefaultMapping gasHCP65SNESDefaults[] =
{
	{ "Arrows", { "Z", "A", "Right Shift", "Enter", "Cursor Up", "Cursor Down", "Cursor Left", "Cursor Right", "X", "S", "Q", "W" } },
	{ "WASD", { "J", "U", "Tab", "Space", "W", "S", "A", "D", "K", "I", "Q", "E" } },
	{ "Numpad", { "Numpad 1", "Numpad 7", "Numpad -", "Numpad +", "Numpad 8", "Numpad 2", "Numpad 4", "Numpad 6", "Numpad 3", "Numpad 9", "Numpad /", "Numpad *" } },
};


//////////////////////////////////////////////////////////////////////////
//
// No keys.
//
//////////////////////////////////////////////////////////////////////////
void SHCP65SNESKeyMapping::Init(const char* szMappingName)
{
	int		i;

	szName.Init(szMappingName);
	for (i = 0; i < HCP65_SNES_KEYS_BUTTONS; i++)
	{
		aszKeys[i].Init();
	}
}


void SHCP65SNESKeyMapping::Kill(void)
{
	int		i;

	for (i = 0; i < HCP65_SNES_KEYS_BUTTONS; i++)
	{
		aszKeys[i].Kill();
	}
	szName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The defaults, nothing attached.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65SNESKeys::Init(void)
{
	int		i;

	mpcLCD = NULL;
	muiNumMappings = 0;
	AddDefaults();
	for (i = 0; i < HCP65_SNES_CONTROLLERS; i++)
	{
		mauiControllers[i] = HCP65_SNES_KEYS_NONE;
		mauiButtons[i] = 0;
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65SNESKeys::Kill(void)
{
	size	i;

	for (i = 0; i < muiNumMappings; i++)
	{
		masMappings[i].Kill();
	}
	muiNumMappings = 0;
	mpcLCD = NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// Only the mapping that is none.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65SNESKeys::Clear(void)
{
	size	i;

	for (i = 0; i < muiNumMappings; i++)
	{
		masMappings[i].Kill();
	}
	masMappings[HCP65_SNES_KEYS_NONE].Init(HCP65_SNES_KEYS_NONE_NAME);
	muiNumMappings = 1;
}


//////////////////////////////////////////////////////////////////////////
//
// None and the three the emulator starts with.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65SNESKeys::AddDefaults(void)
{
	size	i;

	Clear();
	for (i = 0; i < sizeof(gasHCP65SNESDefaults) / sizeof(SHCP65SNESDefaultMapping); i++)
	{
		SetMapping(gasHCP65SNESDefaults[i].szName, gasHCP65SNESDefaults[i].aszKeys);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The mappings in szFilename, in place of these.  False, keeping the
// defaults, if there is no such file or it isn't one.  The controllers
// keep their mappings by name (none if it has gone).
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65SNESKeys::Load(const char* szFilename)
{
	CXMLFile		cFile;
	CFileUtil		cFileUtil;
	CChars			szName;
	CChars			szDirectory;
	CMarkupTag*		pcRoot;
	CMarkupTag*		pcMapping;
	CMarkupTag*		pcButton;
	STagIterator	sMappings;
	STagIterator	sButtons;
	const char*		aszKeys[HCP65_SNES_KEYS_BUTTONS];
	const char*		szMappingName;
	CChars			aszNames[HCP65_SNES_CONTROLLERS];
	int				iButton;
	int				i;
	bool			bResult;

	szName.Init();
	szDirectory.Init();
	cFileUtil.SplitPath(szFilename, &szName, &szDirectory);
	if (szDirectory.Empty())
	{
		szDirectory.Set(".");
	}
	cFile.Init();
	pcRoot = NULL;
	if (cFileUtil.Exists(szFilename) && cFile.Read(szName.Text(), szDirectory.Text()) && cFile.mcMarkup.mpcDoc)
	{
		pcRoot = cFile.mcMarkup.mpcDoc->GetRootTag();
	}
	bResult = pcRoot && (strcmp(pcRoot->GetName(), HCP65_SNES_KEYS_XML_TAG) == 0);
	if (bResult)
	{
		for (i = 0; i < HCP65_SNES_CONTROLLERS; i++)
		{
			aszNames[i].Init(GetName(mauiControllers[i]));
		}
		Clear();
		for (pcMapping = pcRoot->GetTag(HCP65_SNES_MAPPING_XML_TAG, &sMappings); pcMapping; pcMapping = pcRoot->GetNextTag(&sMappings))
		{
			szMappingName = pcMapping->GetStringAttribute("Name");
			if (!szMappingName || (szMappingName[0] == '\0'))
			{
				continue;
			}
			for (iButton = 0; iButton < HCP65_SNES_KEYS_BUTTONS; iButton++)
			{
				aszKeys[iButton] = NULL;
			}
			for (pcButton = pcMapping->GetTag(HCP65_SNES_BUTTON_XML_TAG, &sButtons); pcButton; pcButton = pcMapping->GetNextTag(&sButtons))
			{
				iButton = FindButton(pcButton->GetStringAttribute("Name"));
				if (iButton >= 0)
				{
					aszKeys[iButton] = pcButton->GetStringAttribute("Key");
				}
			}
			SetMapping(szMappingName, aszKeys);
		}
		for (i = 0; i < HCP65_SNES_CONTROLLERS; i++)
		{
			mauiControllers[i] = FindMapping(aszNames[i].Text());
			aszNames[i].Kill();
		}
	}
	cFile.Kill();
	szDirectory.Kill();
	szName.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Every mapping but none into szFilename.
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65SNESKeys::Save(const char* szFilename)
{
	CXMLWriter	cWriter;
	size		uiMapping;
	int			iButton;
	bool		bResult;

	cWriter.Init();
	cWriter.SetIndent("\t");
	cWriter.Comment(" The HCP65 emulator's SNES controller key mappings (HCP65 menu, SNES Key Mappings). ");
	cWriter.Begin(HCP65_SNES_KEYS_XML_TAG);
	for (uiMapping = HCP65_SNES_KEYS_NONE + 1; uiMapping < muiNumMappings; uiMapping++)
	{
		cWriter.Begin(HCP65_SNES_MAPPING_XML_TAG);
		cWriter.Attribute("Name", masMappings[uiMapping].szName.Text());
		for (iButton = 0; iButton < HCP65_SNES_KEYS_BUTTONS; iButton++)
		{
			if (!masMappings[uiMapping].aszKeys[iButton].Empty())
			{
				cWriter.Begin(HCP65_SNES_BUTTON_XML_TAG);
				cWriter.Attribute("Name", gaszHCP65SNESButtons[iButton]);
				cWriter.Attribute("Key", masMappings[uiMapping].aszKeys[iButton].Text());
				cWriter.End();
			}
		}
		cWriter.End();
	}
	cWriter.End();
	bResult = cWriter.Write(szFilename);
	cWriter.Kill();
	return bResult;
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
		mauiControllers[i] = pcLCD ? FindMapping(pcLCD->GetKeys(i)) : HCP65_SNES_KEYS_NONE;
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
	SHCP65SNESKeyMapping*	psMapping;
	bool					bTaken;
	int						iController;
	int						iButton;

	if (!mpcLCD || !szName)
	{
		return false;
	}
	bTaken = false;
	for (iController = 0; iController < HCP65_SNES_CONTROLLERS; iController++)
	{
		psMapping = &masMappings[mauiControllers[iController]];
		for (iButton = 0; iButton < HCP65_SNES_KEYS_BUTTONS; iButton++)
		{
			if (!psMapping->aszKeys[iButton].Empty() && psMapping->aszKeys[iButton].Equals(szName))
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
size CHCP65SNESKeys::GetController(int iController) const
{
	return ((iController >= 0) && (iController < HCP65_SNES_CONTROLLERS)) ? mauiControllers[iController] : HCP65_SNES_KEYS_NONE;
}


//////////////////////////////////////////////////////////////////////////
//
// iController driven by uiMapping's keys from now, with nothing pressed,
// and the board keeps the mapping's name for the machine description.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65SNESKeys::SetController(int iController, size uiMapping)
{
	if ((iController < 0) || (iController >= HCP65_SNES_CONTROLLERS) || (uiMapping >= muiNumMappings))
	{
		return;
	}
	mauiControllers[iController] = uiMapping;
	Release(iController);
	if (mpcLCD)
	{
		mpcLCD->SetKeys(iController, (uiMapping == HCP65_SNES_KEYS_NONE) ? "" : masMappings[uiMapping].szName.Text());
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
size CHCP65SNESKeys::NumMappings(void) const
{
	return muiNumMappings;
}


const char* CHCP65SNESKeys::GetName(size uiMapping)
{
	return masMappings[(uiMapping < muiNumMappings) ? uiMapping : HCP65_SNES_KEYS_NONE].szName.Text();
}


//////////////////////////////////////////////////////////////////////////
//
// uiMapping's key for iButton, "" for none.
//
//////////////////////////////////////////////////////////////////////////
const char* CHCP65SNESKeys::GetKey(size uiMapping, int iButton)
{
	if ((uiMapping >= muiNumMappings) || (iButton < 0) || (iButton >= HCP65_SNES_KEYS_BUTTONS))
	{
		return "";
	}
	return masMappings[uiMapping].aszKeys[iButton].Text();
}


//////////////////////////////////////////////////////////////////////////
//
// The mapping named szName, HCP65_SNES_KEYS_NONE for NULL, "", or one
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
	for (i = HCP65_SNES_KEYS_NONE + 1; i < muiNumMappings; i++)
	{
		if (masMappings[i].szName.Equals(szName))
		{
			return i;
		}
	}
	return HCP65_SNES_KEYS_NONE;
}


//////////////////////////////////////////////////////////////////////////
//
// The mapping named szName has aszKeys (one for each button, in order;
// NULL or "" for none), made if there isn't one.  Its index, or
// HCP65_SNES_KEYS_NONE if there's no name, it is the name of none, or
// there are as many as there can be.
//
//////////////////////////////////////////////////////////////////////////
size CHCP65SNESKeys::SetMapping(const char* szName, const char* const* aszKeys)
{
	size	uiMapping;
	int		i;

	if (!szName || (szName[0] == '\0') || (strcmp(szName, HCP65_SNES_KEYS_NONE_NAME) == 0))
	{
		return HCP65_SNES_KEYS_NONE;
	}
	uiMapping = FindMapping(szName);
	if (uiMapping == HCP65_SNES_KEYS_NONE)
	{
		if (muiNumMappings >= HCP65_SNES_KEYS_MAX)
		{
			return HCP65_SNES_KEYS_NONE;
		}
		uiMapping = muiNumMappings;
		masMappings[uiMapping].Init(szName);
		muiNumMappings++;
	}
	for (i = 0; i < HCP65_SNES_KEYS_BUTTONS; i++)
	{
		masMappings[uiMapping].aszKeys[i].Set((aszKeys && aszKeys[i]) ? aszKeys[i] : "");
	}
	for (i = 0; i < HCP65_SNES_CONTROLLERS; i++)
	{
		if (mauiControllers[i] == uiMapping)
		{
			Release(i);
		}
	}
	return uiMapping;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
const char* CHCP65SNESKeys::GetButtonName(int iButton)
{
	return ((iButton >= 0) && (iButton < HCP65_SNES_KEYS_BUTTONS)) ? gaszHCP65SNESButtons[iButton] : "";
}


int CHCP65SNESKeys::FindButton(const char* szName)
{
	int		i;

	if (!szName)
	{
		return -1;
	}
	for (i = 0; i < HCP65_SNES_KEYS_BUTTONS; i++)
	{
		if (strcmp(gaszHCP65SNESButtons[i], szName) == 0)
		{
			return i;
		}
	}
	return -1;
}
