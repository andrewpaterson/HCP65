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
		CInput					cInput;
		CWinInput				cWinInput;

		cNativeFactory.Init(&gcMemoryAllocator,
			hInstance,
			hPrevInstance,
			nCmdShow,
			"HCP65Emulator");
		cSoundFactory.Init(&gcMemoryAllocator);

		//The devices are described by the XML so they are added after it is read.
		cInput.Init(NULL);
		cInput.Read("../../../CommonLibraries/Libraries/WinInputLib/xml");
		cWinInput.Init(NULL, false);
		cInput.AddNativeInput(&cWinInput);
		cNativeFactory.SetRawInput(WinRawInputCallback, cWinInput.GetRaw());

		bResult = HCP65EmulatorRun(&cNativeFactory, &cSoundFactory, &cInput);

		cNativeFactory.SetRawInput(NULL, NULL);
		cInput.Kill();
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
