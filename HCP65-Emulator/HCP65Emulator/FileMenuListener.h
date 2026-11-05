#ifndef __FILE_MENU_LISTENER_H__
#define __FILE_MENU_LISTENER_H__
#include "WindowLib/Window.h"
#include "WindowLib/MenuContainer.h"


//The File menu.  Exit stops the window's ticks, which closes it.  MenuItemClicked is on the menu bar's item click
//event.  The window holds the menu; the listener only points to them.
class CFileMenuListener
{
protected:
	CWindow*		mpcWindow;
	CMenuItem*		mpcExit;

public:
	void	Init(Ptr<CWindow> pWindow, Ptr<CMenuContainer> pFileMenu);
	void	Kill(void);

	void	MenuItemClicked(CMenuItem* pcItem);
};


#endif // __FILE_MENU_LISTENER_H__
