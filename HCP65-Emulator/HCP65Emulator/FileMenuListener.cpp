#include "HCP65Lib/HCP65.h"
#include "HCP65EmulatorDebugger.h"
#include "FileMenuListener.h"


//////////////////////////////////////////////////////////////////////////
//
// Adds the menu's items to pFileMenu.
//
//////////////////////////////////////////////////////////////////////////
void CFileMenuListener::Init(Ptr<CWindow> pWindow, Ptr<CMenuContainer> pFileMenu, CHCP65* pcHCP65, CHCP65EmulatorDebugger* pcDebugger)
{
	mpcWindow = &pWindow;
	mpcHCP65 = pcHCP65;
	mpcDebugger = pcDebugger;
	mpcReset = &pFileMenu->AddItem("Reset");
	pFileMenu->AddSeparator();
	mpcExit = &pFileMenu->AddItem("Exit");
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CFileMenuListener::Kill(void)
{
	mpcExit = NULL;
	mpcReset = NULL;
	mpcDebugger = NULL;
	mpcHCP65 = NULL;
	mpcWindow = NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// Called between the window's ticks, so never during the HCP65's Run.
//
//////////////////////////////////////////////////////////////////////////
void CFileMenuListener::MenuItemClicked(CMenuItem* pcItem)
{
	if (mpcReset == pcItem)
	{
		if (mpcDebugger)
		{
			mpcDebugger->Reset();
		}
		else
		{
			mpcHCP65->Reset();
		}
	}
	else if (mpcExit == pcItem)
	{
		mpcWindow->Stop();
	}
}
