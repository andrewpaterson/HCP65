#ifndef __FILE_MENU_LISTENER_H__
#define __FILE_MENU_LISTENER_H__
#include "WindowLib/Window.h"
#include "WindowLib/MenuContainer.h"


class CHCP65;
class CHCP65EmulatorDebugger;


//The File menu.  Reset is the mainboard's system reset, which goes through the debugger if there is one so that it can
//tell its client.  Exit stops the window's ticks, which closes it.  MenuItemClicked is on the menu bar's item click
//event.  The window holds the menu; the listener only points to them.
class CFileMenuListener
{
protected:
	CWindow*				mpcWindow;
	CHCP65*					mpcHCP65;
	CHCP65EmulatorDebugger*	mpcDebugger;	//NULL if the emulator isn't debuggable.
	CMenuItem*				mpcReset;
	CMenuItem*				mpcExit;

public:
	void	Init(Ptr<CWindow> pWindow, Ptr<CMenuContainer> pFileMenu, CHCP65* pcHCP65, CHCP65EmulatorDebugger* pcDebugger);
	void	Kill(void);

	void	MenuItemClicked(CMenuItem* pcItem);
};


#endif // __FILE_MENU_LISTENER_H__
