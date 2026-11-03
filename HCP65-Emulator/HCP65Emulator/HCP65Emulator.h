#ifndef __HCP65_EMULATOR_H__
#define __HCP65_EMULATOR_H__


//Relative to the emulator's directory (HCP65Emulator/HCP65Emulator), as Visual Studio runs it.  TestHCP65Lib's
//TestHCP65Kernal builds the ROM, and the SD card image (a copy of the blank Input/SDCard.img with boot.exe, modplay.exe
//and SpaceDebris.mod on it), into its Output: run it first.
#define HCP65_EMULATOR_KERNAL_ROM		"../TestHCP65Lib/TestHCP65Lib/Output/Kernal.rom"
#define HCP65_EMULATOR_SD_CARD_IMAGE	"../TestHCP65Lib/TestHCP65Lib/Output/SDCard.img"
#define HCP65_EMULATOR_SD_SOCKET		2			//Socket 1 is the main memory channel.
#define HCP65_EMULATOR_AUDIO_SOCKET		16


class CNativeWindowFactory;
class CNativeSoundFactory;
class CInput;


bool HCP65EmulatorRun(CNativeWindowFactory* pcNativeFactory, CNativeSoundFactory* pcSoundFactory, CInput* pcInput);


#endif // __HCP65_EMULATOR_H__
