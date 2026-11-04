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
#include "WindowLib/JewelColours.h"
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
	mcAudio.Init();
	mcSDCard.Init();
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
bool CHCP65EmulatorApp::Setup(CNativeWindowFactory* pcNativeFactory, CNativeSoundFactory* pcSoundFactory, CNativeInput* pcNativeInput, const char* szInputDirectory)
{
	Ptr<CHCP65SoundVoice>	pSoundVoice;
	Ptr<CMenuContainer>		pFileMenu;
	Ptr<CCanvas>			pScreen;
	Ptr<CGridContainer>		pMain;
	Ptr<CWindowTheme>		pTheme;
	Ptr<CFont>				pWhite;
	Ptr<CFontColours>		pFontColours;

	mbValid = LoadKernal(&mcHCP65, HCP65_EMULATOR_KERNAL_ROM);
	if (!mcSDCard.InsertCard(HCP65_EMULATOR_SD_CARD_IMAGE, false))
	{
		gcLogger.Error2(__METHOD__, " Could not insert the SD card [", HCP65_EMULATOR_SD_CARD_IMAGE, "].", NULL);
		mbValid = false;
	}
	mcHCP65.SetDevice(HCP65_EMULATOR_SD_SOCKET, &mcSDCard);
	mcHCP65.SetDevice(HCP65_EMULATOR_AUDIO_SOCKET, &mcAudio);
	mcHCP65.Reset();

	if (mbValid)
	{
		pSoundVoice = OMalloc<CHCP65SoundVoice>(&mcAudio);
		mpWindow = OMalloc<CWindow>("HCP65 Emulator", pcNativeFactory, pcSoundFactory, OMalloc<CHCP65EmulatorTick>(&mcHCP65, &mcAudio, pSoundVoice), OMalloc<CClearCanvasDraw>(JEWEL_PANEL), CFT_RGBX, CCO_BGR, CRGB_24bit, ARGB_8bit);
		mpWindow->SetContinuous(true);  //The emulator runs and its screen changes every tick.
		mpWindow->GetSoundMixer()->AddVoice(pSoundVoice);

		//The font is white and coloured as it is drawn.
		pWhite = GenerateFont("Resources/InterVariable.ttf", "Inter 13px", 13.0f, mpWindow->GetColourOrder());
		mbValid = pWhite.IsNotNull();
	}

	if (mbValid)
	{
		pFontColours = OMalloc<CFontColours>(pWhite);
		pTheme = CJewelTheme::CreateDarkTheme(mpWindow, pFontColours->GetFont(JEWEL_TEXT));

		//The menu bar along the top over the HCP65's screen, which is only a blank canvas so far.
		mpMenuBar = OMalloc<CMenuBar>(mpWindow, pTheme);
		mpMenuBar->SetAlignment(CSH_Stretch, CSV_Stretch);
		pFileMenu = OMalloc<CMenuContainer>(mpWindow, pTheme);
		mpMenuBar->AddMenu("File", pFileMenu);
		pScreen = OMalloc<CCanvas>(mpWindow, OMalloc<CClearCanvasDraw>(JEWEL_EDITOR));
		pScreen->SetAlignment(CSH_Stretch, CSV_Stretch);
		pMain = OMalloc<CGridContainer>(mpWindow);
		pMain->AddComponent(mpMenuBar, 0, 0, 1, 1, 1, 0);
		pMain->AddComponent(pScreen, 0, 1, 1, 1, 1, 1);
		mpWindow->SetContainer(pMain);

		mcFileMenuListener.Init(mpWindow, pFileMenu);
		mpMenuBar->AddListener(&mcFileMenuListener);

		mpInput = OMalloc<CInput>((CNativeInput*)NULL);
		mbValid = mpInput->Read((char*)szInputDirectory);
		mpInput->AddNativeInput(pcNativeInput);
		mpInputSource = OMalloc<CWindowInputSource>(mpInput);
		mcComponentInput.Init(&mpInputSource, &mpWindow);
		mbInputs = true;
		mcComponentInput.AddKeyboards();
		mcComponentInput.AddMice();
		mpWindow->SetComponentInput(&mcComponentInput);
		mpWindow->SetWindowSize(1280, 800);
	}
	return mbValid;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65EmulatorApp::Class(void)
{
	U_Data(CHCP65, mcHCP65);
	U_Data(CHCP65Audio, mcAudio);
	U_Data(CHCP65SDCard, mcSDCard);
	M_Pointer(mpWindow);
	M_Pointer(mpMenuBar);
	U_Data(CFileMenuListener, mcFileMenuListener);
	M_Pointer(mpInput);
	M_Pointer(mpInputSource);
	U_Data(CComponentInput, mcComponentInput);
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
		mcFileMenuListener.RemoveFromListenerCalls();
		mcFileMenuListener.Kill();
		mcComponentInput.Kill();
	}
	mcHCP65.Kill();
	mcSDCard.Kill();
	mcAudio.Kill();
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
		mpWindow->Show();
	}

	if (mbInputs)
	{
		mpMenuBar->RemoveListener(&mcFileMenuListener);
		mpWindow->SetComponentInput(NULL);
	}
	return mbValid;
}


//////////////////////////////////////////////////////////////////////////
//
// The emulator, held by the root while it runs.
//
//////////////////////////////////////////////////////////////////////////
bool HCP65EmulatorRun(CNativeWindowFactory* pcNativeFactory, CNativeSoundFactory* pcSoundFactory, CNativeInput* pcNativeInput, const char* szInputDirectory)
{
	Ptr<CHCP65EmulatorApp>	pApp;
	bool					bResult;

	//Validating every object each time one changes is too slow for the text's glyphs.  They are validated once at the end.
	gcObjects.DisableValidation();

	pApp = OMalloc<CHCP65EmulatorApp>();
	ORoot()->Add(pApp);
	bResult = pApp->Setup(pcNativeFactory, pcSoundFactory, pcNativeInput, szInputDirectory) && pApp->Run();
	ORoot()->Remove(pApp);
	pApp = NULL;

	gcObjects.EnableValidation();
	gcObjects.ValidateObjectsConsistency();
	return bResult;
}

