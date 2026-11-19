#include <string.h>
#include "StandardLib/ClassDefines.h"
#include "StandardLib/Objects.h"
#include "WindowLib/ClearCanvasDraw.h"
#include "WindowLib/ComponentInput.h"
#include "WindowLib/Label.h"
#include "SNESMappingDialog.h"


//The key fields' rows on top of the picture, as the buttons are on it from the top: the shoulders and the middle, then
//the D-pad, then the diamond.  Buttons are numbered as a controller sends them (CHCP65SNESKeys::GetButtonName).
static const int gaiSNESMappingFieldOrder[HCP65_SNES_KEYS_BUTTONS] = { 10, 2, 3, 11, 4, 5, 6, 7, 1, 9, 0, 8 };


//////////////////////////////////////////////////////////////////////////
//
// The mapping combo box at uiMapping (HCP65_SNES_KEYS_NONE for a new
// one), and everything filled in from it.
//
//////////////////////////////////////////////////////////////////////////
void CSNESMappingDialog::Init(Ptr<CWindow> pOwner, SDialogLook* psLook, Ptr<CWindowTheme> pTheme, CHCP65SNESKeys* pcKeys, size uiMapping)
{
	size	i;

	PreInit();

	mpcKeys = pcKeys;
	maFields.Init();
	mpDialog = OMalloc<CDialog>(pOwner, psLook, "SNES Key Mappings");

	mpMappings = OMalloc<CComboBox>(mpDialog->GetWindow(), pTheme);
	for (i = HCP65_SNES_KEYS_NONE + 1; i < pcKeys->NumMappings(); i++)
	{
		mpMappings->AddItem(pcKeys->GetName(i));
	}
	mpMappings->AddItem(SNES_MAPPING_NEW);
	mpMappings->SetAlignment(CSH_Stretch, CSV_Center);
	mpDialog->AddEditor("Mapping", mpMappings);

	mpName = OMalloc<CTextField>(mpDialog->GetWindow(), pTheme, "");
	mpName->SetAlignment(CSH_Stretch, CSV_Center);
	mpDialog->AddEditor("Name", mpName);

	mpDialog->AddEditor(NULL, MakeFields(pTheme));

	mpController = OMalloc<CSNESControllerDraw>(pTheme->GetFont(), pTheme);
	mpCanvas = OMalloc<CCanvas>(mpDialog->GetWindow(), mpController);
	mpCanvas->SetAlignment(CSH_Center, CSV_Center);
	mpDialog->AddEditor(NULL, mpCanvas);

	if ((uiMapping == HCP65_SNES_KEYS_NONE) || (uiMapping >= pcKeys->NumMappings()))
	{
		mpMappings->Select((int32)(pcKeys->NumMappings() - 1));
	}
	else
	{
		mpMappings->Select((int32)(uiMapping - 1));
	}
	MappingChosen();
	mpMappings->GetSelectionEvent()->Add(this, &CSNESMappingDialog::MappingChosen);
	mpDialog->SetTicker(this, &CSNESMappingDialog::Tick);

	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CSNESMappingDialog::Class(void)
{
	M_Pointer(mpDialog);
	M_Pointer(mpMappings);
	M_Pointer(mpName);
	M_Embedded(maFields);
	M_Pointer(mpCanvas);
	M_Pointer(mpController);
	U_Pointer(mpcKeys);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CSNESMappingDialog::Free(void)
{
	if (mpDialog.IsNotNull())
	{
		mpDialog->ClearTicker();
	}
	if (mpMappings.IsNotNull())
	{
		mpMappings->GetSelectionEvent()->Remove(this);
	}
}


bool CSNESMappingDialog::Save(CObjectWriter* pcFile) { return false; }
bool CSNESMappingDialog::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// A grid of the buttons' names and their key fields, SNES_MAPPING_COLUMNS
// to a row.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CGridContainer> CSNESMappingDialog::MakeFields(Ptr<CWindowTheme> pTheme)
{
	Ptr<CGridContainer>	pGrid;
	Ptr<CLabel>			pLabel;
	Ptr<CSNESKeyField>	pField;
	Ptr<CCanvas>		pGap;
	Ptr<CSNESKeyField>	apFields[HCP65_SNES_KEYS_BUTTONS];
	int					iButton;
	int					iColumn;
	int					iRow;
	int					i;

	pGrid = OMalloc<CGridContainer>(mpDialog->GetWindow());
	pGrid->SetAlignment(CSH_Stretch, CSV_Top);
	for (i = 0; i < HCP65_SNES_KEYS_BUTTONS; i++)
	{
		iButton = gaiSNESMappingFieldOrder[i];
		iColumn = (i % SNES_MAPPING_COLUMNS) * 3;
		iRow = (i / SNES_MAPPING_COLUMNS) * 2;
		pLabel = OMalloc<CLabel>(mpDialog->GetWindow(), pTheme->GetFont(), CHCP65SNESKeys::GetButtonName(iButton));
		pLabel->SetDesiredSize(pLabel->GetRequiredSize().x + pTheme->GetMetric(TM_DialogLabelGap), -1);
		pLabel->SetAlignment(CSH_Left, CSV_Center);
		pGrid->AddComponent(pLabel, iColumn, iRow);
		pField = OMalloc<CSNESKeyField>(mpDialog->GetWindow(), pTheme, "");
		pField->SetAlignment(CSH_Stretch, CSV_Center);
		pGrid->AddComponent(pField, iColumn + 1, iRow, 1, 1, 1, 0);
		apFields[iButton] = pField;
		if ((i % SNES_MAPPING_COLUMNS) != (SNES_MAPPING_COLUMNS - 1))
		{
			pGap = OMalloc<CCanvas>(mpDialog->GetWindow(), OMalloc<CClearCanvasDraw>(pTheme->GetColour(TC_DialogBackground)));
			pGap->SetDesiredSize(pTheme->GetMetric(TM_DialogLabelGap) * 2, 1);
			pGrid->AddComponent(pGap, iColumn + 2, iRow);
		}
		if ((i % SNES_MAPPING_COLUMNS) == 0)
		{
			pGap = OMalloc<CCanvas>(mpDialog->GetWindow(), OMalloc<CClearCanvasDraw>(pTheme->GetColour(TC_DialogBackground)));
			pGap->SetDesiredSize(1, pTheme->GetMetric(TM_DialogRowGap));
			pGrid->AddComponent(pGap, 0, iRow + 1);
		}
	}
	for (i = 0; i < HCP65_SNES_KEYS_BUTTONS; i++)
	{
		maFields.Add(apFields[i]);
	}
	return pGrid;
}


//////////////////////////////////////////////////////////////////////////
//
// The name and keys of the mapping chosen; none for a new one.
//
//////////////////////////////////////////////////////////////////////////
void CSNESMappingDialog::MappingChosen(void)
{
	int32	iSelected;

	iSelected = mpMappings->GetSelected();
	if ((iSelected < 0) || ((size)iSelected + 1 >= mpcKeys->NumMappings()))
	{
		Fill(HCP65_SNES_KEYS_NONE);
	}
	else
	{
		Fill((size)iSelected + 1);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// uiMapping's name and keys in the fields; nothing for none.
//
//////////////////////////////////////////////////////////////////////////
void CSNESMappingDialog::Fill(size uiMapping)
{
	int		i;

	mpName->SetText((uiMapping == HCP65_SNES_KEYS_NONE) ? "" : mpcKeys->GetName(uiMapping));
	for (i = 0; i < HCP65_SNES_KEYS_BUTTONS; i++)
	{
		maFields.Get(i)->SetText((uiMapping == HCP65_SNES_KEYS_NONE) ? "" : mpcKeys->GetKey(uiMapping, i));
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The picture shows the field with the focus and the buttons whose keys
// are held, looked at again every SNES_MAPPING_FRAME_MS.
//
//////////////////////////////////////////////////////////////////////////
void CSNESMappingDialog::Tick(void)
{
	Ptr<CWindow>		pWindow;
	Ptr<CComponent>		pFocussed;
	Ptr<CSNESKeyField>	pField;
	CComponentInput*	pcInput;
	const char*			szKey;
	uint16				uiPressed;
	int					iFocused;
	int					i;

	pWindow = mpDialog->GetWindow();
	pcInput = pWindow->GetComponentInput();
	pFocussed = pWindow->GetFocus()->GetFocussedComponent();
	uiPressed = 0;
	iFocused = -1;
	for (i = 0; i < HCP65_SNES_KEYS_BUTTONS; i++)
	{
		pField = maFields.Get(i);
		szKey = pField->GetText();
		if (pcInput && szKey && (szKey[0] != '\0') && pWindow->GetFocus()->IsActive() && pcInput->IsSourceDown(szKey))
		{
			uiPressed |= (uint16)(1 << i);
		}
		if ((CComponent*)&pField == &pFocussed)
		{
			iFocused = i;
		}
	}
	mpController->SetPressed(uiPressed);
	mpController->SetFocused(iFocused);
	mpCanvas->Invalidate();
	pWindow->RequestTick(SNES_MAPPING_FRAME_MS);
}


//////////////////////////////////////////////////////////////////////////
//
// True if OK was pressed.
//
//////////////////////////////////////////////////////////////////////////
bool CSNESMappingDialog::Show(void)
{
	mpDialog->GetWindow()->GetFocus()->SetFocus(mpName);
	return mpDialog->Show(720);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
const char* CSNESMappingDialog::GetMappingName(void)
{
	return mpName->GetText();
}


//////////////////////////////////////////////////////////////////////////
//
// The key chosen for iButton (in the order a controller sends them), ""
// for none.
//
//////////////////////////////////////////////////////////////////////////
const char* CSNESMappingDialog::GetKey(int iButton)
{
	if ((iButton < 0) || (iButton >= HCP65_SNES_KEYS_BUTTONS))
	{
		return "";
	}
	return maFields.Get(iButton)->GetText();
}
