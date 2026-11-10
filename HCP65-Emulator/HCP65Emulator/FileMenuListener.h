#ifndef __FILE_MENU_LISTENER_H__
#define __FILE_MENU_LISTENER_H__
#include "WindowLib/Window.h"
#include "WindowLib/MenuContainer.h"


class CHCP65EmulatorRunner;


//The File menu.  Reset is the mainboard's system reset, which the runner does between the HCP65's runs, through the
//debugger if there is one so that it can tell its client.  Exit stops the window's ticks, which closes it.
//MenuItemClicked is on the menu bar's item click event.  The window holds the menu; the listener only points to them.
class CFileMenuListener
{
protected:
	CWindow*				mpcWindow;
	CHCP65EmulatorRunner*	mpcRunner;
	CMenuItem*				mpcReset;
	CMenuItem*				mpcExit;

public:
	void	Init(Ptr<CWindow> pWindow, Ptr<CMenuContainer> pFileMenu, CHCP65EmulatorRunner* pcRunner);
	void	Kill(void);

	void	MenuItemClicked(CMenuItem* pcItem);
};


#endif // __FILE_MENU_LISTENER_H__
