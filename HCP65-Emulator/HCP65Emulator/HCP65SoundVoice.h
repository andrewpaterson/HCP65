#ifndef __HCP65_SOUND_VOICE_H__
#define __HCP65_SOUND_VOICE_H__
#include "WindowLib/SoundStreamVoice.h"


#define HCP65_SOUND_VOICE_BUFFER_FRAMES		0x1000


class CHCP65Audio;


//What the HCP65's sound board played, as a voice in the window's sound mixer: its 8 bit unsigned left and right
//frames at the board's sample rate.  The board makes no frames while it is disabled, so the voice is silent, and so
//it is with no board (NULL).
class CHCP65SoundVoice : public CSoundStreamVoice
{
CONSTRUCTABLE(CHCP65SoundVoice);
DESTRUCTABLE(CHCP65SoundVoice);
protected:
	CHCP65Audio*	mpcAudio;

public:
	void	Init(CHCP65Audio* pcAudio);
	void	Class(void) override;

	void	SetAudio(CHCP65Audio* pcAudio);

	lsize	Fill(Ptr<CSound> pBuffer) override;
	uint32	GetStreamSampleRate(void) override;
};


#endif // __HCP65_SOUND_VOICE_H__

