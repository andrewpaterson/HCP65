#include <string.h>
#include <stdlib.h>
#include "HCP65User.h"
#include "HCP65SFT.h"
#include "HCP65LCDBoard.h"


//\SPRITES.EXE: Maki walking on a stage, drawn with the screen's calls (HCP65User.h), to show them and for
//TestHCP65KernalConsole to look at.  On black in 640x480, with each pixel the colour its bits are (SFTColours):
//  "HCP65 sprites" at (8, 12), in the console's font: each letter a transparent image from its SFT container.
//  The stage (\SPRITES\STAGE.SFT, an opaque 640 by 400) at (0, SPRITES_STAGE_Y).
//  Maki (\SPRITES\MAKI.SFT, a container of transparent 84 by 182 frames: standing then six walking, facing right, then
//  the same facing left) standing on it, in the middle to start with.
//  The flower at the stage's bottom right (\SPRITES\FLOWER.SFT, a transparent 80 by 94 cut from the stage by
//  Images/MakeFlower.py) is in front of her: it is drawn again over her whenever she is across it.
//  What to do under the stage.
//The first SNES controller (the LCD and SNES Board's, HCP65LCDBoard.h) walks her left and right, a frame of her walk
//every SPRITES_WALK_FRAMES of the screen's; she stands still facing the way she went when neither is pressed.  Each
//frame only what she covered in the frame being drawn is drawn again from the stage: of each of her rows, from her
//first pixel to her last (KERNAL_BLIT_SPANS, the spans worked out from her frames as they are loaded).  Then she is
//drawn, and the frames are flipped at the vertical blank.  Without the board she just stands.
//
//Escape ends it, and the kernal gives the screen back to the console; other keys don't (the emulator's keys for the
//controller are typed too: the arrows are escape sequences).  Exits 1 without the font, 2 without the screen and 3
//without the stage, Maki or the flower (or the memory for them).
#define SPRITES_FONT_FILE		"\\OS\\Fonts\\CONSOLE.SFT"
#define SPRITES_STAGE_FILE		"\\SPRITES\\STAGE.SFT"
#define SPRITES_MAKI_FILE		"\\SPRITES\\MAKI.SFT"
#define SPRITES_FLOWER_FILE		"\\SPRITES\\FLOWER.SFT"
#define SPRITES_FONT_MAX_SIZE	0x4000
#define SPRITES_WIDTH			640
#define SPRITES_HEIGHT			480
#define SPRITES_STAGE_Y			40
#define SPRITES_STAGE_HEIGHT	400
#define SPRITES_MAKI_WIDTH		84
#define SPRITES_MAKI_HEIGHT		182
#define SPRITES_MAKI_Y			246		//Her top, so that she stands on the stage's ground.
#define SPRITES_MAKI_START_X	((SPRITES_WIDTH - SPRITES_MAKI_WIDTH) / 2)
#define SPRITES_FLOWER_X		554		//Where the flower is on the stage (Fighting320.png's (277, 153), twice as big),
#define SPRITES_FLOWER_Y		(SPRITES_STAGE_Y + 306)
#define SPRITES_FLOWER_WIDTH	80		//and how big.
#define SPRITES_MAKI_STEP		3		//Pixels a frame as she walks.
#define SPRITES_MAKI_WALK		6		//Her walk's frames.
#define SPRITES_MAKI_FACING		(1 + SPRITES_MAKI_WALK)	//Frames facing each way: the left's follow the right's.
#define SPRITES_MAKI_FRAMES		(2 * SPRITES_MAKI_FACING)
#define SPRITES_WALK_FRAMES		5		//Screen frames each of her walk's is shown for.
#define SPRITES_FRAME_TICKS		(KERNAL_DEVICE_TICKS_PER_SECOND / 10)	//The longest a vertical blank is waited for.
#define SPRITES_NO_RECT			-1		//SSpritesRect's iX when a frame has nothing of Maki's drawn in it.
#define SPRITES_ESCAPE			0x1B
#define SPRITES_KEYS			16		//Typed bytes read at a time.


//Where Maki was drawn in one of the two frames, and which of hers.
struct SSpritesRect
{
	int16	iX;
	int16	iY;
	uint16	uiFrame;
};


uint8			gauiSpritesFont[SPRITES_FONT_MAX_SIZE];
uint32			guiSpritesFontSize;
uint8*			gpuiSpritesStage;
uint32			guiSpritesStageSize;
uint8*			gpuiSpritesMaki;
uint32			guiSpritesMakiSize;
uint8*			gpuiSpritesFlower;		//In one bank.
uint32			guiSpritesFlowerSize;
uint8			gauiSpritesColours[SFT_COLOURS_SIZE];
uint8			gauiSpritesRow[SPRITES_WIDTH];
SSpritesRect	gasSpritesDrawn[2];		//Maki in each frame: the one drawn into next is gasSpritesDrawn[guiSpritesBack].
uint16			guiSpritesBack;
uint16*			gpuiSpritesSpans;		//Each of her frames' rows' first pixel and the one after the last: frames, rows, two words.
const void*		gapvSpritesFrames[SPRITES_MAKI_FRAMES];	//Each of her frames, in one bank (SpritesInOneBank).
uint32			gauiSpritesFrameSizes[SPRITES_MAKI_FRAMES];
void*			gapvSpritesCopies[SPRITES_MAKI_FRAMES];	//The frames copied there, to free.

#define SPRITES_SPANS_SIZE		(SPRITES_MAKI_FRAMES * SPRITES_MAKI_HEIGHT * 2 * sizeof(uint16))
#define SPRITES_SPAN(f, r)		(&gpuiSpritesSpans[((uint32)(f) * SPRITES_MAKI_HEIGHT + (r)) * 2])


//////////////////////////////////////////////////////////////////////////
//
// The image of uiSize bytes at pvImage with its top left at (x, y); only
// what is in Maki's frame psClip if it isn't NULL, the rows' spans of it.
//
//////////////////////////////////////////////////////////////////////////
static void SpritesBlit(const void* pvImage, uint32 uiSize, int16 x, int16 y, const SSpritesRect* psClip)
{
	SKernalBlit	sBlit;

	memset(&sBlit, 0, sizeof(SKernalBlit));
	sBlit.uiImage = (uint32)pvImage;
	sBlit.uiSize = uiSize;
	sBlit.iX = x;
	sBlit.iY = y;
	if (psClip)
	{
		sBlit.uiFlags = KERNAL_BLIT_CLIP | KERNAL_BLIT_SPANS;
		sBlit.iClipLeft = psClip->iX;
		sBlit.iClipTop = psClip->iY;
		sBlit.iClipRight = (int16)(psClip->iX + SPRITES_MAKI_WIDTH);
		sBlit.iClipBottom = (int16)(psClip->iY + SPRITES_MAKI_HEIGHT);
		sBlit.uiSpans = (uint32)SPRITES_SPAN(psClip->uiFrame, 0);
	}
	KernalScreenBlit(&sBlit);
}


//////////////////////////////////////////////////////////////////////////
//
// uiSize bytes that don't cross a bank (a blit's quicker when an image,
// or spans, are all in one), NULL if there isn't the memory: a block
// that would is let go of for the next.
//
//////////////////////////////////////////////////////////////////////////
static void* SpritesInOneBank(uint32 uiSize)
{
	void*	apvTried[4];
	void*	pvMemory;
	int		iTried;
	int		i;

	pvMemory = NULL;
	for (iTried = 0; iTried < 4; iTried++)
	{
		pvMemory = malloc(uiSize);
		if (!pvMemory || ((((uint32)pvMemory) & 0xFFFF) + uiSize <= 0x10000))
		{
			break;
		}
		apvTried[iTried] = pvMemory;
		pvMemory = NULL;
	}
	for (i = 0; i < iTried; i++)
	{
		free(apvTried[i]);
	}
	return pvMemory;
}


//////////////////////////////////////////////////////////////////////////
//
// The file at szPath in memory of its own that doesn't cross a bank, in
// *puiSize bytes; NULL if it isn't there or there isn't the memory.
//
//////////////////////////////////////////////////////////////////////////
static uint8* SpritesLoadInOneBank(const char* szPath, uint32* puiSize)
{
	uint8*	puiFile;

	*puiSize = KernalFileSize(szPath);
	if (*puiSize == KERNAL_FILE_NOT_FOUND)
	{
		return NULL;
	}
	puiFile = (uint8*)SpritesInOneBank(*puiSize);
	if (puiFile && (KernalFileLoad(szPath, puiFile, *puiSize) != KERNAL_RESULT_SUCCESS))
	{
		free(puiFile);
		puiFile = NULL;
	}
	return puiFile;
}


//////////////////////////////////////////////////////////////////////////
//
// Each of Maki's frames where the blit wants it, all in one bank: where
// it is in MAKI.SFT, or a copy where it isn't.  False if there isn't the
// memory, or there aren't as many frames.
//
//////////////////////////////////////////////////////////////////////////
static bool SpritesPlaceFrames(void)
{
	const void*	pvFrame;
	uint32		uiSize;
	uint16		uiFrame;

	for (uiFrame = 0; uiFrame < SPRITES_MAKI_FRAMES; uiFrame++)
	{
		gapvSpritesCopies[uiFrame] = NULL;
		pvFrame = SFTContainedImage(gpuiSpritesMaki, guiSpritesMakiSize, uiFrame, &uiSize);
		if (!pvFrame)
		{
			return false;
		}
		if ((((uint32)pvFrame) & 0xFFFF) + uiSize > 0x10000)
		{
			gapvSpritesCopies[uiFrame] = SpritesInOneBank(uiSize);
			if (!gapvSpritesCopies[uiFrame])
			{
				return false;
			}
			memcpy(gapvSpritesCopies[uiFrame], pvFrame, uiSize);
			pvFrame = gapvSpritesCopies[uiFrame];
		}
		gapvSpritesFrames[uiFrame] = pvFrame;
		gauiSpritesFrameSizes[uiFrame] = uiSize;
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Each of Maki's frames' rows' span: from its first pixel to just after
// its last, 0 to 0 for a row without one.
//
//////////////////////////////////////////////////////////////////////////
static void SpritesFindSpans(void)
{
	const uint8*	puiCode;
	uint16*			puiSpan;
	uint32			uiSize;
	uint16			uiFrame;
	uint16			uiRow;
	uint16			uiX;
	uint16			uiCount;
	uint8			uiCode;

	memset(gpuiSpritesSpans, 0, SPRITES_SPANS_SIZE);
	for (uiFrame = 0; uiFrame < SPRITES_MAKI_FRAMES; uiFrame++)
	{
		puiCode = (const uint8*)gapvSpritesFrames[uiFrame];
		uiSize = gauiSpritesFrameSizes[uiFrame];
		if (uiSize <= sizeof(SSFTImage))
		{
			continue;
		}
		puiCode += sizeof(SSFTImage);
		uiRow = 0;
		uiX = 0;
		for (;;)
		{
			uiCode = *puiCode++;
			if (uiCode == SFT_RUN_END)
			{
				break;
			}
			if ((uiCode >= SFT_RUN_ROWS) && (uiCode < SFT_RUN_MIXED))
			{
				uiRow = (uint16)(uiRow + (uiCode & SFT_RUN_ROWS_MASK) + 1);
				uiX = 0;
				continue;
			}
			if ((uiCode >= SFT_RUN_SKIP) && (uiCode < SFT_RUN_ROWS))
			{
				uiX = (uint16)(uiX + (uiCode & SFT_RUN_COUNT_MASK) + 1);
				continue;
			}
			uiCount = (uiCode >= SFT_RUN_MIXED) ? (uint16)((uiCode & SFT_RUN_MIXED_MASK) + 1) : uiCode;
			if (uiRow < SPRITES_MAKI_HEIGHT)
			{
				puiSpan = SPRITES_SPAN(uiFrame, uiRow);
				if (puiSpan[1] == 0)
				{
					puiSpan[0] = uiX;
				}
				puiSpan[1] = (uint16)(uiX + uiCount);
			}
			puiCode += uiCount;
			uiX = (uint16)(uiX + uiCount);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// szText from (x, y), each character its image in the font.
//
//////////////////////////////////////////////////////////////////////////
static void SpritesText(const char* szText, int16 x, int16 y)
{
	const void*	pvGlyph;
	uint32		uiSize;

	for (; *szText != '\0'; szText++)
	{
		pvGlyph = SFTContainedImage(gauiSpritesFont, guiSpritesFontSize, (uint8)*szText, &uiSize);
		if (pvGlyph)
		{
			SpritesBlit(pvGlyph, uiSize, x, y, NULL);
		}
		x = (int16)(x + 8);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The file at szPath in memory of its own, in *puiSize bytes; NULL if it
// isn't there or there isn't the memory.
//
//////////////////////////////////////////////////////////////////////////
static uint8* SpritesLoad(const char* szPath, uint32* puiSize)
{
	uint8*	puiFile;

	*puiSize = KernalFileSize(szPath);
	if (*puiSize == KERNAL_FILE_NOT_FOUND)
	{
		return NULL;
	}
	puiFile = (uint8*)malloc(*puiSize);
	if (puiFile && (KernalFileLoad(szPath, puiFile, *puiSize) != KERNAL_RESULT_SUCCESS))
	{
		free(puiFile);
		puiFile = NULL;
	}
	return puiFile;
}


//////////////////////////////////////////////////////////////////////////
//
// The frame being drawn: black, the text, and the stage.
//
//////////////////////////////////////////////////////////////////////////
static void SpritesBackground(void)
{
	uint16	i;

	memset(gauiSpritesRow, 0, SPRITES_WIDTH);
	for (i = 0; i < SPRITES_HEIGHT; i++)
	{
		KernalScreenWrite((uint32)i * SPRITES_WIDTH, gauiSpritesRow, SPRITES_WIDTH);
	}
	SpritesText("HCP65 sprites", 8, 12);
	SpritesBlit(gpuiSpritesStage, guiSpritesStageSize, 0, SPRITES_STAGE_Y, NULL);
	SpritesText("The first SNES controller walks Maki.  Escape ends it.", 8, SPRITES_STAGE_Y + SPRITES_STAGE_HEIGHT + 12);
}


//////////////////////////////////////////////////////////////////////////
//
// Maki's frame uiFrame (of MAKI.SFT) at x, into the frame being drawn,
// over the stage where she was drawn in it last.  Then the frames are
// flipped and the one shown is waited for.
//
//////////////////////////////////////////////////////////////////////////
static void SpritesDrawMaki(uint16 uiFrame, int16 x)
{
	SSpritesRect*	psDrawn;
	const void*		pvImage;
	uint32			uiSize;

	psDrawn = &gasSpritesDrawn[guiSpritesBack];
	if (psDrawn->iX != SPRITES_NO_RECT)
	{
		SpritesBlit(gpuiSpritesStage, guiSpritesStageSize, 0, SPRITES_STAGE_Y, psDrawn);
	}
	pvImage = gapvSpritesFrames[uiFrame];
	uiSize = gauiSpritesFrameSizes[uiFrame];
	SpritesBlit(pvImage, uiSize, x, SPRITES_MAKI_Y, NULL);
	psDrawn->iX = x;
	psDrawn->iY = SPRITES_MAKI_Y;
	psDrawn->uiFrame = uiFrame;
	if ((x < SPRITES_FLOWER_X + SPRITES_FLOWER_WIDTH) && (x + SPRITES_MAKI_WIDTH > SPRITES_FLOWER_X))
	{
		SpritesBlit(gpuiSpritesFlower, guiSpritesFlowerSize, SPRITES_FLOWER_X, SPRITES_FLOWER_Y, NULL);
	}

	KernalScreenFlip();
	KernalScreenWaitBlank(SPRITES_FRAME_TICKS);
	guiSpritesBack ^= 1;
}


//////////////////////////////////////////////////////////////////////////
//
// The controller's buttons (LCD_BOARD_SNES_*), none if there's no
// controller.
//
//////////////////////////////////////////////////////////////////////////
static uint16 SpritesButtons(bool bController, uint16 uiHandle)
{
	uint8	auiButtons[LCD_BOARD_SNES_SIZE];
	uint32	uiDone;

	if (!bController)
	{
		return 0;
	}
	uiDone = 0;
	if ((KernalDeviceRead(uiHandle, 0, auiButtons, LCD_BOARD_SNES_SIZE, &uiDone) != KERNAL_DEVICE_SUCCESS) || (uiDone != LCD_BOARD_SNES_SIZE))
	{
		return 0;
	}
	return (uint16)(auiButtons[0] | ((uint16)auiButtons[1] << 8));
}


//////////////////////////////////////////////////////////////////////////
//
// Whether Escape has been pressed: an ESC that doesn't start an escape
// sequence (ESC [), which a key's comes all at once.  What else was typed
// is dropped.
//
//////////////////////////////////////////////////////////////////////////
static bool SpritesEscape(void)
{
	char	acKeys[SPRITES_KEYS];
	uint32	uiDone;
	uint32	i;

	uiDone = 0;
	KernalDeviceRead(KERNAL_STANDARD_INPUT, 0, acKeys, SPRITES_KEYS, &uiDone);
	for (i = 0; i < uiDone; i++)
	{
		if ((acKeys[i] == SPRITES_ESCAPE) && ((i + 1 == uiDone) || (acKeys[i + 1] != '[')))
		{
			return true;
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
int main(void)
{
	uint16	uiHandle;
	uint16	uiButtons;
	uint16	uiStep;
	uint16	uiFrame;
	bool	bController;
	bool	bLeft;
	bool	bWalking;
	int16	x;

	guiSpritesFontSize = KernalFileSize(SPRITES_FONT_FILE);
	if ((guiSpritesFontSize == KERNAL_FILE_NOT_FOUND) || (guiSpritesFontSize > SPRITES_FONT_MAX_SIZE) ||
		(KernalFileLoad(SPRITES_FONT_FILE, gauiSpritesFont, SPRITES_FONT_MAX_SIZE) != KERNAL_RESULT_SUCCESS))
	{
		return 1;
	}
	gpuiSpritesStage = SpritesLoad(SPRITES_STAGE_FILE, &guiSpritesStageSize);
	gpuiSpritesMaki = SpritesLoad(SPRITES_MAKI_FILE, &guiSpritesMakiSize);
	gpuiSpritesSpans = (uint16*)SpritesInOneBank(SPRITES_SPANS_SIZE);
	gpuiSpritesFlower = SpritesLoadInOneBank(SPRITES_FLOWER_FILE, &guiSpritesFlowerSize);
	if (!gpuiSpritesStage || !gpuiSpritesMaki || !gpuiSpritesSpans || !gpuiSpritesFlower || !SpritesPlaceFrames())
	{
		return 3;
	}
	SpritesFindSpans();
	if (KernalScreenSetMode(KERNAL_SCREEN_640_480) != KERNAL_DEVICE_SUCCESS)
	{
		return 2;
	}
	bController = KernalDeviceOpen(LCD_BOARD_MANUFACTURER, LCD_BOARD_NAME, 0, LCD_BOARD_SNES_1, &uiHandle) == KERNAL_DEVICE_SUCCESS;

	//Palette 0 the colours pixels' bits are.  Both frames have the background, and nothing of Maki's yet.
	SFTColours(gauiSpritesColours);
	KernalScreenSetColours(0, gauiSpritesColours);
	KernalScreenSetPalette(0);
	for (guiSpritesBack = 0; guiSpritesBack < 2; guiSpritesBack++)
	{
		SpritesBackground();
		gasSpritesDrawn[guiSpritesBack].iX = SPRITES_NO_RECT;
		KernalScreenFlip();
		KernalScreenWaitBlank(SPRITES_FRAME_TICKS);
	}
	guiSpritesBack = 0;

	//Until Escape.
	x = SPRITES_MAKI_START_X;
	bLeft = false;
	uiStep = 0;
	while (!SpritesEscape())
	{
		uiButtons = SpritesButtons(bController, uiHandle);
		bWalking = false;
		if ((uiButtons & LCD_BOARD_SNES_LEFT) && !(uiButtons & LCD_BOARD_SNES_RIGHT))
		{
			bLeft = true;
			bWalking = x > 0;
			if (bWalking)
			{
				x = (int16)((x > SPRITES_MAKI_STEP) ? x - SPRITES_MAKI_STEP : 0);
			}
		}
		else if ((uiButtons & LCD_BOARD_SNES_RIGHT) && !(uiButtons & LCD_BOARD_SNES_LEFT))
		{
			bLeft = false;
			bWalking = x < (SPRITES_WIDTH - SPRITES_MAKI_WIDTH);
			if (bWalking)
			{
				x = (int16)((x < SPRITES_WIDTH - SPRITES_MAKI_WIDTH - SPRITES_MAKI_STEP) ? x + SPRITES_MAKI_STEP : SPRITES_WIDTH - SPRITES_MAKI_WIDTH);
			}
		}

		if (bWalking)
		{
			uiStep = (uint16)((uiStep + 1) % (SPRITES_MAKI_WALK * SPRITES_WALK_FRAMES));
			uiFrame = (uint16)(1 + uiStep / SPRITES_WALK_FRAMES);
		}
		else
		{
			uiStep = 0;
			uiFrame = 0;
		}
		if (bLeft)
		{
			uiFrame = (uint16)(uiFrame + SPRITES_MAKI_FACING);
		}
		SpritesDrawMaki(uiFrame, x);
	}

	if (bController)
	{
		KernalDeviceClose(uiHandle);
	}
	for (uiFrame = 0; uiFrame < SPRITES_MAKI_FRAMES; uiFrame++)
	{
		free(gapvSpritesCopies[uiFrame]);
	}
	free(gpuiSpritesFlower);
	free(gpuiSpritesSpans);
	free(gpuiSpritesMaki);
	free(gpuiSpritesStage);
	return 0;
}
