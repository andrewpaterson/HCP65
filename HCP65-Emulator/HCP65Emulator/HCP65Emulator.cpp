#include "BaseLib/Logger.h"
#include "BaseLib/TypeNames.h"
#include "BaseLib/TypeConverter.h"
#include "BaseLib/GlobalMemory.h"
#include "BaseLib/GlobalDataTypesIO.h"
#include "StandardLib/Objects.h"
#include "SupportLib/ImageColour.h"
#include "SupportLib/ImageColourConverter.h"
#include "SupportLib/TrueTypeFont.h"
#include "SupportLib/TrueTypeFontFactory.h"
#include "SupportLib/FontColours.h"
#include "InputLib/Input.h"
#include "WindowLib/Window.h"
#include "WindowLib/ClearCanvasDraw.h"
#include "WindowLib/GridContainer.h"
#include "WindowLib/Canvas.h"
#include "WindowLib/ComponentInput.h"
#include "WindowLib/JewelTheme.h"
#include "WindowLib/MenuBar.h"
#include "BaseLib/DiskFile.h"
#include "WindowLib/NativeWindowFactory.h"
#include "WindowLib/NativeSoundFactory.h"
#include "HCP65Lib/HCP65.h"
#include "HCP65Lib/HCP65Audio.h"
#include "HCP65Lib/HCP65SDCard.h"
#include "HCP65SoundVoice.h"
#include "HCP65Emulator.h"
#include "HCP65EmulatorTick.h"
#include "FileMenuListener.h"


#define HCP65_EMULATOR_FONT_CHARACTERS	" !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~"


//////////////////////////////////////////////////////////////////////////
//
// Rasterises the glyphs from the TrueType font in white into an image in
// the window's colour order and makes a font from them.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CFont> GenerateFont(const char* szFontFilename, const char* szName, float32 fPixelSize, EColourOrder eOrder)
{
	CTrueTypeFont			cTrueTypeFont;
	CTrueTypeFontFactory	cFactory;
	CImageColourRGB			cColour;
	CChars					szCharacters;
	Ptr<CFontDescription>	pDescription;
	Ptr<CImage>				pImage;
	Ptr<CFont>				pFont;

	cTrueTypeFont.Init();
	if (!cTrueTypeFont.Load((char*)szFontFilename))
	{
		cTrueTypeFont.Kill();
		gcLogger.Error2(__METHOD__, " Could not load font [", szFontFilename, "].", NULL);
		return NULL;
	}

	cColour.Init(1.0f, 1.0f, 1.0f);
	szCharacters.Init(HCP65_EMULATOR_FONT_CHARACTERS);
	cFactory.Init(&cTrueTypeFont, fPixelSize);
	pDescription = cFactory.Describe((char*)szName, szCharacters.Text());
	pImage = cFactory.Rasterise(pDescription, &cColour);
	if (pImage.IsNotNull())
	{
		if (pImage->GetColourOrder() != eOrder)
		{
			pImage = CImageColourConverter::ConvertColour(pImage, eOrder);
		}
		pFont = OMalloc<CFont>(pDescription, pImage);
	}
	cFactory.Kill();
	szCharacters.Kill();
	cTrueTypeFont.Kill();

	if (pFont.IsNull())
	{
		gcLogger.Error2(__METHOD__, " Could not make a font from [", szFontFilename, "].", NULL);
	}
	return pFont;
}


//////////////////////////////////////////////////////////////////////////
//
// The kernal ROM image szFilename into the HCP65's ROM.
//
//////////////////////////////////////////////////////////////////////////
bool LoadKernal(CHCP65* pcHCP65, const char* szFilename)
{
	CDiskFile	cFile;
	uint8*		puiROM;
	lsize		uiSize;
	bool		bResult;

	cFile.Init(szFilename);
	if (!cFile.Open(EFM_Read))
	{
		cFile.Kill();
		gcLogger.Error2(__METHOD__, " Could not open the kernal ROM [", szFilename, "].", NULL);
		return false;
	}
	uiSize = (lsize)cFile.Size();
	if (uiSize > HCP65_MPU_ROM_SIZE)
	{
		uiSize = HCP65_MPU_ROM_SIZE;
	}
	puiROM = (uint8*)malloc(uiSize);
	bResult = cFile.Read(puiROM, uiSize, 1) == 1;
	cFile.Close();
	cFile.Kill();
	if (bResult)
	{
		bResult = pcHCP65->LoadROM(puiROM, (size)uiSize);
	}
	free(puiROM);
	if (!bResult)
	{
		gcLogger.Error2(__METHOD__, " Could not load the kernal ROM [", szFilename, "].", NULL);
	}
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The machine, before it is set up.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65EmulatorApp::Init(void)
{
	PreInit();

	mcHCP65.Init();
	mcMainboard.Init();
	mcDebugger.Init();
	mcRunner.Init(&mcHCP65, &mcDebugger);
	memset(&msDialogLook, 0, sizeof(msDialogLook));
	mszMachine.Init();
	mbValid = false;
	mbInputs = false;

	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
// The machine and its window.  The devices are described by the XML in
// szInputDirectory; pcNativeInput (the platform's keyboard and mouse) is
// added to them.  Run fails if anything here did.
// Call it once the app is in the root, so that what is made here is
// built under the root rather than as an unrooted graph.
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65EmulatorApp::Setup(CNativeWindowFactory* pcNativeFactory, CNativeSoundFactory* pcSoundFactory, CNativeInput* pcNativeInput, const char* szInputDirectory, int argc, char* argv[])
{
	Ptr<CMenuContainer>		pFileMenu;
	Ptr<CMenuContainer>		pHCP65Menu;
	Ptr<CCanvas>			pScreen;
	Ptr<CGridContainer>		pMain;
	Ptr<CClearCanvasDraw>	pBackground;
	Ptr<CFont>				pWhite;
	Ptr<CFontColours>		pFontColours;
	const char*				szCard;
	const char*				szMachine;
	CChars					szError;
	CHCP65Audio*			pcAudio;
	int						i;

	//Debuggable: it runs until a debugger attaches, or with --debug-wait holds stopped until one continues it.  A
	//debugger it waits for may load the kernal itself, so then the ROM file needn't be there.
	mbValid = mcDebugger.ParseArguments(argc, argv);
	if (mbValid && !LoadKernal(&mcHCP65, HCP65_EMULATOR_KERNAL_ROM))
	{
		if (mcDebugger.IsWaiting())
		{
			gcLogger.Warning2(__METHOD__, " Starting without a kernal ROM: the debugger is to load one.", NULL);
		}
		else
		{
			mbValid = false;
		}
	}
	//The boards are --machine's description's, or HCP65_EMULATOR_MACHINE's.  --card's disk image replaces the first SD
	//Card Board's card.
	szCard = DebugLaunchValue(argc, argv, DEBUG_ARGUMENT_CARD);
	if (szCard && (szCard[0] == '\0'))
	{
		gcLogger.Error2(__METHOD__, " " DEBUG_ARGUMENT_CARD " needs a disk image file.", NULL);
		mbValid = false;
	}
	else if (szCard)
	{
		mcMainboard.SetCard(szCard);
	}
	szMachine = DebugLaunchValue(argc, argv, HCP65_EMULATOR_ARGUMENT_MACHINE);
	if (szMachine && (szMachine[0] == '\0'))
	{
		gcLogger.Error2(__METHOD__, " " HCP65_EMULATOR_ARGUMENT_MACHINE " needs a mainboard description file.", NULL);
		mbValid = false;
	}
	if (!szMachine || (szMachine[0] == '\0'))
	{
		szMachine = HCP65_EMULATOR_MACHINE;
	}
	mszMachine.Set(szMachine);
	szError.Init();
	if (!mcMainboard.Read(szMachine, &szError))
	{
		gcLogger.Error2(__METHOD__, " ", szError.Text(), ".", NULL);
		mbValid = false;
	}
	else if (!mcMainboard.PlugIn(&mcHCP65))
	{
		gcLogger.Error2(__METHOD__, " Could not plug in the boards of [", szMachine, "].", NULL);
		mbValid = false;
	}
	szError.Kill();
	pcAudio = (CHCP65Audio*)mcMainboard.FindBoard(HCP65_AUDIO_NAME);
	for (i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], HCP65_EMULATOR_ARGUMENT_FILL) == 0)
		{
			mcHCP65.SetMemoryFill(true);
		}
	}
	mcHCP65.Reset();

	if (mbValid && mcDebugger.IsEnabled())
	{
		mbValid = mcDebugger.Start(&mcHCP65);
	}

	if (mbValid)
	{
		mpSoundVoice = OMalloc<CHCP65SoundVoice>(pcAudio);  //Even with no Sound Board: one may be plugged in.
		mpTick = OMalloc<CHCP65EmulatorTick>(&mcHCP65, pcAudio, mpSoundVoice, &mcRunner);
		pBackground = OMalloc<CClearCanvasDraw>(0);  //Coloured once there is a theme.
		mpWindow = OMalloc<CWindow>("HCP65 Emulator", pcNativeFactory, pcSoundFactory, mpTick, pBackground, CFT_RGBX, CCO_BGR, CRGB_24bit, ARGB_8bit);
		mpWindow->SetContinuous(true);  //The emulator runs and its screen changes every tick.

		//Only what changed is drawn.  HCP65_EMULATOR_VERIFY_REDRAW checks every partial paint against drawing all of
		//the window and logs where they differ, for finding a component that changes without being invalidated.
		mpWindow->SetPartialRedraw(true);
		mpWindow->SetVerifyRedraw(getenv("HCP65_EMULATOR_VERIFY_REDRAW") != NULL);
		mpWindow->GetSoundMixer()->AddVoice(mpSoundVoice);

		//The font is white and coloured as it is drawn.
		pWhite = GenerateFont("Resources/InterVariable.ttf", "Inter 13px", 13.0f, mpWindow->GetColourOrder());
		mbValid = pWhite.IsNotNull();
	}

	if (mbValid)
	{
		pFontColours = OMalloc<CFontColours>(pWhite);
		mpTheme = CJewelTheme::CreateDarkTheme(mpWindow, pFontColours);
		pBackground->SetColour(mpTheme->GetColour(TC_Panel));
		msDialogLook.pcTheme = &mpTheme;
		msDialogLook.pcDimFont = &mpTheme->GetFont();

		//The menu bar along the top over the HCP65's screen, which is only a blank canvas so far.
		mpMenuBar = OMalloc<CMenuBar>(mpWindow, mpTheme);
		mpMenuBar->SetAlignment(CSH_Stretch, CSV_Stretch);
		pFileMenu = OMalloc<CMenuContainer>(mpWindow, mpTheme);
		mpMenuBar->AddMenu("File", pFileMenu);
		pHCP65Menu = OMalloc<CMenuContainer>(mpWindow, mpTheme);
		mpMenuBar->AddMenu("HCP65", pHCP65Menu);
		pScreen = OMalloc<CCanvas>(mpWindow, OMalloc<CClearCanvasDraw>(mpTheme->GetColour(TC_Editor)));
		pScreen->SetDrawnEveryPaint(true);  //The emulated screen changes every tick without telling the canvas.
		pScreen->SetAlignment(CSH_Stretch, CSV_Stretch);
		pMain = OMalloc<CGridContainer>(mpWindow);
		pMain->AddComponent(mpMenuBar, 0, 0, 1, 1, 1, 0);
		pMain->AddComponent(pScreen, 0, 1, 1, 1, 1, 1);
		mpWindow->SetContainer(pMain);

		mcFileMenuListener.Init(mpWindow, pFileMenu, &mcRunner);
		mpMenuBar->GetItemClickEvent()->Add(&mcFileMenuListener, &CFileMenuListener::MenuItemClicked);
		mcHCP65MenuListener.Init(mpWindow, pHCP65Menu, this);
		mpMenuBar->GetItemClickEvent()->Add(&mcHCP65MenuListener, &CHCP65MenuListener::MenuItemClicked);

		mpInput = OMalloc<CInput>((CNativeInput*)NULL);
		mbValid = mpInput->Read((char*)szInputDirectory);
		mpInput->AddNativeInput(pcNativeInput);
		mpInputSource = OMalloc<CWindowInputSource>(mpInput);
		mcComponentInput.Init(&mpInputSource, &mpWindow);
		mbInputs = true;
		mcComponentInput.AddKeyboards();
		mcComponentInput.AddMice();
		mpWindow->SetComponentInput(&mcComponentInput);
		mpWindow->SetWindowSize(1920, 1200);
	}
	return mbValid;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65EmulatorApp::Class(void)
{
	U_Unknown(CHCP65, mcHCP65);
	U_Unknown(CHCP65Mainboard, mcMainboard);
	U_Unknown(CHCP65EmulatorDebugger, mcDebugger);
	U_Unknown(CHCP65EmulatorRunner, mcRunner);
	M_Pointer(mpWindow);
	M_Pointer(mpMenuBar);
	U_Unknown(CFileMenuListener, mcFileMenuListener);
	U_Unknown(CHCP65MenuListener, mcHCP65MenuListener);
	M_Pointer(mpTick);
	M_Pointer(mpSoundVoice);
	M_Pointer(mpTheme);
	U_Pointer(msDialogLook.pcTheme);
	U_Pointer(msDialogLook.pcDimFont);
	M_Pointer(mpBoardDialog);
	M_Pointer(mpBoardScene);
	U_String(mszMachine);
	M_Pointer(mpInput);
	M_Pointer(mpInputSource);
	U_Unknown(CComponentInput, mcComponentInput);
	U_Bool(mbValid);
	U_Bool(mbInputs);
}


//////////////////////////////////////////////////////////////////////////
//
// Only this object's own members: the window, the menu bar and the input
// may already be freed (Run let go of them).
//
//////////////////////////////////////////////////////////////////////////
void CHCP65EmulatorApp::Free(void)
{
	if (mbInputs)
	{
		if (mpMenuBar.IsNotNull())
		{
			mpMenuBar->GetItemClickEvent()->Remove(&mcFileMenuListener);
			mpMenuBar->GetItemClickEvent()->Remove(&mcHCP65MenuListener);
		}
		mcHCP65MenuListener.Kill();
		mcFileMenuListener.Kill();
		mcComponentInput.Kill();
	}
	msDialogLook.Kill();
	mszMachine.Kill();
	mcRunner.Kill();
	mcDebugger.Kill();
	mcHCP65.Kill();
	mcMainboard.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65EmulatorApp::Save(CObjectWriter* pcFile)
{
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65EmulatorApp::Load(CObjectReader* pcFile)
{
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// Runs the window's ticks until the window closes (if Init worked), then
// takes the menu bar's listener and the component input out of the window.
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65EmulatorApp::Run(void)
{
	if (mbValid)
	{
		mbValid = mcRunner.Start();
	}
	if (mbValid)
	{
		mpWindow->Show();
	}
	mcRunner.Stop();

	if (mbInputs)
	{
		mpWindow->CancelPosted(this);
		mpMenuBar->GetItemClickEvent()->Remove(&mcFileMenuListener);
		mpMenuBar->GetItemClickEvent()->Remove(&mcHCP65MenuListener);
		mpWindow->SetComponentInput(NULL);
	}
	return mbValid;
}


//////////////////////////////////////////////////////////////////////////
//
// The Board Configuration dialog, from a tick.  OK writes the boards to
// the description and resets the HCP65; Cancel leaves everything as it
// was.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65EmulatorApp::ShowBoardConfiguration(void)
{
	const char*	aszTypes[HCP65_NUM_DEVICE_SLOTS];

	//Only the Small Mainboard has a model.
	if (mpBoardScene.IsNull() && (mcMainboard.GetType() == HMB_Small) && CBoardSceneDraw::HasMainboardModel())
	{
		mpBoardScene = OMalloc<CBoardSceneDraw>(mpTheme);
	}
	mpBoardDialog = OMalloc<CBoardConfigurationDialog>(mpWindow, &msDialogLook, &mcMainboard, mpBoardScene);
	if (mpBoardDialog->Show())
	{
		mpBoardDialog->GetTypes(aszTypes);
		ChangeBoards(aszTypes);
	}
	mpBoardDialog = NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The sockets hold aszTypes, the description says so, the first Sound
// Board is heard and the HCP65 is reset (through the debugger, if there
// is one, so it can tell its client).  The reset happens whatever failed.
// The HCP65 is paused while its boards change, and the sound mixer, whose
// thread plays the board, is locked while the board heard changes.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65EmulatorApp::ChangeBoards(const char** aszTypes)
{
	CChars	szError;

	mcRunner.Pause();
	szError.Init();
	if (!mcMainboard.ChangeBoards(&mcHCP65, aszTypes, &szError) || !mcMainboard.Write(mszMachine.Text(), &szError))
	{
		gcLogger.Error2(__METHOD__, " ", szError.Text(), ".", NULL);
	}
	szError.Kill();

	mpWindow->GetSoundMixer()->Lock();
	mpTick->SetAudio((CHCP65Audio*)mcMainboard.FindBoard(HCP65_AUDIO_NAME));
	mpWindow->GetSoundMixer()->Unlock();
	mcRunner.Reset();
	mcRunner.Resume();
}


//////////////////////////////////////////////////////////////////////////
//
// The emulator, held by the root while it runs.
//
//////////////////////////////////////////////////////////////////////////
bool HCP65EmulatorRun(CNativeWindowFactory* pcNativeFactory, CNativeSoundFactory* pcSoundFactory, CNativeInput* pcNativeInput, const char* szInputDirectory, int argc, char* argv[])
{
	Ptr<CHCP65EmulatorApp>	pApp;
	bool					bResult;

	//Validating every object each time one changes is too slow for the text's glyphs.  They are validated once at the end.
	gcObjects.DisableValidation();

	pApp = OMalloc<CHCP65EmulatorApp>();
	ORoot()->Add(pApp);
	bResult = pApp->Setup(pcNativeFactory, pcSoundFactory, pcNativeInput, szInputDirectory, argc, argv) && pApp->Run();
	ORoot()->Remove(pApp);
	pApp = NULL;

	gcObjects.EnableValidation();
	gcObjects.ValidateObjectsConsistency();
	return bResult;
}

