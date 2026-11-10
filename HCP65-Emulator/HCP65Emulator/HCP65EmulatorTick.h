#ifndef __HCP65_EMULATOR_TICK_H__
#define __HCP65_EMULATOR_TICK_H__
#include "WindowLib/WindowTick.h"


class CHCP65;
class CHCP65Audio;
class CHCP65SoundVoice;
class CHCP65EmulatorRunner;


//The HCP65 runs on CHCP65EmulatorRunner's thread, not in the ticks, so that it keeps running while a dialog is shown over
//the window, which stops them.  What the HCP65's sound board played is a voice in the window's sound mixer, which plays
//it on a thread of its own for the same reason.  The tick stops the window when the runner has finished, as the
//debugger said to close, and after HCP65_EMULATOR_SECONDS.  HCP65_EMULATOR_TRACE's report reads the HCP65 while it runs,
//which is only for printing.
class CHCP65EmulatorTick : public CWindowTick
{
CONSTRUCTABLE(CHCP65EmulatorTick);
DESTRUCTABLE(CHCP65EmulatorTick);
protected:
	CHCP65*					mpcHCP65;
	CHCP65Audio*			mpcAudio;
	Ptr<CHCP65SoundVoice>	mpSoundVoice;
	CHCP65EmulatorRunner*	mpcRunner;
	int64					miStopTime;				//HCP65_EMULATOR_SECONDS (environment): the window stops after it, or -1.
	int64					miReportTime;			//HCP65_EMULATOR_TRACE (environment): when the next report is printed, or -1.

public:
	void	Init(CHCP65* pcHCP65, CHCP65Audio* pcAudio, Ptr<CHCP65SoundVoice> pSoundVoice, CHCP65EmulatorRunner* pcRunner);
	void	Class(void) override;
	void	Free(void) override;
	void	Tick(Ptr<CWindow> pWindow, int64 iUpdateTimeInMillieconds, int64 iTotalTimeInMillieconds) override;
	void	SetAudio(CHCP65Audio* pcAudio);

protected:
	void	Report(Ptr<CWindow> pWindow, int64 iTotalTimeInMillieconds);
};


#endif // __HCP65_EMULATOR_TICK_H__
