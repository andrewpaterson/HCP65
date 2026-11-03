#include "BaseLib/Logger.h"
#include "BaseLib/TypeNames.h"
#include "BaseLib/TypeConverter.h"
#include "BaseLib/GlobalMemory.h"
#include "BaseLib/GlobalDataTypesIO.h"
#include "StandardLib/Objects.h"
#include "X11Lib/X11WindowFactory.h"
#include "TesterApp.h"


//////////////////////////////////////////////////////////////////////////
//
// The tester's empty window on Linux, without the board.
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
		CX11WindowFactory		cNativeFactory;
		Ptr<CTesterApp>			pApp;

		bResult = cNativeFactory.Init(&gcMemoryAllocator, NULL);
		if (bResult)
		{
			//The app (and its window) is held by the root while it runs.
			pApp = OMalloc<CTesterApp>();
			ORoot()->Add(pApp);
			bResult = pApp->Setup(&cNativeFactory) && pApp->Run();
			ORoot()->Remove(pApp);
			pApp = NULL;
		}

		cNativeFactory.Kill();
	}

	ObjectsFlush();
	ObjectsKill();
	DataIOKill();
	MemoryKill();
	TypeConverterKill();
	TypesKill();

	gcLogger.Kill();

	if (bResult)
	{
		return 0;
	}
	else
	{
		return 1;
	}
}
