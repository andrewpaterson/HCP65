#ifndef __HCP65_EMULATOR_H__
#define __HCP65_EMULATOR_H__


//Relative to the emulator's directory (HCP65Emulator/HCP65Emulator), as Visual Studio runs it.  TestHCP65Lib's
//TestHCP65Kernal builds the ROM, and the SD card image (a copy of the blank Input/SDCard.img with boot.exe, modplay.exe
//and SpaceDebris.mod on it), into its Output: run it first.
#define HCP65_EMULATOR_KERNAL_ROM		"../TestHCP65Lib/TestHCP65Lib/Output/Kernal.rom"
#define HCP65_EMULATOR_SD_CARD_IMAGE	"../TestHCP65Lib/TestHCP65Lib/Output/SDCard.img"
#define HCP65_EMULATOR_SD_SOCKET		2			//Socket 1 is the main memory channel.
#define HCP65_EMULATOR_AUDIO_SOCKET		16


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
#include "FileMenuListener.h"


//The emulator: the HCP65 (its kernal ROM, the sound board and an SD card board with HCP65_EMULATOR_SD_CARD_IMAGE) in a
//window with a menu bar, and the input that drives it.  HCP65EmulatorRun makes it, adds it to the root, runs it and removes
//it from the root.
class CHCP65EmulatorApp : public CObject
{
CONSTRUCTABLE(CHCP65EmulatorApp);
DESTRUCTABLE(CHCP65EmulatorApp);
protected:
	CHCP65					mcHCP65;
	CHCP65Audio				mcAudio;
	CHCP65SDCard			mcSDCard;
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
	bool	Setup(CNativeWindowFactory* pcNativeFactory, CNativeSoundFactory* pcSoundFactory, CNativeInput* pcNativeInput, const char* szInputDirectory);
	void	Class(void) override;
	void	Free(void) override;

	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	bool	Run(void);
};


bool HCP65EmulatorRun(CNativeWindowFactory* pcNativeFactory, CNativeSoundFactory* pcSoundFactory, CNativeInput* pcNativeInput, const char* szInputDirectory);


#endif // __HCP65_EMULATOR_H__
