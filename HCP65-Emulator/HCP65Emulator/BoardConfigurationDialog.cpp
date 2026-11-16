#include <stdio.h>
#include <string.h>
#include "BaseLib/Timer.h"
#include "StandardLib/ClassDefines.h"
#include "StandardLib/Objects.h"
#include "WindowLib/ClearCanvasDraw.h"
#include "WindowLib/Label.h"
#include "BoardConfigurationDialog.h"


//The order of the rows, which is the order of the sockets along the Small Mainboard: 3 and 4, then 18, 19, 17 and 16.  The
//sockets it hasn't got are skipped.  Any socket a mainboard can have must be here.
static const int gaiBoardConfigurationSockets[] = { 2, 3, 4, 5, 6, 7, 18, 19, 17, 16 };


//////////////////////////////////////////////////////////////////////////
//
// The mainboard isn't held.  pScene can be NULL, for no picture.
//
//////////////////////////////////////////////////////////////////////////
void CBoardConfigurationDialog::Init(Ptr<CWindow> pOwner, SDialogLook* psLook, CHCP65Mainboard* pcMainboard, Ptr<CBoardSceneDraw> pScene)
{
	Ptr<CGridContainer>	pColumns;
	size				i;

	PreInit();

	maCombos.Init();
	maiComboSockets.Init();
	maiComboRows.Init();
	maBreakouts.Init();
	maiBreakoutsShown.Init();
	maBreakoutBoxes.Init();
	mpcMainboard = pcMainboard;
	mpScene = pScene;
	mbPainted = false;
	miStartTime = -1;
	mpDialog = OMalloc<CDialog>(pOwner, psLook, "Board Configuration");

	mpRows = OMalloc<CGridContainer>(mpDialog->GetWindow());
	mpRows->SetDesiredSize(BOARD_SCENE_WIDTH, -1);
	mpRows->SetAlignment(CSH_Center, CSV_Top);
	miNumRows = 0;

	pColumns = OMalloc<CGridContainer>(mpDialog->GetWindow());
	pColumns->SetAlignment(CSH_Stretch, CSV_Top);
	AddSideSpace(pColumns, 0);
	pColumns->AddComponent(mpRows, 1, 0);
	AddSideSpace(pColumns, 2);
	mpDialog->AddEditor(NULL, pColumns);

	if (pScene.IsNotNull())
	{
		mpSceneCanvas = OMalloc<CCanvas>(mpDialog->GetWindow(), pScene);
		mpSceneCanvas->SetDesiredSize(BOARD_SCENE_WIDTH, BOARD_SCENE_HEIGHT);
		mpSceneCanvas->SetAlignment(CSH_Center, CSV_Center);
		mpRows->AddComponent(mpSceneCanvas, 0, miNumRows, 2, 1, 1, 0);
		miNumRows++;
		mpDialog->SetTicker(this, &CBoardConfigurationDialog::TickScene);
	}
	for (i = 0; i < sizeof(gaiBoardConfigurationSockets) / sizeof(int); i++)
	{
		if (CHCP65Mainboard::HasSocket(pcMainboard->GetType(), 0, gaiBoardConfigurationSockets[i]))
		{
			AddSocket(gaiBoardConfigurationSockets[i]);
		}
	}
	ShowBreakouts();

	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBoardConfigurationDialog::Class(void)
{
	M_Pointer(mpDialog);
	M_Pointer(mpRows);
	U_Int32(miNumRows);
	M_Embedded(maCombos);
	U_Unknown(CArrayInt, maiComboSockets);
	U_Unknown(CArrayInt, maiComboRows);
	M_Embedded(maBreakouts);
	U_Unknown(CArrayInt, maiBreakoutsShown);
	M_Embedded(maBreakoutBoxes);
	U_Pointer(mpcMainboard);
	M_Pointer(mpSceneCanvas);
	M_Pointer(mpScene);
	U_Bool(mbPainted);
	U_Int64(miStartTime);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBoardConfigurationDialog::Free(void)
{
	size	i;

	if (mpDialog.IsNotNull())
	{
		mpDialog->ClearTicker();
	}
	for (i = 0; i < maCombos.NumElements(); i++)
	{
		maCombos.Get(i)->GetSelectionEvent()->Remove(this);
	}
	for (i = 0; i < maBreakoutBoxes.NumElements(); i++)
	{
		maBreakoutBoxes.Get(i)->GetChangeEvent()->Remove(this);
	}
	maiBreakoutsShown.Kill();
	maiComboRows.Kill();
	maiComboSockets.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CBoardConfigurationDialog::Save(CObjectWriter* pcFile)
{
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CBoardConfigurationDialog::Load(CObjectReader* pcFile)
{
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// The dialog's ticker.  Nothing reaches the screen while a model is being
// read, so the first tick paints the dialog and the window's messages are
// handled before the next tick.  After that each tick loads a model that's
// wanted and isn't yet, or else moves the picture on.  So what's chosen
// appears in its socket as soon as it can be.
//
//////////////////////////////////////////////////////////////////////////
void CBoardConfigurationDialog::TickScene(void)
{
	const char*	aszTypes[HCP65_NUM_DEVICE_SLOTS];
	int64		iNow;

	if (!mbPainted)
	{
		mbPainted = true;
		mpDialog->GetWindow()->Paint();
		mpDialog->GetWindow()->RequestTick(BOARD_CONFIGURATION_FRAME_MS);
		return;
	}

	GetTypes(aszTypes);
	if (mpScene->LoadNext(aszTypes))
	{
		mpScene->SetBoards(aszTypes);
		mpSceneCanvas->Invalidate();
		mpDialog->GetWindow()->Paint();
		mpDialog->GetWindow()->RequestTick(1);
		return;
	}

	iNow = GetDebugTickCount();
	if (miStartTime < 0)
	{
		miStartTime = iNow;
	}
	mpScene->SetBoards(aszTypes);
	mpScene->SetYaw((float32)((iNow - miStartTime) % BOARD_CONFIGURATION_TURN_MS) * 6.28318530718f / BOARD_CONFIGURATION_TURN_MS);
	mpSceneCanvas->Invalidate();
	mpDialog->GetWindow()->RequestTick(BOARD_CONFIGURATION_FRAME_MS);
}


//////////////////////////////////////////////////////////////////////////
//
// A row for iSocket: Empty and the boards that fit it, with the one in it
// chosen.
//
//////////////////////////////////////////////////////////////////////////
void CBoardConfigurationDialog::AddSocket(int iSocket)
{
	CWindowTheme*	pcTheme;
	Ptr<CLabel>		pLabel;
	Ptr<CComboBox>	pCombo;
	const char*	aszItems[HCP65_NUM_DEVICE_SLOTS];
	const char*	szName;
	const char*	szCurrent;
	char		szLabel[32];
	size		uiNumItems;
	size		i;
	int			iSelected;

	szCurrent = mpcMainboard->GetBoardType(iSocket);
	aszItems[0] = BOARD_CONFIGURATION_EMPTY;
	uiNumItems = 1;
	iSelected = 0;
	for (i = 0; i < CHCP65Mainboard::NumBoardTypes(); i++)
	{
		szName = CHCP65Mainboard::GetBoardTypeName(i);
		if (mpcMainboard->CanHold(iSocket, szName))
		{
			if (szCurrent && (strcmp(szCurrent, szName) == 0))
			{
				iSelected = (int)uiNumItems;
			}
			aszItems[uiNumItems] = szName;
			uiNumItems++;
		}
	}

	snprintf(szLabel, sizeof(szLabel), "Socket %d (%dKB)", iSocket, (int)(CHCP65Mainboard::GetSocketSize(iSocket) / 1024));
	pcTheme = mpDialog->GetLook()->pcTheme;

	if (miNumRows > 0)
	{
		AddRowGap();
	}

	pLabel = OMalloc<CLabel>(mpDialog->GetWindow(), pcTheme->GetFont(), szLabel);
	pLabel->SetDesiredSize(pLabel->GetRequiredSize().x + pcTheme->GetMetric(TM_DialogLabelGap), -1);
	pLabel->SetAlignment(CSH_Left, CSV_Center);
	mpRows->AddComponent(pLabel, 0, miNumRows);

	pCombo = OMalloc<CComboBox>(mpDialog->GetWindow(), pcTheme);
	for (i = 0; i < uiNumItems; i++)
	{
		pCombo->AddItem(aszItems[i]);
	}
	pCombo->Select(iSelected);
	pCombo->SetAlignment(CSH_Stretch, CSV_Center);
	pCombo->GetSelectionEvent()->Add(this, &CBoardConfigurationDialog::ShowBreakouts);
	mpRows->AddComponent(pCombo, 1, miNumRows, 1, 1, 1, 0);

	maCombos.Add(pCombo);
	maiComboSockets.Add(iSocket);
	maiComboRows.Add(miNumRows);
	AddBreakouts(iSocket);
	miNumRows++;
}


//////////////////////////////////////////////////////////////////////////
//
// The check boxes of iSocket's VIA Breakouts, ticked where the VIA Board
// in it now has them, or all of them if there isn't one.  They aren't in
// the rows until ShowBreakouts puts them there.
//
//////////////////////////////////////////////////////////////////////////
void CBoardConfigurationDialog::AddBreakouts(int iSocket)
{
	CWindowTheme*		pcTheme;
	Ptr<CGridContainer>	pGrid;
	Ptr<CCheckBox>		pBox;
	Ptr<CCanvas>		pGap;
	CHCP65VIABoard*		pcVIA;
	const char*			szCurrent;
	char				szCaption[8];
	uint8				uiFitted;
	int					i;

	uiFitted = BOARD_CONFIGURATION_ALL_BREAKOUTS;
	szCurrent = mpcMainboard->GetBoardType(iSocket);
	if (szCurrent && (strcmp(szCurrent, HCP65_VIA_BOARD_NAME) == 0))
	{
		pcVIA = (CHCP65VIABoard*)mpcMainboard->GetBoard(iSocket);
		uiFitted = 0;
		for (i = 0; i < HCP65_VIA_NUM_BREAKOUTS; i++)
		{
			if (pcVIA->HasBreakout(i))
			{
				uiFitted |= (uint8)(1 << i);
			}
		}
		if (uiFitted == 0)
		{
			uiFitted = BOARD_CONFIGURATION_ALL_BREAKOUTS;
		}
	}

	pcTheme = mpDialog->GetLook()->pcTheme;
	pGrid = OMalloc<CGridContainer>(mpDialog->GetWindow());
	pGrid->SetAlignment(CSH_Left, CSV_Center);
	for (i = 0; i < HCP65_VIA_NUM_BREAKOUTS; i++)
	{
		pGap = OMalloc<CCanvas>(mpDialog->GetWindow(), OMalloc<CClearCanvasDraw>(pcTheme->GetColour(TC_DialogBackground)));
		pGap->SetDesiredSize(pcTheme->GetMetric(TM_DialogLabelGap), 1);
		pGrid->AddComponent(pGap, i * 2, 0);

		snprintf(szCaption, sizeof(szCaption), "%s/%s", CHCP65VIABoard::GetVIAName(i * 2), CHCP65VIABoard::GetVIAName(i * 2 + 1));
		pBox = OMalloc<CCheckBox>(mpDialog->GetWindow(), pcTheme, szCaption, (uiFitted & (1 << i)) != 0);
		pBox->SetAlignment(CSH_Left, CSV_Center);
		pBox->GetChangeEvent()->Add(this, &CBoardConfigurationDialog::BreakoutChanged);
		pGrid->AddComponent(pBox, i * 2 + 1, 0);
		maBreakoutBoxes.Add(pBox);
	}
	maBreakouts.Add(pGrid);
	maiBreakoutsShown.Add(0);
}


//////////////////////////////////////////////////////////////////////////
//
// Each combo box's breakouts are beside it while it has the VIA Board.
//
//////////////////////////////////////////////////////////////////////////
void CBoardConfigurationDialog::ShowBreakouts(void)
{
	size	i;
	bool	bWanted;
	bool	bShown;
	bool	bChanged;

	bChanged = false;
	for (i = 0; i < maCombos.NumElements(); i++)
	{
		bWanted = strcmp(maCombos.Get(i)->GetSelectedText(), HCP65_VIA_BOARD_NAME) == 0;
		bShown = maiBreakoutsShown.GetValue(i) != 0;
		if (bWanted && !bShown)
		{
			mpRows->AddComponent(maBreakouts.Get(i), 2, maiComboRows.GetValue(i));
			bChanged = true;
		}
		else if (!bWanted && bShown)
		{
			mpRows->RemoveComponent(maBreakouts.Get(i));
			bChanged = true;
		}
		maiBreakoutsShown.SetValue(i, bWanted ? 1 : 0);
	}
	if (bChanged)
	{
		mpDialog->GetWindow()->InvalidateLayout();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A VIA Board has at least one breakout: pcBox, just unticked, is ticked
// again if it was the last.
//
//////////////////////////////////////////////////////////////////////////
void CBoardConfigurationDialog::BreakoutChanged(CCheckBox* pcBox)
{
	size	i;
	size	j;
	bool	bAny;

	for (i = 0; i < maCombos.NumElements(); i++)
	{
		bAny = false;
		for (j = 0; j < HCP65_VIA_NUM_BREAKOUTS; j++)
		{
			bAny |= maBreakoutBoxes.Get(i * HCP65_VIA_NUM_BREAKOUTS + j)->IsTicked();
		}
		if (!bAny)
		{
			pcBox->SetTicked(true);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The VIA Breakouts ticked for iSocket's VIA Board: bit n for breakout n.
// Every one for a socket without a row.
//
//////////////////////////////////////////////////////////////////////////
uint8 CBoardConfigurationDialog::GetBreakouts(int iSocket)
{
	uint8	uiFitted;
	size	i;
	size	j;

	for (i = 0; i < maCombos.NumElements(); i++)
	{
		if (maiComboSockets.GetValue(i) == iSocket)
		{
			uiFitted = 0;
			for (j = 0; j < HCP65_VIA_NUM_BREAKOUTS; j++)
			{
				if (maBreakoutBoxes.Get(i * HCP65_VIA_NUM_BREAKOUTS + j)->IsTicked())
				{
					uiFitted |= (uint8)(1 << j);
				}
			}
			return uiFitted;
		}
	}
	return BOARD_CONFIGURATION_ALL_BREAKOUTS;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBoardConfigurationDialog::AddRowGap(void)
{
	CWindowTheme*	pcTheme;
	Ptr<CCanvas>	pGap;

	pcTheme = mpDialog->GetLook()->pcTheme;
	pGap = OMalloc<CCanvas>(mpDialog->GetWindow(), OMalloc<CClearCanvasDraw>(pcTheme->GetColour(TC_DialogBackground)));
	pGap->SetDesiredSize(1, pcTheme->GetMetric(TM_DialogRowGap));
	mpRows->AddComponent(pGap, 0, miNumRows);
	miNumRows++;
}


//////////////////////////////////////////////////////////////////////////
//
// A column that takes its share of whatever the middle one doesn't need.
//
//////////////////////////////////////////////////////////////////////////
void CBoardConfigurationDialog::AddSideSpace(Ptr<CGridContainer> pGrid, int iColumn)
{
	Ptr<CCanvas>	pSpace;

	pSpace = OMalloc<CCanvas>(mpDialog->GetWindow(), OMalloc<CClearCanvasDraw>(mpDialog->GetLook()->pcTheme->GetColour(TC_DialogBackground)));
	pSpace->SetDesiredSize(1, 1);
	pGrid->AddComponent(pSpace, iColumn, 0, 1, 1, 1, 0);
}


//////////////////////////////////////////////////////////////////////////
//
// Shows the dialog modally (from a tick, see CWindow::ShowModal).  True if
// OK closed it.
//
//////////////////////////////////////////////////////////////////////////
bool CBoardConfigurationDialog::Show(void)
{
	//The rows are inside a grid, which the dialog can't see to give the first of them the focus.
	if (maCombos.NumElements() > 0)
	{
		mpDialog->GetWindow()->GetFocus()->SetFocus(maCombos.Get(0));
	}
	return mpDialog->Show(BOARD_SCENE_WIDTH + (mpDialog->GetLook()->pcTheme->GetMetric(TM_DialogInset) + BOARD_CONFIGURATION_SIDE_SPACE) * 2);
}


//////////////////////////////////////////////////////////////////////////
//
// What was chosen for each of HCP65_NUM_DEVICE_SLOTS sockets, NULL for
// empty.  The names are CHCP65Mainboard's own, so they outlast the dialog.
//
//////////////////////////////////////////////////////////////////////////
void CBoardConfigurationDialog::GetTypes(const char** aszTypes)
{
	const char*	szChosen;
	size		uiCombo;
	size		uiRow;
	size		i;
	int			iSocket;

	for (iSocket = 0; iSocket < HCP65_NUM_DEVICE_SLOTS; iSocket++)
	{
		aszTypes[iSocket] = NULL;
	}

	uiCombo = 0;
	for (uiRow = 0; uiRow < sizeof(gaiBoardConfigurationSockets) / sizeof(int); uiRow++)
	{
		iSocket = gaiBoardConfigurationSockets[uiRow];
		if (CHCP65Mainboard::HasSocket(mpcMainboard->GetType(), 0, iSocket))
		{
			szChosen = maCombos.Get(uiCombo)->GetSelectedText();
			uiCombo++;
			for (i = 0; i < CHCP65Mainboard::NumBoardTypes(); i++)
			{
				if (strcmp(szChosen, CHCP65Mainboard::GetBoardTypeName(i)) == 0)
				{
					aszTypes[iSocket] = CHCP65Mainboard::GetBoardTypeName(i);
				}
			}
		}
	}
}
