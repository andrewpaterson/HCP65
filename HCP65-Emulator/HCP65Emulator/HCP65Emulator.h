#ifndef __HCP65_EMULATOR_H__
#define __HCP65_EMULATOR_H__


//Relative to the emulator's directory (HCP65Emulator/HCP65Emulator), as Visual Studio runs it.  TestHCP65Lib's
//TestHCP65Kernal builds the boot loader's ROM, and the SD card images the machine descriptions name (copies of the blank
//Input/SDCard.img with the kernal, \OS\OS.SYS, and the programs on them), into its Output: run it first.
#define HCP65_EMULATOR_BOOT_ROM			"../TestHCP65Lib/TestHCP65Lib/Output/Boot.rom"
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
#include "HCP65Lib/HCP65PS2Keyboard.h"
#include "HCP65ScreenDraw.h"
#include "HCP65Lib/HCP65Mainboard.h"
#include "FileMenuListener.h"
#include "HCP65MenuListener.h"
#include "BoardConfigurationDialog.h"
#include "HCP65SoundVoice.h"
#include "HCP65EmulatorTick.h"
#include "HCP65EmulatorDebugger.h"
#include "HCP65EmulatorRunner.h"


//The emulator: the HCP65 (its boot loader's ROM, and the boards HCP65_EMULATOR_MACHINE or --machine's description puts in its
//sockets; --card's image replaces the first SD Card Board's card in its card socket 0) in a window with a menu bar, and
//the input that drives it.  The first Sound Board is heard.  HCP65EmulatorRun makes it, adds it to the root, runs it and
//removes it from the root.  The command line can make it debuggable (CHCP65EmulatorDebugger).  The HCP65 runs on a
//thread of its own (CHCP65EmulatorRunner) while the window shows, and anything else that changes it pauses that first.
//
//A keyboard (CHCP65PS2Keyboard) is plugged into port 1 of the first PS2 Board, if there is one; port 2 has nothing in
//it.  The HCP65's screen shows the first Video Board's picture (CHCP65ScreenDraw); it takes the host's keys while it has
//the focus, which it has from the start, and gives each going down and up to the keyboard.
//
//The HCP65 menu's Board Configuration changes the boards: on OK the description is written and the HCP65 is reset.  It
//runs on while the dialog is up.
class CHCP65EmulatorApp : public CObject
{
CONSTRUCTABLE(CHCP65EmulatorApp);
DESTRUCTABLE(CHCP65EmulatorApp);
protected:
	CHCP65					mcHCP65;
	CHCP65Mainboard			mcMainboard;
	CHCP65EmulatorDebugger	mcDebugger;
	CHCP65EmulatorRunner	mcRunner;
	Ptr<CWindow>			mpWindow;
	Ptr<CMenuBar>			mpMenuBar;
	CFileMenuListener		mcFileMenuListener;
	CHCP65MenuListener		mcHCP65MenuListener;
	Ptr<CHCP65EmulatorTick>	mpTick;
	Ptr<CHCP65SoundVoice>	mpSoundVoice;
	Ptr<CWindowTheme>		mpTheme;
	SDialogLook				msDialogLook;
	Ptr<CBoardConfigurationDialog>	mpBoardDialog;	//While it's shown.
	Ptr<CBoardSceneDraw>	mpBoardScene;	//Made when the dialog is first shown and kept, as it holds the models.  NULL if there's no model.
	CChars					mszMachine;		//The mainboard description the boards were read from.
	Ptr<CInput>				mpInput;
	Ptr<CWindowInputSource>	mpInputSource;
	CComponentInput			mcComponentInput;
	CHCP65PS2Keyboard		mcKeyboard;
	Ptr<CHCP65ScreenDraw>	mpScreenDraw;
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
	void	ShowBoardConfiguration(void);

protected:
	void	ChangeBoards(const char** aszTypes, const uint8* auiBreakouts);
	void	PlugInDevices(void);
	void	KeyDownAction(SComponentInputEvent* psEvent);
	void	KeyUpAction(SComponentInputEvent* psEvent);
};


bool HCP65EmulatorRun(CNativeWindowFactory* pcNativeFactory, CNativeSoundFactory* pcSoundFactory, CNativeInput* pcNativeInput, const char* szInputDirectory, int argc, char* argv[]);


#endif // __HCP65_EMULATOR_H__
