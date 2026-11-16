#include <windows.h>
#include "BaseLib/LeakDetector.h"
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
	BreakOnAllocation(0);

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
		Ptr<CWinGDIWindowFactory>	pNativeFactory;
		CWinWaveoutSoundFactory		cSoundFactory;
		CWinInput					cWinInput;

		//Rooted while the emulator runs.  Its windows hold it too, so it's freed after the last of them.
		pNativeFactory = OMalloc<CWinGDIWindowFactory>(&gcMemoryAllocator,
			hInstance,
			hPrevInstance,
			nCmdShow,
			"HCP65Emulator");
		ORoot()->Add(pNativeFactory);
		cSoundFactory.Init(&gcMemoryAllocator);

		//The emulator's input reads the devices from the XML, then adds these.
		cWinInput.Init(NULL, false);
		pNativeFactory->SetRawInput(WinRawInputCallback, cWinInput.GetRaw());

		bResult = HCP65EmulatorRun(&pNativeFactory, &cSoundFactory, &cWinInput, "../../../CommonLibraries/Libraries/WinInputLib/xml", __argc, __argv);

		pNativeFactory->SetRawInput(NULL, NULL);
		cWinInput.Kill();
		cSoundFactory.Kill();
		ORoot()->Remove(pNativeFactory);
		pNativeFactory = NULL;
	}

	ObjectsFlush();
	ObjectsKill();
	DataIOKill();
	MemoryKill();
	TypeConverterKill();
	TypesKill();

	gcLogger.Kill();

	LeakDetection();

	return bResult ? 0 : 1;
}
