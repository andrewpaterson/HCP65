#include "BaseLib/Logger.h"
#include "BaseLib/TypeNames.h"
#include "BaseLib/TypeConverter.h"
#include "BaseLib/GlobalMemory.h"
#include "BaseLib/GlobalDataTypesIO.h"
#include "StandardLib/Objects.h"
#include "WindowLib/Window.h"
#include "WindowLib/ClearCanvasDraw.h"
#include "WindowLib/JewelColours.h"
#include "X11Lib/X11WindowFactory.h"
#include "TesterWindowTick.h"


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
		Ptr<CWindow>			pTesterWindow;
		Ptr<CTesterWindowTick>	pTick;

		bResult = cNativeFactory.Init(&gcMemoryAllocator, NULL);
		if (bResult)
		{
			//The window is in the root; its tick is allocated, not on the stack, because stack objects can't be under the root.
			pTick = OMalloc<CTesterWindowTick>();
			pTesterWindow = OMalloc<CWindow>("HCP65 Board Tester", &cNativeFactory, pTick, OMalloc<CClearCanvasDraw>(JEWEL_PANEL), CFT_RGBX, CCO_BGR, CRGB_24bit, ARGB_8bit);
			ORoot()->Add(pTesterWindow);

			pTesterWindow->SetWindowSize(1280, 800);
			pTesterWindow->Show();

			ORoot()->Remove(pTesterWindow);
			pTesterWindow = NULL;
			pTick = NULL;
			ORoot()->RemoveAll();
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
