#include <stdio.h>
#include <stdlib.h>
#include "StandardLib/ClassDefines.h"
#include "WindowLib/Window.h"
#include "HCP65Lib/HCP65.h"
#include "HCP65Lib/HCP65Audio.h"
#include "HCP65SoundVoice.h"
#include "HCP65EmulatorTick.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65EmulatorTick::Init(CHCP65* pcHCP65, CHCP65Audio* pcAudio, Ptr<CHCP65SoundVoice> pSoundVoice)
{
	PreInit();

	CWindowTick::Init();
	mpcHCP65 = pcHCP65;
	mpcAudio = pcAudio;
	mpSoundVoice = pSoundVoice;
	miStartTime = -1;
	muiStartCycles = 0;
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
	U_Int64(miStartTime);
	U_UInt64(muiStartCycles);
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
	if (mpcHCP65 && !mpcHCP65->IsStopped())
	{
		Run(iTotalTimeInMillieconds);
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
// The HCP65 runs to where its clock should be by now, at most
// HCP65_EMULATOR_MAX_TICK_MS of its time.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65EmulatorTick::Run(int64 iTotalTimeInMillieconds)
{
	uint64	uiClockRate;
	uint64	uiTarget;
	uint64	uiCycles;
	uint64	uiMaxCycles;

	uiClockRate = mpcHCP65->GetClockRate();
	if (miStartTime < 0)
	{
		miStartTime = iTotalTimeInMillieconds;
		muiStartCycles = mpcHCP65->NumCycles();
	}

	uiTarget = muiStartCycles + (uiClockRate * (uint64)(iTotalTimeInMillieconds - miStartTime)) / 1000;
	if (uiTarget <= mpcHCP65->NumCycles())
	{
		return;
	}
	uiCycles = uiTarget - mpcHCP65->NumCycles();
	uiMaxCycles = (uiClockRate * HCP65_EMULATOR_MAX_TICK_MS) / 1000;
	if (uiCycles > uiMaxCycles)
	{
		//The host can't keep up: the HCP65's time falls behind the window's (the start goes back by the cycles not run,
		//so the next target is from where it got to; it may wrap, but the targets made from it don't).
		muiStartCycles -= uiCycles - uiMaxCycles;
		uiCycles = uiMaxCycles;
	}
	mpcHCP65->Run(uiCycles);
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
