#include <windows.h>
#include "BaseLib/Logger.h"
#include "BaseLib/TypeNames.h"
#include "BaseLib/TypeConverter.h"
#include "BaseLib/GlobalMemory.h"
#include "BaseLib/GlobalDataTypesIO.h"
#include "StandardLib/Objects.h"
#include "InputLib/Input.h"
#include "WinGdiLib/WinGDIWindowFactory.h"
#include "WinWaveoutLib/WinWaveoutSoundFactory.h"
#include "WinInputLib/WinInput.h"
#include "HCP65Emulator.h"


//////////////////////////////////////////////////////////////////////////
//
// The emulator on Windows: GDI windows, raw input and waveOut sound.
//
//////////////////////////////////////////////////////////////////////////
int PASCAL WinMain(HINSTANCE hInstance,	HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{
	_CrtSetBreakAlloc(0);

	//Reading a board's model stops the window's loop for seconds.  Windows would put a ghost in place of a window that
	//stops answering, and when it took the ghost away the Board Configuration dialog was left behind its disabled owner.
	DisableProcessWindowsGhosting();

	gcLogger.Init();
	TypesInit();
	TypeConverterInit();
	MemoryInit();
	DataIOInit();
	ObjectsInit();

	bool bResult;

	{
		CWinGDIWindowFactory	cNativeFactory;
		CWinWaveoutSoundFactory	cSoundFactory;
		CWinInput				cWinInput;

		cNativeFactory.Init(&gcMemoryAllocator,
			hInstance,
			hPrevInstance,
			nCmdShow,
			"HCP65Emulator");
		cSoundFactory.Init(&gcMemoryAllocator);

		//The emulator's input reads the devices from the XML, then adds these.
		cWinInput.Init(NULL, false);
		cNativeFactory.SetRawInput(WinRawInputCallback, cWinInput.GetRaw());

		bResult = HCP65EmulatorRun(&cNativeFactory, &cSoundFactory, &cWinInput, "../../../CommonLibraries/Libraries/WinInputLib/xml", __argc, __argv);

		cNativeFactory.SetRawInput(NULL, NULL);
		cWinInput.Kill();
		cSoundFactory.Kill();
		cNativeFactory.Kill();
	}

	ObjectsFlush();
	ObjectsKill();
	DataIOKill();
	MemoryKill();
	TypeConverterKill();
	TypesKill();

	gcLogger.Kill();

	_CrtDumpMemoryLeaks();

	return bResult ? 0 : 1;
}
