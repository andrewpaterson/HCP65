#include <string.h>
#include "HCP65.h"
#include "HCP65DebugPort.h"


//////////////////////////////////////////////////////////////////////////
//
// A small slot's device: its ROM holds its name.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65DebugPort::Init(void)
{
	CHCP65Device::Init(HCP65_SMALL_SLOT_SIZE);
	memset(mauiROM, 0xFF, HCP65_DEBUG_PORT_ROM_SIZE);
	memcpy(mauiROM, HCP65_DEBUG_PORT_NAME, strlen(HCP65_DEBUG_PORT_NAME) + 1);
	muiProcess = 0;
	muiExitCode = 0;
	memset(mszName, 0, sizeof(mszName));
	memset(mabRunning, 0, sizeof(mabRunning));
	memset(maszRunning, 0, sizeof(maszRunning));
	mbWatching = false;
	masEvents.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65DebugPort::Kill(void)
{
	masEvents.Kill();
	CHCP65Device::Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The ROM, then the registers as written.
//
//////////////////////////////////////////////////////////////////////////
uint8 CHCP65DebugPort::Read(uint32 uiOffset)
{
	if (uiOffset < HCP65_DEBUG_PORT_ROM_SIZE)
	{
		return mauiROM[uiOffset];
	}
	switch (uiOffset)
	{
	case HCP65_DEBUG_PORT_PROCESS:		return muiProcess;
	case HCP65_DEBUG_PORT_EXIT_LOW:		return (uint8)muiExitCode;
	case HCP65_DEBUG_PORT_EXIT_HIGH:	return (uint8)(muiExitCode >> 8);
	}
	if ((uiOffset >= HCP65_DEBUG_PORT_NAME_BUFFER) && (uiOffset < HCP65_DEBUG_PORT_NAME_BUFFER + DEBUG_PROCESS_NAME_SIZE))
	{
		return (uint8)mszName[uiOffset - HCP65_DEBUG_PORT_NAME_BUFFER];
	}
	return 0xFF;
}


//////////////////////////////////////////////////////////////////////////
//
// The kernal writes the process, its exit code and name, then the command.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65DebugPort::Write(uint32 uiOffset, uint8 uiValue)
{
	switch (uiOffset)
	{
	case HCP65_DEBUG_PORT_COMMAND:		Tell(uiValue);											return;
	case HCP65_DEBUG_PORT_PROCESS:		muiProcess = uiValue;									return;
	case HCP65_DEBUG_PORT_EXIT_LOW:		muiExitCode = (uint16)((muiExitCode & 0xFF00) | uiValue);	return;
	case HCP65_DEBUG_PORT_EXIT_HIGH:	muiExitCode = (uint16)((muiExitCode & 0x00FF) | (uiValue << 8));	return;
	}
	if ((uiOffset >= HCP65_DEBUG_PORT_NAME_BUFFER) && (uiOffset < HCP65_DEBUG_PORT_NAME_BUFFER + DEBUG_PROCESS_NAME_SIZE - 1))
	{
		mszName[uiOffset - HCP65_DEBUG_PORT_NAME_BUFFER] = (char)uiValue;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A process started (it is running, with the name written) or ended.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65DebugPort::Tell(uint8 uiCommand)
{
	SHCP65DebugPortEvent*	psEvent;

	if (uiCommand == HCP65_DEBUG_PORT_BANK)
	{
		if (mpcHCP65 != NULL)
		{
			mpcHCP65->FillMainMemoryBank(muiProcess);
		}
		return;
	}
	if ((uiCommand != HCP65_DEBUG_PORT_STARTED) && (uiCommand != HCP65_DEBUG_PORT_ENDED))
	{
		return;
	}

	if (uiCommand == HCP65_DEBUG_PORT_STARTED)
	{
		mabRunning[muiProcess] = true;
		memcpy(maszRunning[muiProcess], mszName, DEBUG_PROCESS_NAME_SIZE);
		maszRunning[muiProcess][DEBUG_PROCESS_NAME_SIZE - 1] = '\0';
	}

	if (mbWatching)
	{
		psEvent = masEvents.Add();
		psEvent->eEvent = (uiCommand == HCP65_DEBUG_PORT_STARTED) ? DPE_Started : DPE_Ended;
		psEvent->sProcess.uiProcess = muiProcess;
		psEvent->sProcess.iExitCode = (int16)muiExitCode;
		memcpy(psEvent->sProcess.szName, maszRunning[muiProcess], DEBUG_PROCESS_NAME_SIZE);
		if ((uiCommand == HCP65_DEBUG_PORT_STARTED) && (mpcHCP65 != NULL))
		{
			mpcHCP65->StopRun();
		}
	}

	if (uiCommand == HCP65_DEBUG_PORT_ENDED)
	{
		mabRunning[muiProcess] = false;
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
	for (i = 0; i < 256; i++)
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

	for (i = 0; i < 256; i++)
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
	masEvents.Kill();
	masEvents.Init();
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

