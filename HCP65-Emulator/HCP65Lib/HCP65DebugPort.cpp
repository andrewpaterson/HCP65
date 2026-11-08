#include <string.h>
#include "HCP65.h"
#include "HCP65DebugPort.h"


#if (HCP65_DEBUG_NAME_SIZE != DEBUG_PROCESS_NAME_SIZE) || (HCP65_DEBUG_NAME_SIZE != DEBUG_MODULE_NAME_SIZE)
#error A debug message's name is copied whole into a process's and a driver's.
#endif


//////////////////////////////////////////////////////////////////////////
//
// pcHCP65 is the machine it stops and fills the banks of.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65DebugPort::Init(CHCP65* pcHCP65)
{
	mpcHCP65 = pcHCP65;
	memset(mabRunning, 0, sizeof(mabRunning));
	memset(maszRunning, 0, sizeof(maszRunning));
	mbWatching = false;
	masEvents.Init();
	masDrivers.Init();
	masDriverEvents.Init();
	mbDriversCleared = false;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65DebugPort::Kill(void)
{
	masDriverEvents.Kill();
	masDrivers.Kill();
	masEvents.Kill();
	mpcHCP65 = NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The machine's reset: the kernal starts again, so the drivers it loaded
// are gone.  While watched that is an event if there were any.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65DebugPort::Reset(void)
{
	if (mbWatching && (masDrivers.NumElements() > 0))
	{
		mbDriversCleared = true;
	}
	masDrivers.ReInit();
	masDriverEvents.ReInit();
}


//////////////////////////////////////////////////////////////////////////
//
// A message from the kernal.  A command it doesn't know is ignored.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65DebugPort::Tell(SHCP65DebugMessage* psMessage)
{
	switch (psMessage->uiCommand)
	{
	case HCP65_DEBUG_STARTED:
	case HCP65_DEBUG_ENDED:
		TellProcess(psMessage);
		break;
	case HCP65_DEBUG_BANK:
		mpcHCP65->FillMainMemoryBank(psMessage->uiValue);
		break;
	case HCP65_DEBUG_DRIVER:
		TellDriver(psMessage);
		break;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A process started (it is running, with the message's name) or ended.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65DebugPort::TellProcess(SHCP65DebugMessage* psMessage)
{
	SHCP65DebugPortEvent*	psEvent;
	uint8					uiProcess;
	bool					bStarted;

	uiProcess = psMessage->uiValue;
	bStarted = psMessage->uiCommand == HCP65_DEBUG_STARTED;
	if (bStarted)
	{
		mabRunning[uiProcess] = true;
		memcpy(maszRunning[uiProcess], psMessage->szName, DEBUG_PROCESS_NAME_SIZE);
		maszRunning[uiProcess][DEBUG_PROCESS_NAME_SIZE - 1] = '\0';
	}

	if (mbWatching)
	{
		psEvent = masEvents.Add();
		psEvent->eEvent = bStarted ? DPE_Started : DPE_Ended;
		psEvent->sProcess.uiProcess = uiProcess;
		psEvent->sProcess.iExitCode = (int16)psMessage->uiExitCode;
		memcpy(psEvent->sProcess.szName, maszRunning[uiProcess], DEBUG_PROCESS_NAME_SIZE);
		if (bStarted)
		{
			mpcHCP65->StopRun();
		}
	}

	if (!bStarted)
	{
		mabRunning[uiProcess] = false;
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
size CHCP65DebugPort::NumProcesses(void)
{
	size	i;
	size	uiCount;

	uiCount = 0;
	for (i = 0; i < HCP65_NUM_PROCESSES; i++)
	{
		uiCount += mabRunning[i] ? 1 : 0;
	}
	return uiCount;
}


//////////////////////////////////////////////////////////////////////////
//
// The uiIndex'th running process, by ID.
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65DebugPort::GetProcess(size uiIndex, SDebugProcess* psDest)
{
	size	i;

	for (i = 0; i < HCP65_NUM_PROCESSES; i++)
	{
		if (mabRunning[i])
		{
			if (uiIndex == 0)
			{
				psDest->uiProcess = (uint32)i;
				psDest->iExitCode = 0;
				memcpy(psDest->szName, maszRunning[i], DEBUG_PROCESS_NAME_SIZE);
				return true;
			}
			uiIndex--;
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// While watched, processes started and ended are kept until taken; not
// watched, none are.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65DebugPort::Watch(bool bWatch)
{
	mbWatching = bWatch;
	masEvents.ReInit();
	masDriverEvents.ReInit();
}


//////////////////////////////////////////////////////////////////////////
//
// The oldest event not taken.  False if there is none.
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65DebugPort::TakeEvent(EDebugProcessEvent* peEvent, SDebugProcess* psDest)
{
	SHCP65DebugPortEvent*	psEvent;

	if (masEvents.NumElements() == 0)
	{
		return false;
	}
	psEvent = masEvents.Get(0);
	*peEvent = psEvent->eEvent;
	memcpy(psDest, &psEvent->sProcess, sizeof(SDebugProcess));
	masEvents.RemoveAt(0);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The kernal loaded a driver: it is kept, and while watched it is an event
// and stops the Run before the driver's code runs.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65DebugPort::TellDriver(SHCP65DebugMessage* psMessage)
{
	SHCP65DebugDriver*	psDriver;

	psDriver = masDrivers.Add();
	psDriver->uiSlot = psMessage->uiValue;
	psDriver->uiAddress = psMessage->uiAddress;
	psDriver->uiSize = psMessage->uiSize;
	memcpy(psDriver->szName, psMessage->szName, DEBUG_MODULE_NAME_SIZE);
	psDriver->szName[DEBUG_MODULE_NAME_SIZE - 1] = '\0';
	if (mbWatching)
	{
		masDriverEvents.Add(psDriver);
		mpcHCP65->StopRun();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The drivers the kernal loaded since the last reset, in load order.
//
//////////////////////////////////////////////////////////////////////////
size CHCP65DebugPort::NumDrivers(void)
{
	return masDrivers.NumElements();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
SHCP65DebugDriver* CHCP65DebugPort::GetDriver(size uiIndex)
{
	return masDrivers.Get(uiIndex);
}


//////////////////////////////////////////////////////////////////////////
//
// The oldest driver loaded while watched that hasn't been taken.  False if
// there is none.
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65DebugPort::TakeDriverEvent(SHCP65DebugDriver* psDest)
{
	if (masDriverEvents.NumElements() == 0)
	{
		return false;
	}
	memcpy(psDest, masDriverEvents.Get(0), sizeof(SHCP65DebugDriver));
	masDriverEvents.RemoveAt(0);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether a reset dropped the drivers since this was last asked (while
// watched).  Taken before the drivers loaded since, which follow it.
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65DebugPort::TakeDriversCleared(void)
{
	bool	bCleared;

	bCleared = mbDriversCleared;
	mbDriversCleared = false;
	return bCleared;
}
