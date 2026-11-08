#ifndef __HCP65_MENU_LISTENER_H__
#define __HCP65_MENU_LISTENER_H__
#include "WindowLib/Window.h"
#include "WindowLib/MenuContainer.h"


class CHCP65EmulatorApp;


//The HCP65 menu.  Board Configuration has the app show its dialog at the next tick: a dialog can't be shown from the
//input that asks for it (see CWindow::ShowModal).  MenuItemClicked is on the menu bar's item click event.  The window
//holds the menu; the listener only points to them.
class CHCP65MenuListener
{
protected:
	CWindow*			mpcWindow;
	CHCP65EmulatorApp*	mpcApp;
	CMenuItem*			mpcBoardConfiguration;

public:
	void	Init(Ptr<CWindow> pWindow, Ptr<CMenuContainer> pMenu, CHCP65EmulatorApp* pcApp);
	void	Kill(void);

	void	MenuItemClicked(CMenuItem* pcItem);
};


#endif // __HCP65_MENU_LISTENER_H__
