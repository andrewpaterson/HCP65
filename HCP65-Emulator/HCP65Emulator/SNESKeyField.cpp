#include <string.h>
#include "StandardLib/ClassDefines.h"
#include "WindowLib/ComponentInput.h"
#include "SNESKeyField.h"


//////////////////////////////////////////////////////////////////////////
//
// szKey to start with, NULL or "" for none.
//
//////////////////////////////////////////////////////////////////////////
void CSNESKeyField::Init(Ptr<CWindow> pWindow, Ptr<CWindowTheme> pTheme, const char* szKey)
{
	PreInit();

	CTextField::Init(pWindow, pTheme, szKey ? szKey : "");
	AddInputAction(IAF_AnyKeyDown, NULL, this, &CSNESKeyField::KeyDownAction);

	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CSNESKeyField::Class(void)
{
	CTextField::Class();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CSNESKeyField::Free(void)
{
	CTextField::Free();
}


bool CSNESKeyField::Save(CObjectWriter* pcFile) { return false; }
bool CSNESKeyField::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// The key is the field's text.
//
//////////////////////////////////////////////////////////////////////////
void CSNESKeyField::KeyDownAction(SComponentInputEvent* psEvent)
{
	if (!psEvent->szName || (strcmp(psEvent->szName, "Escape") == 0))
	{
		psEvent->bPass = true;
		return;
	}
	if (strcmp(psEvent->szName, "Backspace") == 0)
	{
		SetText("");
		return;
	}
	SetText(psEvent->szName);
}
