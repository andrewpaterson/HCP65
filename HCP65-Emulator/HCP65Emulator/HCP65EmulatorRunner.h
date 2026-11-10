#ifndef __HCP65_EMULATOR_RUNNER_H__
#define __HCP65_EMULATOR_RUNNER_H__
#include "BaseLib/ConditionVariable.h"
#include "BaseLib/Mutex.h"
#include "BaseLib/NativeThread.h"
#include "BaseLib/PrimitiveTypes.h"


//The most of the HCP65's time it runs at once (when the host falls behind the HCP65 slows down rather than catching up).
#define HCP65_EMULATOR_MAX_RUN_MS		100


class CHCP65;
class CHCP65EmulatorDebugger;


//Runs the HCP65 on a thread of its own, so that it keeps running whatever the window's thread is doing, a dialog shown
//modally over the window included.  It runs to where its clock should be by now, at most HCP65_EMULATOR_MAX_RUN_MS of
//its time at once, and sleeps when that's done.  With a debugger the debugger runs it, and the HCP65's time stands still
//while the debugger has it stopped.
//
//Nothing else touches the HCP65, its boards or the debugger while the thread runs, except between Pause and Resume, when
//it's waiting: Pause returns once it is, and the HCP65's clock starts again from where it was when Resume lets it go.  Pauses
//can nest.  Reset is a pause of its own.  IsFinished says the debugger told the emulator to close, when the thread has
//stopped and the window should too.  Stop stops the thread for good, before the HCP65 goes.
class CHCP65EmulatorRunner
{
protected:
	CHCP65*					mpcHCP65;
	CHCP65EmulatorDebugger*	mpcDebugger;		//Runs the HCP65 when it's enabled.
	CNativeThread			mcThread;
	CMutex					mcMutex;
	CConditionVariable		mcChanged;			//Woken when any of the values below changes.
	int						miPauses;			//Pauses not yet resumed.  Under mcMutex, as are the next three.
	bool					mbIdle;				//The thread isn't running the HCP65: it's paused, stopped or not started.
	bool					mbStop;
	bool					mbFinished;
	int64					miStartTime;		//The thread's: GetDebugTickCount when the clock started, -1 to start it again.
	uint64					muiStartCycles;		//The HCP65's cycles then, less those it didn't run when the host fell behind.

public:
	void	Init(CHCP65* pcHCP65, CHCP65EmulatorDebugger* pcDebugger);
	void	Kill(void);

	bool	Start(void);
	void	Stop(void);

	void	Pause(void);
	void	Resume(void);
	void	Reset(void);
	bool	IsFinished(void);

protected:
	static void	ThreadFunction(void* pvRunner);
	void		Run(void);
	bool		WaitWhilePaused(void);
	bool		RunDue(void);
	uint64		CyclesDue(int64 iNow);
};


#endif // __HCP65_EMULATOR_RUNNER_H__
