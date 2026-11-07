#ifndef __HCP65_EMULATOR_H__
#define __HCP65_EMULATOR_H__


//Relative to the emulator's directory (HCP65Emulator/HCP65Emulator), as Visual Studio runs it.  TestHCP65Lib's
//TestHCP65Kernal builds the ROM, and the SD card image (a copy of the blank Input/SDCard.img with boot.exe, modplay.exe
//and SpaceDebris.mod on it), into its Output: run it first.
#define HCP65_EMULATOR_KERNAL_ROM		"../TestHCP65Lib/TestHCP65Lib/Output/Kernal.rom"
#define HCP65_EMULATOR_SD_CARD_IMAGE	"../TestHCP65Lib/TestHCP65Lib/Output/SDCard.img"
#define HCP65_EMULATOR_SD_SOCKET		2			//Socket 1 is the main memory channel.
#define HCP65_EMULATOR_AUDIO_SOCKET		16
#define HCP65_EMULATOR_DEBUG_SOCKET		7			//The debug port: the kernal tells it of its processes.
#define HCP65_EMULATOR_UART_SOCKET		4			//The UART Board; nothing is on the far end of its ports yet.
#define HCP65_EMULATOR_MEMORY_SOCKET	18			//The Memory Board: main memory, window 1 once the kernal has set it.
#define HCP65_EMULATOR_ARGUMENT_FILL	"--fill-memory"	//Fill mode (CHCP65::SetMemoryFill): memory nothing wrote reads $CC.


#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "InputLib/Input.h"
#include "InputLib/NativeInput.h"
#include "WindowLib/Window.h"
#include "WindowLib/MenuBar.h"
#include "WindowLib/WindowInputSource.h"
#include "WindowLib/ComponentInput.h"
#include "WindowLib/NativeWindowFactory.h"
#include "WindowLib/NativeSoundFactory.h"
#include "HCP65Lib/HCP65.h"
#include "HCP65Lib/HCP65Audio.h"
#include "HCP65Lib/HCP65SDCard.h"
#include "HCP65Lib/HCP65DebugPort.h"
#include "HCP65Lib/HCP65UART.h"
#include "FileMenuListener.h"
#include "HCP65EmulatorDebugger.h"


//The emulator: the HCP65 (its kernal ROM, the sound board, the UART Board and an SD card board with
//HCP65_EMULATOR_SD_CARD_IMAGE, or the card given with --card) in a
//window with a menu bar, and the input that drives it.  HCP65EmulatorRun makes it, adds it to the root, runs it and removes
//it from the root.  The command line can make it debuggable (CHCP65EmulatorDebugger).
class CHCP65EmulatorApp : public CObject
{
CONSTRUCTABLE(CHCP65EmulatorApp);
DESTRUCTABLE(CHCP65EmulatorApp);
protected:
	CHCP65					mcHCP65;
	CHCP65Audio				mcAudio;
	CHCP65SDCard			mcSDCard;
	CHCP65DebugPort			mcDebugPort;
	CHCP65UARTBoard			mcUART;
	CHCP65MemoryBoard		mcMemory;
	CHCP65EmulatorDebugger	mcDebugger;
	Ptr<CWindow>			mpWindow;
	Ptr<CMenuBar>			mpMenuBar;
	CFileMenuListener		mcFileMenuListener;
	Ptr<CInput>				mpInput;
	Ptr<CWindowInputSource>	mpInputSource;
	CComponentInput			mcComponentInput;
	bool					mbValid;
	bool					mbInputs;  //The menu listener and the component input are initialised.

public:
	void	Init(void);
	bool	Setup(CNativeWindowFactory* pcNativeFactory, CNativeSoundFactory* pcSoundFactory, CNativeInput* pcNativeInput, const char* szInputDirectory, int argc, char* argv[]);
	void	Class(void) override;
	void	Free(void) override;

	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	bool	Run(void);
};


bool HCP65EmulatorRun(CNativeWindowFactory* pcNativeFactory, CNativeSoundFactory* pcSoundFactory, CNativeInput* pcNativeInput, const char* szInputDirectory, int argc, char* argv[]);


#endif // __HCP65_EMULATOR_H__
