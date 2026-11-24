#include "BaseLib/Logger.h"
#include "BaseLib/TypeNames.h"
#include "BaseLib/TypeConverter.h"
#include "BaseLib/GlobalMemory.h"
#include "BaseLib/GlobalDataTypesIO.h"
#include "StandardLib/Objects.h"
#include "WinRefLib/WinRefWindowFactory.h"
#include "WinRefLib/WinRefInput.h"
#include "HCP65Emulator.h"


//The window's size when the emulator hasn't one of its own.
#define HCP65_REF_WINDOW_WIDTH	1280
#define HCP65_REF_WINDOW_HEIGHT	800


//////////////////////////////////////////////////////////////////////////
//
// The emulator on WinRefLib's reference screen, drawn into memory with
// nothing on any display and no sound, as HCP65IDERef is the IDE: for the
// IDE's checks to debug programs in (HCP65_IDE_EMULATOR names it).  It
// takes the same arguments and HCP65_EMULATOR_* variables as on screen,
// and is run from the HCP65Emulator directory the same way.  Nothing
// outside it can press a key or move the pointer.
//
//////////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[])
{
	gcLogger.Init();
	TypesInit();
	TypeConverterInit();
	MemoryInit();
	DataIOInit();
	ObjectsInit();

	bool bResult;

	{
		Ptr<CWinRefWindowFactory>	pNativeFactory;
		CWinRefInput				cRefInput;

		//Rooted while the emulator runs.  Its windows hold it too, so it's freed after the last of them.
		pNativeFactory = OMalloc<CWinRefWindowFactory>(&gcMemoryAllocator, HCP65_REF_WINDOW_WIDTH, HCP65_REF_WINDOW_HEIGHT, (char*)NULL);
		ORoot()->Add(pNativeFactory);
		cRefInput.Init();
		bResult = HCP65EmulatorRun(&pNativeFactory, NULL, &cRefInput, "../../../CommonLibraries/Libraries/WinInputLib/xml", argc, argv);
		cRefInput.Kill();
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

	return bResult ? 0 : 1;
}
