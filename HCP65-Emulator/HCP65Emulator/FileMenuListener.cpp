#include "HCP65EmulatorRunner.h"
#include "FileMenuListener.h"


//////////////////////////////////////////////////////////////////////////
//
// Adds the menu's items to pFileMenu.
//
//////////////////////////////////////////////////////////////////////////
void CFileMenuListener::Init(Ptr<CWindow> pWindow, Ptr<CMenuContainer> pFileMenu, CHCP65EmulatorRunner* pcRunner)
{
	mpcWindow = &pWindow;
	mpcRunner = pcRunner;
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
	mpcRunner = NULL;
	mpcWindow = NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// Called between the window's ticks.  The HCP65 runs on its own thread,
// which the runner pauses to reset it.
//
//////////////////////////////////////////////////////////////////////////
void CFileMenuListener::MenuItemClicked(CMenuItem* pcItem)
{
	if (mpcReset == pcItem)
	{
		mpcRunner->Reset();
	}
	else if (mpcExit == pcItem)
	{
		mpcWindow->Stop();
	}
}
