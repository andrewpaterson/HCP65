#include "BaseLib/Logger.h"
#include "BaseLib/TypeNames.h"
#include "BaseLib/TypeConverter.h"
#include "BaseLib/GlobalMemory.h"
#include "BaseLib/GlobalDataTypesIO.h"
#include "StandardLib/Objects.h"
#include "InputLib/Input.h"
#include "X11Lib/X11WindowFactory.h"
#include "X11Lib/X11Input.h"
#include "PulseAudioLib/PulseAudioSoundFactory.h"
#include "HCP65Emulator.h"


//////////////////////////////////////////////////////////////////////////
//
// The emulator on Linux: X11 windows, their keyboard and mouse events
// given to the input as Windows raw input events (X11Input.h), and
// PulseAudio sound.
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
		Ptr<CX11WindowFactory>			pNativeFactory;
		Ptr<CPulseAudioSoundFactory>	pSoundFactory;
		CX11Input						cX11Input;

		//Both rooted while the emulator runs.  Its windows and mixer hold them too, so they're freed after the last of those.
		pNativeFactory = OMalloc<CX11WindowFactory>(&gcMemoryAllocator, (const char*)NULL);
		ORoot()->Add(pNativeFactory);
		bResult = pNativeFactory->IsOpen();
		pSoundFactory = OMalloc<CPulseAudioSoundFactory>(&gcMemoryAllocator);
		ORoot()->Add(pSoundFactory);
		if (bResult)
		{
			//The emulator's input reads the devices from the XML, then adds these.
			cX11Input.Init();
			pNativeFactory->SetInput(&cX11Input);

			bResult = HCP65EmulatorRun(&pNativeFactory, &pSoundFactory, &cX11Input, "../../../CommonLibraries/Libraries/WinInputLib/xml", argc, argv);

			pNativeFactory->SetInput(NULL);
			cX11Input.Kill();
		}
		ORoot()->Remove(pSoundFactory);
		pSoundFactory = NULL;
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
