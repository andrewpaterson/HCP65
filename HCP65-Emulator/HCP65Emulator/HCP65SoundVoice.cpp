#include "StandardLib/ClassDefines.h"
#include "HCP65Lib/HCP65.h"
#include "HCP65Lib/HCP65Audio.h"
#include "HCP65SoundVoice.h"


//////////////////////////////////////////////////////////////////////////
//
// The buffer's frames are the board's: unsigned 8 bit left and right
// pairs, so ReadFrames writes straight into it.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65SoundVoice::Init(CHCP65Audio* pcAudio)
{
	PreInit();
	CSoundStreamVoice::Init(pcAudio ? pcAudio->GetSampleRate() : HCP65_CRYSTAL_RATE, HCP65_SOUND_VOICE_BUFFER_FRAMES, PT_uint8, true);
	mpcAudio = pcAudio;
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65SoundVoice::Class(void)
{
	CSoundStreamVoice::Class();
	U_Pointer(mpcAudio);
}


//////////////////////////////////////////////////////////////////////////
//
// The board to play from now on, or NULL for none.  What another board
// played while no one was listening isn't played.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65SoundVoice::SetAudio(CHCP65Audio* pcAudio)
{
	if ((pcAudio != mpcAudio) && pcAudio)
	{
		pcAudio->ClearFrames();
	}
	mpcAudio = pcAudio;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
lsize CHCP65SoundVoice::Fill(CSound* pcBuffer)
{
	if (mpcAudio == NULL)
	{
		return 0;
	}
	return mpcAudio->ReadFrames((uint8*)pcBuffer->GetData(), (uint32)pcBuffer->GetNumFrames());
}


//////////////////////////////////////////////////////////////////////////
//
// The rate the board's flags select, which changing restarts the stream.
// With no board the stream isn't playing.
//
//////////////////////////////////////////////////////////////////////////
uint32 CHCP65SoundVoice::GetStreamSampleRate(void)
{
	return mpcAudio ? mpcAudio->GetSampleRate() : 0;
}

