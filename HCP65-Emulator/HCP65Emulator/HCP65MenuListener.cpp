#include "HCP65Emulator.h"
#include "HCP65MenuListener.h"


//////////////////////////////////////////////////////////////////////////
//
// Adds the menu's items to pMenu.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65MenuListener::Init(Ptr<CWindow> pWindow, Ptr<CMenuContainer> pMenu, CHCP65EmulatorApp* pcApp)
{
	mpcWindow = &pWindow;
	mpcApp = pcApp;
	mpcBoardConfiguration = &pMenu->AddItem("Board Configuration");
	mpcSNESMappings = &pMenu->AddItem("SNES Key Mappings");
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65MenuListener::Kill(void)
{
	mpcBoardConfiguration = NULL;
	mpcSNESMappings = NULL;
	mpcApp = NULL;
	mpcWindow = NULL;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65MenuListener::MenuItemClicked(CMenuItem* pcItem)
{
	if (mpcBoardConfiguration == pcItem)
	{
		mpcWindow->Post(mpcApp, &CHCP65EmulatorApp::ShowBoardConfiguration);
	}
	else if (mpcSNESMappings == pcItem)
	{
		mpcWindow->Post(mpcApp, &CHCP65EmulatorApp::ShowSNESMappings);
	}
}
