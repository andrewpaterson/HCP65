#ifndef __HCP65_EMULATOR_H__
#define __HCP65_EMULATOR_H__


//Relative to the emulator's directory (HCP65Emulator/HCP65Emulator), as Visual Studio runs it.  TestHCP65Lib's
//TestHCP65Kernal builds the ROM, and the SD card image (a copy of the blank Input/SDCard.img with boot.exe, modplay.exe
//and SpaceDebris.mod on it) the machine descriptions name, into its Output: run it first.
#define HCP65_EMULATOR_KERNAL_ROM		"../TestHCP65Lib/TestHCP65Lib/Output/Kernal.rom"
#define HCP65_EMULATOR_MACHINE			"Machines/SmallMainboard.xml"	//The boards (CHCP65Mainboard) without --machine.
#define HCP65_EMULATOR_ARGUMENT_MACHINE	"--machine"		//The mainboard description: which boards are in which sockets.
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
#include "HCP65Lib/HCP65Mainboard.h"
#include "FileMenuListener.h"
#include "HCP65EmulatorDebugger.h"


//The emulator: the HCP65 (its kernal ROM, and the boards HCP65_EMULATOR_MACHINE or --machine's description puts in its
//sockets; --card's image replaces the first SD Card Board's card) in a window with a menu bar, and the input that drives
//it.  The first Sound Board is heard and the first Debug Port is the debugger's.  HCP65EmulatorRun makes it, adds it to
//the root, runs it and removes it from the root.  The command line can make it debuggable (CHCP65EmulatorDebugger).
class CHCP65EmulatorApp : public CObject
{
CONSTRUCTABLE(CHCP65EmulatorApp);
DESTRUCTABLE(CHCP65EmulatorApp);
protected:
	CHCP65					mcHCP65;
	CHCP65Mainboard			mcMainboard;
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
