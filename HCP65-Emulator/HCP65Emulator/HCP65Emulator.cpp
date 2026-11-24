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
#include "WindowLib/Label.h"
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
#include "HCP65Lib/HCP65VIA.h"
#include "HCP65Lib/HCP65Video.h"
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
// The boot loader's ROM image szFilename into the HCP65's ROM.
//
//////////////////////////////////////////////////////////////////////////
bool LoadBootROM(CHCP65* pcHCP65, const char* szFilename)
{
	CDiskFile	cFile;
	uint8*		puiROM;
	lsize		uiSize;
	bool		bResult;

	cFile.Init(szFilename);
	if (!cFile.Open(EFM_Read))
	{
		cFile.Kill();
		gcLogger.Error2(__METHOD__, " Could not open the boot loader's ROM [", szFilename, "].", NULL);
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
		gcLogger.Error2(__METHOD__, " Could not load the boot loader's ROM [", szFilename, "].", NULL);
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
	mcKeyboard.Init();
	mcSNESKeys.Init();
	mbLCDRowShown = false;
	mbFillingControllers = false;
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
	if (mbValid && !LoadBootROM(&mcHCP65, HCP65_EMULATOR_BOOT_ROM))
	{
		if (mcDebugger.IsWaiting())
		{
			gcLogger.Warning2(__METHOD__, " Starting without a boot loader ROM: the debugger is to load one.", NULL);
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
	else
	{
		PlugInDevices();
	}
	szError.Kill();
	mcSNESKeys.Load(HCP65_EMULATOR_SNES_MAPPINGS);
	mcSNESKeys.Attach((CHCP65LCDBoard*)mcMainboard.FindBoard(HCP65_LCD_BOARD_NAME));
	pcAudio = (CHCP65Audio*)mcMainboard.FindBoard(HCP65_AUDIO_NAME);
	for (i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], HCP65_EMULATOR_ARGUMENT_FILL) == 0)
		{
			mcHCP65.SetMemoryFill(true);
		}
	}
	mcHCP65.Reset();

	mcDebugTarget.Init(&mcHCP65);
	if (mbValid && mcDebugger.IsEnabled())
	{
		mbValid = mcDebugger.Start(&mcDebugTarget);
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
		if (mpWindow->GetSoundMixer().IsNotNull())
		{
			//Heard, unless the emulator has no sound (HCP65EmulatorRef).
			mpWindow->GetSoundMixer()->AddVoice(mpSoundVoice);
		}

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
		mpScreenDraw = OMalloc<CHCP65ScreenDraw>(mpTheme->GetColour(TC_Editor));
		mpTick->SetScreen(mpScreenDraw);
		mpScreenDraw->SetVideo((CHCP65VideoBoard*)mcMainboard.FindBoard(HCP65_VIDEO_BOARD_NAME));
		pScreen = OMalloc<CCanvas>(mpWindow, mpScreenDraw);
		pScreen->SetDrawnEveryPaint(true);  //The emulated screen changes every tick without telling the canvas.
		pScreen->SetAlignment(CSH_Stretch, CSV_Stretch);
		pScreen->SetCanGetFocus(true);
		pScreen->AddInputAction(IAF_AnyKeyDown, NULL, this, &CHCP65EmulatorApp::KeyDownAction);
		pScreen->AddInputAction(IAF_AnyKeyUp, NULL, this, &CHCP65EmulatorApp::KeyUpAction);
		pMain = OMalloc<CGridContainer>(mpWindow);
		pMain->AddComponent(mpMenuBar, 0, 0, 1, 1, 1, 0);
		pMain->AddComponent(pScreen, 0, 1, 1, 1, 1, 1);
		mpMain = pMain;
		mpScreen = pScreen;
		MakeLCDRow();
		ShowLCDRow();
		mpWindow->SetContainer(pMain);
		mpWindow->GetFocus()->SetFocus(pScreen);

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
		//Held keys repeat in its dialogs' fields, as the IDE's do.  The screen takes keys as they go down and up, for the
		//SNES pads and the PS/2 keyboard, so they still go down once.
		mcComponentInput.SetRepeat(COMPONENT_INPUT_REPEAT_DELAY, COMPONENT_INPUT_REPEAT_PERIOD);
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
	U_Unknown(CHCP65DebugTarget, mcDebugTarget);
	U_Unknown(CEmulatorDebugger, mcDebugger);
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
	U_Unknown(CHCP65PS2Keyboard, mcKeyboard);
	U_Unknown(CHCP65SNESKeys, mcSNESKeys);
	M_Pointer(mpMappingDialog);
	M_Pointer(mpScreenDraw);
	M_Pointer(mpLCDDraw);
	M_Pointer(mpLCDRow);
	M_Pointer(mapControllers[0]);
	M_Pointer(mapControllers[1]);
	M_Pointer(mpMain);
	M_Pointer(mpScreen);
	U_Bool(mbLCDRowShown);
	U_Bool(mbFillingControllers);
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
	mcDebugTarget.Kill();
	mcKeyboard.Kill();
	mcSNESKeys.Kill();
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
// waits for any board models still being read and takes the menu bar's
// listener and the component input out of the window.
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
	if (mpBoardScene.IsNotNull())
	{
		//Here, where no Free is running: the board scene says why.
		mpBoardScene->StopLoading();
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
	uint8		auiBreakouts[HCP65_NUM_DEVICE_SLOTS];
	int			i;

	//Only the Small Mainboard has a model.
	if (mpBoardScene.IsNull() && (mcMainboard.GetType() == HMB_Small) && CBoardSceneDraw::HasMainboardModel())
	{
		mpBoardScene = OMalloc<CBoardSceneDraw>(mpTheme);
	}
	mpBoardDialog = OMalloc<CBoardConfigurationDialog>(mpWindow, &msDialogLook, &mcMainboard, mpBoardScene);
	if (mpBoardDialog->Show())
	{
		mpBoardDialog->GetTypes(aszTypes);
		for (i = 0; i < HCP65_NUM_DEVICE_SLOTS; i++)
		{
			auiBreakouts[i] = mpBoardDialog->GetBreakouts(i);
		}
		ChangeBoards(aszTypes, auiBreakouts);
	}
	mpBoardDialog = NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The SNES Key Mappings dialog, at the first controller's mapping: on OK
// the mapping named is made or changed and they are all written to
// HCP65_EMULATOR_SNES_MAPPINGS.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65EmulatorApp::ShowSNESMappings(void)
{
	const char*	aszKeys[HCP65_SNES_KEYS_BUTTONS];
	int			i;

	mpMappingDialog = OMalloc<CSNESMappingDialog>(mpWindow, &msDialogLook, mpTheme, &mcSNESKeys, mcSNESKeys.GetController(0));
	if (mpMappingDialog->Show())
	{
		for (i = 0; i < HCP65_SNES_KEYS_BUTTONS; i++)
		{
			aszKeys[i] = mpMappingDialog->GetKey(i);
		}
		if (mcSNESKeys.SetMapping(mpMappingDialog->GetMappingName(), aszKeys) == HCP65_SNES_KEYS_NONE)
		{
			gcLogger.Error2(__METHOD__, " A mapping needs a name, not [", mpMappingDialog->GetMappingName(), "], and there can be only so many.", NULL);
		}
		else if (!mcSNESKeys.Save(HCP65_EMULATOR_SNES_MAPPINGS))
		{
			gcLogger.Error2(__METHOD__, " Could not write [" HCP65_EMULATOR_SNES_MAPPINGS "].", NULL);
		}
		FillControllers();
	}
	mpMappingDialog = NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The row under the screen: the LCD's display, then "Controller 1" and
// "Controller 2" each with its combo box, together in the middle of the
// row.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65EmulatorApp::MakeLCDRow(void)
{
	Ptr<CCanvas>		pLCD;
	Ptr<CCanvas>		pSpace;
	Ptr<CGridContainer>	pControllers;
	Ptr<CLabel>			pLabel;
	char				szLabel[32];
	int					i;

	mpLCDDraw = OMalloc<CHCP65LCDDraw>(mpTheme->GetColour(TC_Panel), &mcHCP65);
	mpLCDDraw->SetLCD((CHCP65LCDBoard*)mcMainboard.FindBoard(HCP65_LCD_BOARD_NAME));
	pLCD = OMalloc<CCanvas>(mpWindow, mpLCDDraw);
	pLCD->SetDrawnEveryPaint(true);  //The display changes, and its cursor blinks, without telling the canvas.
	pLCD->SetAlignment(CSH_Center, CSV_Center);

	pControllers = OMalloc<CGridContainer>(mpWindow);
	pControllers->SetAlignment(CSH_Left, CSV_Center);
	for (i = 0; i < HCP65_SNES_CONTROLLERS; i++)
	{
		snprintf(szLabel, sizeof(szLabel), "Controller %d", i + 1);
		pLabel = OMalloc<CLabel>(mpWindow, mpTheme->GetFont(), szLabel);
		pLabel->SetDesiredSize(pLabel->GetRequiredSize().x + mpTheme->GetMetric(TM_DialogLabelGap), -1);
		pLabel->SetAlignment(CSH_Left, CSV_Center);
		pControllers->AddComponent(pLabel, 0, i * 2);
		mapControllers[i] = OMalloc<CComboBox>(mpWindow, mpTheme);
		mapControllers[i]->SetDesiredSize(HCP65_EMULATOR_CONTROLLER_WIDTH, -1);
		mapControllers[i]->SetAlignment(CSH_Left, CSV_Center);
		mapControllers[i]->GetSelectionEvent()->Add(this, &CHCP65EmulatorApp::ControllerChosen);
		pControllers->AddComponent(mapControllers[i], 1, i * 2);
		if (i == 0)
		{
			pSpace = OMalloc<CCanvas>(mpWindow, OMalloc<CClearCanvasDraw>(mpTheme->GetColour(TC_Panel)));
			pSpace->SetDesiredSize(1, mpTheme->GetMetric(TM_DialogRowGap));
			pControllers->AddComponent(pSpace, 0, 1);
		}
	}
	FillControllers();

	//The display and the combo boxes in the middle: as much room either side, and a gap between them.
	mpLCDRow = OMalloc<CGridContainer>(mpWindow);
	mpLCDRow->SetAlignment(CSH_Stretch, CSV_Center);
	pSpace = OMalloc<CCanvas>(mpWindow, OMalloc<CClearCanvasDraw>(mpTheme->GetColour(TC_Panel)));
	pSpace->SetDesiredSize(1, 1);
	mpLCDRow->AddComponent(pSpace, 0, 0, 1, 1, 1, 0);
	mpLCDRow->AddComponent(pLCD, 1, 0);
	pSpace = OMalloc<CCanvas>(mpWindow, OMalloc<CClearCanvasDraw>(mpTheme->GetColour(TC_Panel)));
	pSpace->SetDesiredSize(HCP65_EMULATOR_CONTROLLER_GAP, 1);
	mpLCDRow->AddComponent(pSpace, 2, 0);
	mpLCDRow->AddComponent(pControllers, 3, 0);
	pSpace = OMalloc<CCanvas>(mpWindow, OMalloc<CClearCanvasDraw>(mpTheme->GetColour(TC_Panel)));
	pSpace->SetDesiredSize(1, 1);
	mpLCDRow->AddComponent(pSpace, 4, 0, 1, 1, 1, 0);
}


//////////////////////////////////////////////////////////////////////////
//
// The LCD's row in the window while there is an LCD and SNES Board, with
// its controllers' mappings in the combo boxes; not without one.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65EmulatorApp::ShowLCDRow(void)
{
	bool	bWanted;

	if (mpMain.IsNull() || mpLCDRow.IsNull())
	{
		return;
	}
	bWanted = mcSNESKeys.HasBoard();
	if (bWanted && !mbLCDRowShown)
	{
		mpMain->AddComponent(mpLCDRow, 0, 2, 1, 1, 1, 0);
	}
	else if (!bWanted && mbLCDRowShown)
	{
		mpMain->RemoveComponent(mpLCDRow);
		mpWindow->GetFocus()->SetFocus(mpScreen);
	}
	mbLCDRowShown = bWanted;
	FillControllers();
	mpWindow->InvalidateLayout();
}


//////////////////////////////////////////////////////////////////////////
//
// Each combo box's items the mappings (the first, not plugged in), at the
// one its controller has.  Not a choice: nothing is written.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65EmulatorApp::FillControllers(void)
{
	size	i;
	int		iController;

	mbFillingControllers = true;
	for (iController = 0; iController < HCP65_SNES_CONTROLLERS; iController++)
	{
		if (mapControllers[iController].IsNotNull())
		{
			mapControllers[iController]->RemoveAllItems();
			for (i = 0; i < mcSNESKeys.NumMappings(); i++)
			{
				mapControllers[iController]->AddItem(mcSNESKeys.GetName(i));
			}
			mapControllers[iController]->Select((int32)mcSNESKeys.GetController(iController));
		}
	}
	mbFillingControllers = false;
}


//////////////////////////////////////////////////////////////////////////
//
// A controller's combo box chosen: the controller has that mapping, the
// description is written with it (the HCP65 paused meanwhile, as writing
// it reads every board), and the screen has the focus again so that the
// keys go to the HCP65.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65EmulatorApp::ControllerChosen(CComboBox* pcCombo)
{
	CChars	szError;
	int32	iSelected;
	int		i;

	if (mbFillingControllers)
	{
		return;
	}
	for (i = 0; i < HCP65_SNES_CONTROLLERS; i++)
	{
		if (&mapControllers[i] == pcCombo)
		{
			iSelected = pcCombo->GetSelected();
			mcSNESKeys.SetController(i, (iSelected < 0) ? HCP65_SNES_KEYS_NONE : (size)iSelected);
		}
	}
	mcRunner.Pause();
	szError.Init();
	if (!mcMainboard.Write(mszMachine.Text(), &szError))
	{
		gcLogger.Error2(__METHOD__, " ", szError.Text(), ".", NULL);
	}
	szError.Kill();
	mcRunner.Resume();
	mpWindow->GetFocus()->SetFocus(mpScreen);
}


//////////////////////////////////////////////////////////////////////////
//
// The sockets hold aszTypes, each VIA Board with the breakouts in its
// socket's auiBreakouts (bit n for breakout n), the description says so,
// the first Sound
// Board is heard and the HCP65 is reset (through the debugger, if there
// is one, so it can tell its client).  The reset happens whatever failed.
// The HCP65 is paused while its boards change, and the sound mixer, whose
// thread plays the board, is locked while the board heard changes.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65EmulatorApp::ChangeBoards(const char** aszTypes, const uint8* auiBreakouts)
{
	CChars				szError;
	CHCP65VIABoard*		pcVIA;
	const char*			szType;
	int					iSocket;
	int					i;
	bool				bChanged;

	mcRunner.Pause();
	szError.Init();
	bChanged = mcMainboard.ChangeBoards(&mcHCP65, aszTypes, &szError);
	if (bChanged)
	{
		for (iSocket = 0; iSocket < HCP65_NUM_DEVICE_SLOTS; iSocket++)
		{
			szType = mcMainboard.GetBoardType(iSocket);
			if (szType && (strcmp(szType, HCP65_VIA_BOARD_NAME) == 0))
			{
				pcVIA = (CHCP65VIABoard*)mcMainboard.GetBoard(iSocket);
				for (i = 0; i < HCP65_VIA_NUM_BREAKOUTS; i++)
				{
					pcVIA->SetBreakout(i, (auiBreakouts[iSocket] & (1 << i)) != 0);
				}
			}
		}
	}
	PlugInDevices();
	if (!bChanged || !mcMainboard.Write(mszMachine.Text(), &szError))
	{
		gcLogger.Error2(__METHOD__, " ", szError.Text(), ".", NULL);
	}
	szError.Kill();

	if (mpWindow->GetSoundMixer().IsNotNull())
	{
		mpWindow->GetSoundMixer()->Lock();
	}
	mpTick->SetAudio((CHCP65Audio*)mcMainboard.FindBoard(HCP65_AUDIO_NAME));
	if (mpWindow->GetSoundMixer().IsNotNull())
	{
		mpWindow->GetSoundMixer()->Unlock();
	}
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


//////////////////////////////////////////////////////////////////////////
//
// The screen onto the first Video Board, the LCD's display onto the first
// LCD and SNES Board, the keyboard into port 1 of the
// first PS2 Board (unplugged if there is none) and nothing into its port
// 2.  Only while the HCP65 isn't running.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65EmulatorApp::PlugInDevices(void)
{
	CHCP65PS2Board*	pcPS2;

	if (mpScreenDraw.IsNotNull())
	{
		mpScreenDraw->SetVideo((CHCP65VideoBoard*)mcMainboard.FindBoard(HCP65_VIDEO_BOARD_NAME));
	}
	mcSNESKeys.Attach((CHCP65LCDBoard*)mcMainboard.FindBoard(HCP65_LCD_BOARD_NAME));
	if (mpLCDDraw.IsNotNull())
	{
		mpLCDDraw->SetLCD((CHCP65LCDBoard*)mcMainboard.FindBoard(HCP65_LCD_BOARD_NAME));
		ShowLCDRow();
	}

	pcPS2 = (CHCP65PS2Board*)mcMainboard.FindBoard(HCP65_PS2_BOARD_NAME);
	if (pcPS2 == NULL)
	{
		mcKeyboard.Detach();
		return;
	}
	if (mcKeyboard.GetPort() != pcPS2->GetPort(0))
	{
		mcKeyboard.Attach(pcPS2->GetPort(0));
	}
	pcPS2->GetPort(1)->SetPlugged(false);
}


//////////////////////////////////////////////////////////////////////////
//
// A host key going down or up, while the screen has the focus: a SNES
// controller whose keys have it presses or lets go of that button, and
// the keyboard sends it (from the HCP65's thread) too, so that the keys
// can still be typed.  Keys the keyboard hasn't are ignored.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65EmulatorApp::KeyDownAction(SComponentInputEvent* psEvent)
{
	if (psEvent->szName)
	{
		mcSNESKeys.Key(psEvent->szName, true);
		mcKeyboard.Key(psEvent->szName, true);
	}
}


void CHCP65EmulatorApp::KeyUpAction(SComponentInputEvent* psEvent)
{
	if (psEvent->szName)
	{
		mcSNESKeys.Key(psEvent->szName, false);
		mcKeyboard.Key(psEvent->szName, false);
	}
}
