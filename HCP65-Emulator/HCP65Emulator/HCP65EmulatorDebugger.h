#ifndef __HCP65_EMULATOR_DEBUGGER_H__
#define __HCP65_EMULATOR_DEBUGGER_H__
#include "BaseLib/Chars.h"
#include "BaseLib/ListenSocket.h"
#include "ThreadLib/NetworkConnection.h"
#include "ThreadLib/SharedMemoryChannel.h"
#include "EmulatorDebugLib/DebugServer.h"
#include "HCP65Lib/HCP65DebugTarget.h"


//The command line arguments that make the emulator debuggable.
#define HCP65_DEBUG_SHARED_MEMORY	"--debug-shm"		//Then a name: the emulator Creates a shared memory channel with it.
#define HCP65_DEBUG_NETWORK			"--debug-tcp"		//Then a port (0 for any): the emulator listens on the loopback network.
#define HCP65_DEBUG_EXIT_ON_DETACH	"--exit-on-detach"	//The emulator closes when the debugger detaches or goes.
#define HCP65_DEBUG_WAIT			"--debug-wait"		//The HCP65 holds stopped before its first instruction until a debugger continues it.

//Printed on standard output when listening, with the port, so a debugger that started the emulator can connect.
#define HCP65_DEBUG_PORT_LINE		"HCP65 debug port "


enum EHCP65DebugTransport
{
	HDT_None,
	HDT_SharedMemory,
	HDT_Network,
};


//The emulator's end of a debugger's connection: the channel (made from the command line), the HCP65 as a debug target
//and the server.  The HCP65 runs from the start and a debugger can attach whenever it likes, or with HCP65_DEBUG_WAIT
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
