#include <stdio.h>
#include <string.h>
#include "BaseLib/Timer.h"
#include "StandardLib/ClassDefines.h"
#include "StandardLib/Objects.h"
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
	size	i;

	PreInit();

	maCombos.Init();
	mpcMainboard = pcMainboard;
	mpScene = pScene;
	mbPainted = false;
	miStartTime = -1;
	mpDialog = OMalloc<CDialog>(pOwner, psLook, "Board Configuration");
	if (pScene.IsNotNull())
	{
		mpSceneCanvas = OMalloc<CCanvas>(mpDialog->GetWindow(), pScene);
		mpSceneCanvas->SetDesiredSize(BOARD_SCENE_WIDTH, BOARD_SCENE_HEIGHT);
		mpSceneCanvas->SetAlignment(CSH_Center, CSV_Center);
		mpDialog->AddEditor(NULL, mpSceneCanvas);
		mpDialog->SetTicker(this, &CBoardConfigurationDialog::TickScene);
	}
	for (i = 0; i < sizeof(gaiBoardConfigurationSockets) / sizeof(int); i++)
	{
		if (CHCP65Mainboard::HasSocket(pcMainboard->GetType(), 0, gaiBoardConfigurationSockets[i]))
		{
			AddSocket(gaiBoardConfigurationSockets[i]);
		}
	}

	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBoardConfigurationDialog::Class(void)
{
	M_Pointer(mpDialog);
	M_Embedded(maCombos);
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
	if (mpDialog.IsNotNull())
	{
		mpDialog->ClearTicker();
	}
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
	maCombos.Add(mpDialog->AddComboBox(szLabel, aszItems, uiNumItems, iSelected));
}


//////////////////////////////////////////////////////////////////////////
//
// Shows the dialog modally (from a tick, see CWindow::ShowModal).  True if
// OK closed it.
//
//////////////////////////////////////////////////////////////////////////
bool CBoardConfigurationDialog::Show(void)
{
	return mpDialog->Show();
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
