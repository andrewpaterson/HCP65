#ifndef __HCP65_EMULATOR_DEBUGGER_H__
#define __HCP65_EMULATOR_DEBUGGER_H__
#include "BaseLib/Chars.h"
#include "BaseLib/ListenSocket.h"
#include "ThreadLib/NetworkConnection.h"
#include "ThreadLib/SharedMemoryChannel.h"
#include "EmulatorDebugLib/DebugServer.h"
#include "EmulatorDebugLib/DebugLaunch.h"
#include "HCP65Lib/HCP65DebugTarget.h"


//The command line arguments that make the emulator debuggable are EmulatorDebugLib's (DebugLaunch.h), shared with
//whatever starts it.


enum EHCP65DebugTransport
{
	HDT_None,
	HDT_SharedMemory,
	HDT_Network,
};


//The emulator's end of a debugger's connection: the channel (made from the command line), the HCP65 as a debug target
//and the server.  The HCP65 runs from the start and a debugger can attach whenever it likes, or with DEBUG_ARGUMENT_WAIT
//it holds stopped before its first instruction until a debugger attaches and continues it.  On the network another
//debugger can attach after one detaches.
class CHCP65EmulatorDebugger
{
protected:
	EHCP65DebugTransport	meTransport;
	CChars					mszName;
	int						miPort;
	bool					mbExitOnDetach;
	bool					mbWait;

	CListenSocket			mcListen;
	CNetworkConnection		mcNetwork;
	CSharedMemoryChannel	mcShared;
	bool					mbStarted;
	bool					mbChannelOpen;		//Created (shared memory) or accepted (network): the server can read it.

	CHCP65DebugTarget		mcTarget;
	CDebugServer			mcServer;

public:
	void	Init(void);
	void	Kill(void);

	bool	ParseArguments(int argc, char* argv[]);
	bool	IsEnabled(void);

	bool	Start(CHCP65* pcHCP65);
	bool	Tick(uint64 uiCycles);
	bool	IsRunning(void);

protected:
	void	Accept(void);
};


#endif // __HCP65_EMULATOR_DEBUGGER_H__
