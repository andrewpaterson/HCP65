#ifndef __TESTER_APP_H__
#define __TESTER_APP_H__
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "WindowLib/Window.h"
#include "WindowLib/NativeWindowFactory.h"


//The board tester: its window.  main makes it, adds it to the root, runs it and removes it from the root.
class CTesterApp : public CObject
{
CONSTRUCTABLE(CTesterApp);
DESTRUCTABLE(CTesterApp);
protected:
	Ptr<CWindow>	mpWindow;

public:
	void	Init(CNativeWindowFactory* pcNativeFactory);
	void	Class(void) override;
	void	Free(void) override;

	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	bool	Run(void);
};


#endif // __TESTER_APP_H__

