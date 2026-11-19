#include <stdio.h>
#include "StandardLib/ClassDefines.h"
#include "StandardLib/Objects.h"
#include "WindowLib/ComboBox.h"
#include "WindowLib/Label.h"
#include "SNESControllersDialog.h"


//////////////////////////////////////////////////////////////////////////
//
// A row for each controller, starting at pcKeys' mappings, or a line
// that there is no board.
//
//////////////////////////////////////////////////////////////////////////
void CSNESControllersDialog::Init(Ptr<CWindow> pOwner, SDialogLook* psLook, CHCP65SNESKeys* pcKeys)
{
	Ptr<CLabel>	pLabel;
	int			i;

	PreInit();

	maCombos.Init();
	mpDialog = OMalloc<CDialog>(pOwner, psLook, "SNES Controllers");
	if (pcKeys->HasBoard())
	{
		for (i = 0; i < HCP65_SNES_CONTROLLERS; i++)
		{
			AddController(pcKeys, i);
		}
	}
	else
	{
		pLabel = OMalloc<CLabel>(mpDialog->GetWindow(), psLook->pcTheme->GetFont(), "There is no LCD and SNES Board to plug a controller into.");
		pLabel->SetAlignment(CSH_Left, CSV_Center);
		mpDialog->AddEditor(NULL, pLabel);
	}

	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CSNESControllersDialog::Class(void)
{
	M_Pointer(mpDialog);
	M_Embedded(maCombos);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CSNESControllersDialog::Free(void)
{
}


bool CSNESControllersDialog::Save(CObjectWriter* pcFile) { return false; }
bool CSNESControllersDialog::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// "Controller n" and a combo box of every mapping by name, at the one it
// has.
//
//////////////////////////////////////////////////////////////////////////
void CSNESControllersDialog::AddController(CHCP65SNESKeys* pcKeys, int iController)
{
	Ptr<CComboBox>	pCombo;
	char			szLabel[32];
	size			i;

	pCombo = OMalloc<CComboBox>(mpDialog->GetWindow(), mpDialog->GetLook()->pcTheme);
	for (i = 0; i < pcKeys->NumMappings(); i++)
	{
		pCombo->AddItem(pcKeys->GetName(i));
	}
	pCombo->Select((int32)pcKeys->GetController(iController));
	pCombo->SetAlignment(CSH_Stretch, CSV_Center);
	snprintf(szLabel, sizeof(szLabel), "Controller %d", iController + 1);
	mpDialog->AddEditor(szLabel, pCombo);
	maCombos.Add(pCombo);
}


//////////////////////////////////////////////////////////////////////////
//
// True if OK was pressed.
//
//////////////////////////////////////////////////////////////////////////
bool CSNESControllersDialog::Show(void)
{
	if (maCombos.NumElements() > 0)
	{
		mpDialog->GetWindow()->GetFocus()->SetFocus(maCombos.Get(0));
	}
	return mpDialog->Show(720);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CSNESControllersDialog::HasControllers(void)
{
	return maCombos.NumElements() > 0;
}


//////////////////////////////////////////////////////////////////////////
//
// The mapping chosen for iController (HCP65_SNES_KEYS_NONE without the
// board).
//
//////////////////////////////////////////////////////////////////////////
size CSNESControllersDialog::GetMapping(int iController)
{
	int32	iSelected;

	if ((iController < 0) || ((size)iController >= maCombos.NumElements()))
	{
		return HCP65_SNES_KEYS_NONE;
	}
	iSelected = maCombos.Get(iController)->GetSelected();
	return (iSelected < 0) ? HCP65_SNES_KEYS_NONE : (size)iSelected;
}
