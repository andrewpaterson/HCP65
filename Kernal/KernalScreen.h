#ifndef __KERNAL_SCREEN_H__
#define __KERNAL_SCREEN_H__
#include "BaseLib/PrimitiveTypes.h"
#include "KernalDriver.h"


//The screen (COP 14, UserLib/HCP65User.h's KernalScreen*): the picture, which is the kernal's.  It is the Video
//Board's, which the kernal has as the console does (KERNAL_CONSOLE_OWNER), so no program can open the board while the
//screen or the console's text is on it.  It shows the console's text until a process sets a mode: then the picture is
//that process's (the console draws nothing) until it sets text again or ends, when the console has its text back, or
//the board is turned off and let go if the console isn't on it.
//
//The Video Board's driver draws what is asked.  A blit's runs are copies in main memory (KernalMainCopy,
//KernalDriver.h), which a Blitter Board makes if there is one.
class CKernalScreen
{
protected:
	CKernalDriver*	mpcVideo;		//The Video Board's driver while a process has the picture.
	uint8			muiProcess;		//That process, 0 if none.

public:
	void	Init(void);

	uint16	Call(uint32 uiAddress);
	void	ProcessEnded(uint8 uiProcess);

protected:
	uint16	SetMode(uint16 uiMode);
	void	EndGraphics(void);
	uint16	SetColours(uint16 uiPalette, uint32 uiUserAddress);
};


extern CKernalScreen	gcKernalScreen;


#endif // __KERNAL_SCREEN_H__

