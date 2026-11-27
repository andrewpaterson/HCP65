#ifdef LINUX_GNU
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
		Ptr<CX11WindowFactory>	pNativeFactory;
		Ptr<CTesterApp>			pApp;

		//Rooted while the tester runs.  Its windows hold it too, so it's freed after the last of them.
		pNativeFactory = OMalloc<CX11WindowFactory>(&gcMemoryAllocator, (const char*)NULL);
		ORoot()->Add(pNativeFactory);
		bResult = pNativeFactory->IsOpen();
		if (bResult)
		{
			//The app (and its window) is held by the root while it runs.
			pApp = OMalloc<CTesterApp>();
			ORoot()->Add(pApp);
			bResult = pApp->Setup(&pNativeFactory) && pApp->Run();
			ORoot()->Remove(pApp);
			pApp = NULL;
		}

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

	if (bResult)
	{
		return 0;
	}
	else
	{
		return 1;
	}
}


#endif // LINUX_GNU

