#include <stdio.h>
#include <string.h>
#include "StandardLib/ClassDefines.h"
#include "StandardLib/Objects.h"
#include "BoardConfigurationDialog.h"


//////////////////////////////////////////////////////////////////////////
//
// The mainboard isn't held.
//
//////////////////////////////////////////////////////////////////////////
void CBoardConfigurationDialog::Init(Ptr<CWindow> pOwner, SDialogLook* psLook, CHCP65Mainboard* pcMainboard)
{
	int		i;

	PreInit();

	maCombos.Init();
	mpcMainboard = pcMainboard;
	mpDialog = OMalloc<CDialog>(pOwner, psLook, "Board Configuration");
	for (i = 0; i < HCP65_NUM_DEVICE_SLOTS; i++)
	{
		if (CHCP65Mainboard::HasSocket(pcMainboard->GetType(), 0, i))
		{
			AddSocket(i);
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
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBoardConfigurationDialog::Free(void)
{
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
	size		i;
	int			iSocket;

	uiCombo = 0;
	for (iSocket = 0; iSocket < HCP65_NUM_DEVICE_SLOTS; iSocket++)
	{
		aszTypes[iSocket] = NULL;
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
