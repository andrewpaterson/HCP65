#ifndef __HCP65_EMULATOR_TICK_H__
#define __HCP65_EMULATOR_TICK_H__
#include "WindowLib/WindowTick.h"


//The most of the HCP65's time one tick runs (when the host falls behind the HCP65 slows down rather than catching up).
#define HCP65_EMULATOR_MAX_TICK_MS		100


class CHCP65;
class CHCP65Audio;
class CHCP65SoundVoice;
class CHCP65EmulatorDebugger;


//Each tick runs the HCP65 for the time since the last (at its clock rate) and paints the window.  What its sound board
//played is a voice in the window's sound mixer, which the window mixes after the tick.  With a debugger the debugger runs
//it, and the HCP65's time stands still while the debugger has it stopped.
class CHCP65EmulatorTick : public CWindowTick
{
CONSTRUCTABLE(CHCP65EmulatorTick);
DESTRUCTABLE(CHCP65EmulatorTick);
protected:
	CHCP65*			mpcHCP65;
	CHCP65Audio*	mpcAudio;
	Ptr<CHCP65SoundVoice>	mpSoundVoice;
	CHCP65EmulatorDebugger*	mpcDebugger;		//NULL when the emulator isn't being debugged.
	int64			miStartTime;			//The window's time when the HCP65 started, -1 before the first tick.
	uint64			muiStartCycles;			//The HCP65's cycles then, less those it didn't run when the host fell behind.
	int64			miStopTime;				//HCP65_EMULATOR_SECONDS (environment): the window stops after it, or -1.
	int64			miReportTime;			//HCP65_EMULATOR_TRACE (environment): when the next report is printed, or -1.

public:
	void	Init(CHCP65* pcHCP65, CHCP65Audio* pcAudio, Ptr<CHCP65SoundVoice> pSoundVoice, CHCP65EmulatorDebugger* pcDebugger);
	void	Class(void) override;
	void	Free(void) override;
	void	Tick(Ptr<CWindow> pWindow, int64 iUpdateTimeInMillieconds, int64 iTotalTimeInMillieconds) override;
	void	SetAudio(CHCP65Audio* pcAudio);

protected:
	void	Run(int64 iTotalTimeInMillieconds);
	bool	RunDebugged(int64 iTotalTimeInMillieconds);
	uint64	CyclesDue(int64 iTotalTimeInMillieconds);
	void	Report(Ptr<CWindow> pWindow, int64 iTotalTimeInMillieconds);
};


#endif // __HCP65_EMULATOR_TICK_H__
