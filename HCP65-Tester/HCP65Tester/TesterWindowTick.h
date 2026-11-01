#ifndef __TESTER_WINDOW_TICK_H__
#define __TESTER_WINDOW_TICK_H__
#include "WindowLib/WindowTick.h"


//Paints the tester's window each tick.  The window is empty.
class CTesterWindowTick : public CWindowTick
{
CONSTRUCTABLE(CTesterWindowTick);
DESTRUCTABLE(CTesterWindowTick);
public:
	void	Init(void);
	void	Tick(Ptr<CWindow> pWindow, int64 iUpdateTimeInMillieconds, int64 iTotalTimeInMillieconds) override;
};


#endif // __TESTER_WINDOW_TICK_H__
