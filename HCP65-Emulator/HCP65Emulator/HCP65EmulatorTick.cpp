#include <stdio.h>
#include <stdlib.h>
#include "StandardLib/ClassDefines.h"
#include "WindowLib/Window.h"
#include "HCP65Lib/HCP65.h"
#include "HCP65Lib/HCP65Audio.h"
#include "HCP65SoundVoice.h"
#include "HCP65EmulatorRunner.h"
#include "HCP65EmulatorTick.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65EmulatorTick::Init(CHCP65* pcHCP65, CHCP65Audio* pcAudio, Ptr<CHCP65SoundVoice> pSoundVoice, CHCP65EmulatorRunner* pcRunner)
{
	PreInit();

	CWindowTick::Init();
	mpcHCP65 = pcHCP65;
	mpcAudio = pcAudio;
	mpSoundVoice = pSoundVoice;
	mpcRunner = pcRunner;
	miStopTime = getenv("HCP65_EMULATOR_SECONDS") ? (int64)atoi(getenv("HCP65_EMULATOR_SECONDS")) * 1000 : -1;
	miReportTime = getenv("HCP65_EMULATOR_TRACE") ? 0 : -1;

	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65EmulatorTick::Class(void)
{
	CWindowTick::Class();
	U_Pointer(mpcHCP65);
	U_Pointer(mpcAudio);
	M_Pointer(mpSoundVoice);
	U_Pointer(mpcRunner);
	U_Int64(miStopTime);
	U_Int64(miReportTime);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65EmulatorTick::Free(void)
{
	CWindowTick::Free();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65EmulatorTick::Tick(Ptr<CWindow> pWindow, int64 iUpdateTimeInMillieconds, int64 iTotalTimeInMillieconds)
{
	if (mpcRunner->IsFinished())
	{
		pWindow->Stop();
	}
	if (miReportTime >= 0)
	{
		Report(pWindow, iTotalTimeInMillieconds);
	}
	if ((miStopTime >= 0) && (iTotalTimeInMillieconds >= miStopTime))
	{
		pWindow->Stop();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The sound board that is heard has changed, NULL if there is none.  The
// HCP65 is paused, as its boards have changed.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65EmulatorTick::SetAudio(CHCP65Audio* pcAudio)
{
	mpcAudio = pcAudio;
	mpSoundVoice->SetAudio(pcAudio);
}


//////////////////////////////////////////////////////////////////////////
//
// HCP65_EMULATOR_TRACE: once a second, how far the HCP65 has got and what
// it has played.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65EmulatorTick::Report(Ptr<CWindow> pWindow, int64 iTotalTimeInMillieconds)
{
	Ptr<CSoundMixer>	pMixer;

	if (iTotalTimeInMillieconds < miReportTime)
	{
		return;
	}
	miReportTime = iTotalTimeInMillieconds + 1000;
	pMixer = pWindow->GetSoundMixer();
	if (mpcHCP65)
	{
		printf("%6.1f s: %llu cycles (%.1f s of HCP65 time), PC $%06X, process $%02X, sound %s at %u Hz, %llu frames played, %llu dropped by the board, %llu by the voice (%llu underruns), %llu by the host\n",
			(double)iTotalTimeInMillieconds / 1000.0,
			(unsigned long long)mpcHCP65->NumCycles(),
			(double)mpcHCP65->NumCycles() / (double)mpcHCP65->GetClockRate(),
			mpcHCP65->GetMPU()->GetProgramAddress(),
			mpcHCP65->GetProcessID(),
			(mpcAudio && mpcAudio->IsEnabled()) ? "on" : "off",
			mpcAudio ? mpcAudio->GetSampleRate() : 0,
			mpSoundVoice.IsNotNull() ? (unsigned long long)mpSoundVoice->NumFramesPlayed() : 0ULL,
			mpcAudio ? (unsigned long long)mpcAudio->NumDroppedFrames() : 0ULL,
			mpSoundVoice.IsNotNull() ? (unsigned long long)mpSoundVoice->NumDroppedFrames() : 0ULL,
			mpSoundVoice.IsNotNull() ? (unsigned long long)mpSoundVoice->NumUnderruns() : 0ULL,
			pMixer.IsNotNull() ? (unsigned long long)pMixer->NumDroppedFrames() : 0ULL);
		fflush(stdout);
	}
}
