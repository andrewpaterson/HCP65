#include <stdlib.h>
#include <string.h>
#include "BaseLib/TypeNames.h"
#include "BaseLib/TypeConverter.h"
#include "BaseLib/GlobalMemory.h"
#include "BaseLib/GlobalDataTypesIO.h"
#include "StandardLib/Objects.h"
#include "SupportLib/MODReader.h"
#include "SupportLib/ModPlayer.h"
#include "HCP65User.h"
#include "HCP65SoundBoard.h"
#include "ModPlaySamples.h"


SModUnsigned8	gsModPlayBlock;		//A block as the sound board plays it (MODPLAY_BLOCK_FRAMES is MOD_MIX_BLOCK).
uint16			guiModPlayLeft;		//The handles on the first sound board's units (KernalDeviceOpen).
uint16			guiModPlayRight;


//////////////////////////////////////////////////////////////////////////
//
// Writes one of the sound board's registers.
//
//////////////////////////////////////////////////////////////////////////
void ModPlaySetRegister(uint16 uiRegister, uint16 uiValue)
{
	KernalDeviceControl(guiModPlayLeft, SOUND_BOARD_SET_REGISTER + uiRegister, uiValue, NULL);
}


//////////////////////////////////////////////////////////////////////////
//
// The sound board's counter: the address of the samples it plays next.
//
//////////////////////////////////////////////////////////////////////////
uint16 ModPlayCounter(void)
{
	uint32	uiCounter;

	uiCounter = 0;
	KernalDeviceControl(guiModPlayLeft, SOUND_BOARD_GET_COUNTER, 0, &uiCounter);
	return (uint16)uiCounter;
}


//////////////////////////////////////////////////////////////////////////
//
// Copies uiFrames of the block to the sound board's RAMs at uiWrite.
//
//////////////////////////////////////////////////////////////////////////
void ModPlayWrite(uint16 uiWrite, uint16 uiFrames)
{
	KernalDeviceWrite(guiModPlayLeft, uiWrite, gsModPlayBlock.auiLeft, uiFrames);
	KernalDeviceWrite(guiModPlayRight, uiWrite, gsModPlayBlock.auiRight, uiFrames);
}


//////////////////////////////////////////////////////////////////////////
//
// Renders the next block (or what is left of the song, or of the frames
// to play) and writes it to the sound board at uiWrite.  The number of
// frames written.
//
//////////////////////////////////////////////////////////////////////////
uint16 ModPlayBlock(CModPlayer* pcPlayer, uint16 uiWrite, lsize uiFramesLeft)
{
	lsize	uiFrames;

	uiFrames = (uiFramesLeft < MODPLAY_BLOCK_FRAMES) ? uiFramesLeft : MODPLAY_BLOCK_FRAMES;
	uiFrames = pcPlayer->Render8(&gsModPlayBlock, uiFrames);
	if (uiFrames > 0)
	{
		ModPlayWrite(uiWrite, (uint16)uiFrames);
	}
	return (uint16)uiFrames;
}


//////////////////////////////////////////////////////////////////////////
//
// Frames written to the sound board that it hasn't played yet.
//
//////////////////////////////////////////////////////////////////////////
uint16 ModPlayQueued(uint16 uiWrite)
{
	return (uint16)((uiWrite - ModPlayCounter()) & (MODPLAY_BUFFER_FRAMES - 1));
}


//////////////////////////////////////////////////////////////////////////
//
// Plays the song through the sound board: renders MODPLAY_START_FRAMES,
// starts it, then renders a block whenever there is room for one (it
// keeps up to MODPLAY_MAX_QUEUED ahead, sleeping in KernalDeviceWait until
// the board's interrupt says there is room), until the song ends.
// The sound board plays its 32KB again and again, so after the song a
// block of silence follows and it is stopped once it reaches it.
//
//////////////////////////////////////////////////////////////////////////
void ModPlaySong(CModPlayer* pcPlayer, lsize uiMaxFrames)
{
	uint16	uiWrite;
	uint16	uiWritten;
	lsize	uiPlayed;
	uint16	i;
	bool	bStarted;

	ModPlaySetRegister(SOUND_BOARD_FLAGS, MODPLAY_RATE_SELECT);
	ModPlaySetRegister(SOUND_BOARD_START_LOW, 0);
	ModPlaySetRegister(SOUND_BOARD_START_HIGH, 0);
	ModPlaySetRegister(SOUND_BOARD_END_LOW, (MODPLAY_BUFFER_FRAMES - 1) & 0xFF);
	ModPlaySetRegister(SOUND_BOARD_END_HIGH, (MODPLAY_BUFFER_FRAMES - 1) >> 8);

	uiWrite = 0;
	uiPlayed = 0;
	bStarted = false;
	while (uiPlayed < uiMaxFrames)
	{
		if (bStarted && (ModPlayQueued(uiWrite) > MODPLAY_MAX_QUEUED - MODPLAY_BLOCK_FRAMES))
		{
			//There's room for a block once the board has played up to this far behind the write.
			KernalDeviceWait(guiModPlayLeft, (uiWrite - (MODPLAY_MAX_QUEUED - MODPLAY_BLOCK_FRAMES)) & (MODPLAY_BUFFER_FRAMES - 1), SOUND_BOARD_WAIT_TICKS);
			continue;
		}
		uiWritten = ModPlayBlock(pcPlayer, uiWrite, uiMaxFrames - uiPlayed);
		if (uiWritten == 0)
		{
			break;
		}
		uiWrite = (uint16)((uiWrite + uiWritten) & (MODPLAY_BUFFER_FRAMES - 1));
		uiPlayed += uiWritten;
		if (!bStarted && ((uiWrite >= MODPLAY_START_FRAMES) || (uiPlayed >= uiMaxFrames)))
		{
			ModPlaySetRegister(SOUND_BOARD_FLAGS, SOUND_BOARD_FLAG_ENABLE | MODPLAY_RATE_SELECT);
			bStarted = true;
		}
	}
	if (!bStarted)
	{
		ModPlaySetRegister(SOUND_BOARD_FLAGS, SOUND_BOARD_FLAG_ENABLE | MODPLAY_RATE_SELECT);
	}

	//Silence after the end, then wait for the last of the song to play.
	for (i = 0; i < MODPLAY_BLOCK_FRAMES; i++)
	{
		gsModPlayBlock.auiLeft[i] = 0x80;
		gsModPlayBlock.auiRight[i] = 0x80;
	}
	ModPlayWrite(uiWrite, MODPLAY_BLOCK_FRAMES);
	while (ModPlayQueued(uiWrite) != 0)
	{
		KernalDeviceWait(guiModPlayLeft, uiWrite, SOUND_BOARD_WAIT_TICKS);
	}
	ModPlaySetRegister(SOUND_BOARD_FLAGS, MODPLAY_RATE_SELECT);
}


//////////////////////////////////////////////////////////////////////////
//
// modplay.exe: plays Space Debris from the SD card through the sound
// board.  Its argument is how many seconds to play, zero for all of it.
//
//////////////////////////////////////////////////////////////////////////
int main(void)
{
	CModPlayer		cPlayer;
	Ptr<CModSong>	pcSong;
	uint32			uiSize;
	void*			pvFile;
	lsize			uiMaxFrames;
	uint16			uiSeconds;
	int				iResult;

	uiSeconds = KernalGetArgument();
	uiMaxFrames = (uiSeconds == 0) ? (lsize)-1 : (lsize)uiSeconds * MODPLAY_SAMPLE_RATE;

	uiSize = KernalFileSize(MODPLAY_FILE);
	if (uiSize == KERNAL_FILE_NOT_FOUND)
	{
		return MODPLAY_EXIT_NO_FILE;
	}
	pvFile = malloc(uiSize);
	if (!pvFile)
	{
		return MODPLAY_EXIT_NO_MEMORY;
	}
	if (KernalFileLoad(MODPLAY_FILE, pvFile, uiSize) != KERNAL_RESULT_SUCCESS)
	{
		free(pvFile);
		return MODPLAY_EXIT_NO_FILE;
	}
	if ((KernalDeviceOpen(SOUND_BOARD_MANUFACTURER, SOUND_BOARD_NAME, 0, SOUND_BOARD_LEFT, &guiModPlayLeft) != KERNAL_DEVICE_SUCCESS) ||
		(KernalDeviceOpen(SOUND_BOARD_MANUFACTURER, SOUND_BOARD_NAME, 0, SOUND_BOARD_RIGHT, &guiModPlayRight) != KERNAL_DEVICE_SUCCESS))
	{
		free(pvFile);
		return MODPLAY_EXIT_NO_SOUND_BOARD;
	}

	TypesInit();
	TypeConverterInit();
	MemoryInit();
	DataIOInit();
	ObjectsInit();

	pcSong = LoadMOD(pvFile, uiSize);
	if (pcSong.IsNull())
	{
		iResult = (memcmp((char*)pvFile + 1080, "M.K.", 4) == 0) ? MODPLAY_EXIT_NOT_A_MODULE : MODPLAY_EXIT_BAD_LOAD;
		free(pvFile);
	}
	else
	{
		free(pvFile);
		cPlayer.Init(pcSong, MODPLAY_SAMPLE_RATE, MI_Nearest, 1.0);
		ModPlaySong(&cPlayer, uiMaxFrames);
		cPlayer.Kill();
		iResult = MODPLAY_EXIT_SUCCESS;
	}
	pcSong = NULL;
	KernalDeviceClose(guiModPlayRight);
	KernalDeviceClose(guiModPlayLeft);

	ObjectsKill(false);
	DataIOKill();
	MemoryKill();
	TypeConverterKill();
	TypesKill();
	return iResult;
}
