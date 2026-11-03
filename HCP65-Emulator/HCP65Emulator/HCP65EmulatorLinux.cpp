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
		CX11WindowFactory		cNativeFactory;
		CPulseAudioSoundFactory	cSoundFactory;
		CX11Input				cX11Input;

		bResult = cNativeFactory.Init(&gcMemoryAllocator, NULL);
		cSoundFactory.Init(&gcMemoryAllocator);
		if (bResult)
		{
			//The emulator's input reads the devices from the XML, then adds these.
			cX11Input.Init();
			cNativeFactory.SetInput(&cX11Input);

			bResult = HCP65EmulatorRun(&cNativeFactory, &cSoundFactory, &cX11Input, "../../../commonlibraries/Libraries/WinInputLib/xml");

			cNativeFactory.SetInput(NULL);
			cX11Input.Kill();
		}
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

	return bResult ? 0 : 1;
}
