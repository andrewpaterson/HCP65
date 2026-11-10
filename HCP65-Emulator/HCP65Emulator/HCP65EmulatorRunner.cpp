#include "BaseLib/Timer.h"
#include "HCP65Lib/HCP65.h"
#include "HCP65EmulatorDebugger.h"
#include "HCP65EmulatorRunner.h"


//How long the thread sleeps when there's nothing for the HCP65 to run yet.
#define HCP65_EMULATOR_IDLE_MS		1


//////////////////////////////////////////////////////////////////////////
//
// pcDebugger runs the HCP65 if it's enabled, which is known before Start.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65EmulatorRunner::Init(CHCP65* pcHCP65, CHCP65EmulatorDebugger* pcDebugger)
{
	mpcHCP65 = pcHCP65;
	mpcDebugger = pcDebugger;
	mcThread.Init();
	mcMutex.Init();
	mcChanged.Init();
	miPauses = 0;
	mbIdle = true;
	mbStop = false;
	mbFinished = false;
	miStartTime = -1;
	muiStartCycles = 0;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65EmulatorRunner::Kill(void)
{
	Stop();
	mcThread.Kill();
	mcChanged.Kill();
	mcMutex.Kill();
	mpcDebugger = NULL;
	mpcHCP65 = NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The thread isn't idle from here, so that a Pause before it gets going
// waits for it.
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65EmulatorRunner::Start(void)
{
	mcMutex.Lock();
	mbIdle = false;
	mbStop = false;
	mbFinished = false;
	mcMutex.Unlock();
	miStartTime = -1;

	if (!mcThread.Start(ThreadFunction, this))
	{
		mcMutex.Lock();
		mbIdle = true;
		mcChanged.WakeAll();
		mcMutex.Unlock();
		return false;
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Returns once the thread has stopped.  Nothing happens if it wasn't
// started.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65EmulatorRunner::Stop(void)
{
	mcMutex.Lock();
	mbStop = true;
	mcChanged.WakeAll();
	mcMutex.Unlock();
	mcThread.Join();
}


//////////////////////////////////////////////////////////////////////////
//
// Returns once the HCP65 isn't running.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65EmulatorRunner::Pause(void)
{
	mcMutex.Lock();
	miPauses++;
	while (!mbIdle)
	{
		mcChanged.Wait(&mcMutex);
	}
	mcMutex.Unlock();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65EmulatorRunner::Resume(void)
{
	mcMutex.Lock();
	if (miPauses > 0)
	{
		miPauses--;
	}
	mcChanged.WakeAll();
	mcMutex.Unlock();
}


//////////////////////////////////////////////////////////////////////////
//
// Through the debugger, if there is one, so that it can tell its client.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65EmulatorRunner::Reset(void)
{
	Pause();
	if (mpcDebugger && mpcDebugger->IsEnabled())
	{
		mpcDebugger->Reset();
	}
	else
	{
		mpcHCP65->Reset();
	}
	Resume();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65EmulatorRunner::IsFinished(void)
{
	bool	bFinished;

	mcMutex.Lock();
	bFinished = mbFinished;
	mcMutex.Unlock();
	return bFinished;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65EmulatorRunner::ThreadFunction(void* pvRunner)
{
	((CHCP65EmulatorRunner*)pvRunner)->Run();
}


//////////////////////////////////////////////////////////////////////////
//
// The thread: it runs the HCP65 until it's stopped or the debugger says
// the emulator should close.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65EmulatorRunner::Run(void)
{
	bool	bFinished;

	bFinished = false;
	while (WaitWhilePaused())
	{
		if (!RunDue())
		{
			bFinished = true;
			break;
		}
	}

	mcMutex.Lock();
	mbIdle = true;
	mbFinished = bFinished;
	mcChanged.WakeAll();
	mcMutex.Unlock();
}


//////////////////////////////////////////////////////////////////////////
//
// While there's a pause the thread is idle, which lets Pause return.  The
// clock starts again after one.  False once the thread is to stop.
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65EmulatorRunner::WaitWhilePaused(void)
{
	bool	bPaused;
	bool	bRun;

	bPaused = false;
	mcMutex.Lock();
	while ((miPauses > 0) && !mbStop)
	{
		if (!mbIdle)
		{
			mbIdle = true;
			mcChanged.WakeAll();
		}
		bPaused = true;
		mcChanged.Wait(&mcMutex);
	}
	bRun = !mbStop;
	if (bRun)
	{
		mbIdle = false;
	}
	mcMutex.Unlock();

	if (bPaused)
	{
		miStartTime = -1;
	}
	return bRun;
}


//////////////////////////////////////////////////////////////////////////
//
// Runs the HCP65 for the cycles due, or sleeps if there are none.  The
// debugger reads its messages either way, and while it has the HCP65
// stopped the clock starts again from where it was when it runs again.
// False when the debugger says the emulator should close.
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65EmulatorRunner::RunDue(void)
{
	uint64	uiCycles;
	bool	bResult;

	if (mpcDebugger && mpcDebugger->IsEnabled())
	{
		uiCycles = 0;
		if (mpcDebugger->IsRunning())
		{
			uiCycles = CyclesDue(GetDebugTickCount());
		}
		else
		{
			miStartTime = -1;
		}
		bResult = mpcDebugger->Tick(uiCycles);
		if (uiCycles == 0)
		{
			ThreadSleep(HCP65_EMULATOR_IDLE_MS);
		}
		return bResult;
	}

	uiCycles = mpcHCP65->IsStopped() ? 0 : CyclesDue(GetDebugTickCount());
	if (uiCycles == 0)
	{
		ThreadSleep(HCP65_EMULATOR_IDLE_MS);
		return true;
	}
	mpcHCP65->Run(uiCycles);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The cycles to run for the HCP65's clock to be where it should be by
// iNow, at most HCP65_EMULATOR_MAX_RUN_MS of its time.
//
//////////////////////////////////////////////////////////////////////////
uint64 CHCP65EmulatorRunner::CyclesDue(int64 iNow)
{
	uint64	uiClockRate;
	uint64	uiTarget;
	uint64	uiCycles;
	uint64	uiMaxCycles;

	uiClockRate = mpcHCP65->GetClockRate();
	if (miStartTime < 0)
	{
		miStartTime = iNow;
		muiStartCycles = mpcHCP65->NumCycles();
	}

	uiTarget = muiStartCycles + (uiClockRate * (uint64)(iNow - miStartTime)) / 1000;
	if (uiTarget <= mpcHCP65->NumCycles())
	{
		return 0;
	}
	uiCycles = uiTarget - mpcHCP65->NumCycles();
	uiMaxCycles = (uiClockRate * HCP65_EMULATOR_MAX_RUN_MS) / 1000;
	if (uiCycles > uiMaxCycles)
	{
		//The host can't keep up: the HCP65's time falls behind (the start goes back by the cycles not run, so the next
		//target is from where it got to; it may wrap, but the targets made from it don't).
		muiStartCycles -= uiCycles - uiMaxCycles;
		uiCycles = uiMaxCycles;
	}
	return uiCycles;
}
