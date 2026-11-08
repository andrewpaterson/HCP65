#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "BaseLib/Logger.h"
#include "BaseLib/Socket.h"
#include "HCP65EmulatorDebugger.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65EmulatorDebugger::Init(void)
{
	meTransport = HDT_None;
	mszName.Init();
	miPort = 0;
	mbExitOnDetach = false;
	mbWait = false;
	mcNetwork.Init();
	mcShared.Init();
	mbStarted = false;
	mbChannelOpen = false;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65EmulatorDebugger::Kill(void)
{
	if (mbStarted)
	{
		mcServer.Kill();
		mcTarget.Kill();
		if (meTransport == HDT_Network)
		{
			mcListen.Kill();
			SocketsKill();
		}
	}
	mcShared.Kill();
	mcNetwork.Kill();
	mszName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The debugging arguments (DEBUG_ARGUMENT_*); others are left for the rest of
// the emulator.  False if one is malformed.
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65EmulatorDebugger::ParseArguments(int argc, char* argv[])
{
	int		i;

	for (i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], DEBUG_ARGUMENT_SHARED_MEMORY) == 0)
		{
			if ((i + 1 >= argc) || (meTransport != HDT_None))
			{
				gcLogger.Error2(__METHOD__, " " DEBUG_ARGUMENT_SHARED_MEMORY " needs a name, and only one debug channel can be given.", NULL);
				return false;
			}
			i++;
			meTransport = HDT_SharedMemory;
			mszName.Kill();
			mszName.Init(argv[i]);
		}
		else if (strcmp(argv[i], DEBUG_ARGUMENT_NETWORK) == 0)
		{
			if ((i + 1 >= argc) || (meTransport != HDT_None))
			{
				gcLogger.Error2(__METHOD__, " " DEBUG_ARGUMENT_NETWORK " needs a port, and only one debug channel can be given.", NULL);
				return false;
			}
			i++;
			meTransport = HDT_Network;
			miPort = atoi(argv[i]);
		}
		else if (strcmp(argv[i], DEBUG_ARGUMENT_EXIT_ON_DETACH) == 0)
		{
			mbExitOnDetach = true;
		}
		else if (strcmp(argv[i], DEBUG_ARGUMENT_WAIT) == 0)
		{
			mbWait = true;
		}
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65EmulatorDebugger::IsEnabled(void)
{
	return meTransport != HDT_None;
}


//////////////////////////////////////////////////////////////////////////
//
// A debugger is to continue it (--debug-wait), so it may load the
// program first.
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65EmulatorDebugger::IsWaiting(void)
{
	return IsEnabled() && mbWait;
}


//////////////////////////////////////////////////////////////////////////
//
// Makes the channel, debugging pcHCP65.  A shared memory channel is created now; the network
// listens now and accepts the debugger in a later Tick.  The server holds
// the HCP65 stopped (DEBUG_ARGUMENT_WAIT) or runs it until then.
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65EmulatorDebugger::Start(CHCP65* pcHCP65)
{
	CProcessChannel*	pcChannel;

	mcTarget.Init(pcHCP65);
	if (meTransport == HDT_SharedMemory)
	{
		if (!mcShared.Create(mszName.Text()))
		{
			gcLogger.Error2(__METHOD__, " Could not create the debug channel [", mszName.Text(), "].", NULL);
			mcTarget.Kill();
			return false;
		}
		mbChannelOpen = true;
		pcChannel = &mcShared;
	}
	else
	{
		SocketsInit();
		if (!mcListen.Init(NETWORK_LOOPBACK_ADDRESS, miPort))
		{
			gcLogger.Error2(__METHOD__, " Could not listen for a debugger on port [", IntToString(miPort), "].", NULL);
			SocketsKill();
			mcTarget.Kill();
			return false;
		}
		printf(DEBUG_PORT_LINE "%d\n", mcListen.GetPort());
		fflush(stdout);
		pcChannel = NULL;
	}

	mcServer.Init(pcChannel, &mcTarget, mbWait);
	mbStarted = true;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Each window tick, with the cycles the HCP65 is due (0 while it isn't
// running).  False when the emulator should close: the debugger sent
// DM_Quit, or it went and DEBUG_ARGUMENT_EXIT_ON_DETACH was given.  Without
// a debugger the server runs the HCP65 itself; on the network the next
// debugger is accepted once the last has gone.
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65EmulatorDebugger::Tick(uint64 uiCycles)
{
	if (!mbChannelOpen)
	{
		Accept();
	}

	mcServer.Tick(uiCycles);
	if (mcServer.IsQuitting())
	{
		return false;
	}
	if (mbChannelOpen && mcServer.IsClosed())
	{
		if (mbExitOnDetach)
		{
			return false;
		}
		if (meTransport == HDT_Network)
		{
			mcNetwork.Kill();
			mcNetwork.Init();
			mbChannelOpen = false;
		}
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether the HCP65's time goes on: not while the debugger has it stopped
// or is stepping it, nor while it waits for a debugger (DEBUG_ARGUMENT_WAIT).
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65EmulatorDebugger::IsRunning(void)
{
	return mcServer.GetState() == DSS_Running;
}


//////////////////////////////////////////////////////////////////////////
//
// Without waiting.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65EmulatorDebugger::Accept(void)
{
	if (mcNetwork.Accept(&mcListen, 0))
	{
		mbChannelOpen = true;
		mcServer.Attach(&mcNetwork);
	}
}
