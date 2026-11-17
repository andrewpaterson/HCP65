#include <string.h>
#include "HCP65User.h"
#include "HCP65SFT.h"


//\SPRITES.EXE: a picture drawn with the screen's calls (HCP65User.h), to show them and for TestHCP65KernalConsole to
//look at.  On black in 640x480, with each pixel the colour its bits are (SFTColours):
//  "HCP65 sprites" at (64, 32), in the console's font: each letter a transparent image from its SFT container.
//  Eight 32 by 32 tiles from (64, 64), 48 apart: red, green, blue, yellow, cyan, magenta, white and grey opaque images.
//  "Blits" at (72, 72), over the first tiles: only the letters' pixels are drawn on them.
//  A white tile at (-16, 128) and a grey one at (624, 464): the parts of them that are on the picture.
//A key ends it, and the kernal gives the screen back to the console.  Exits 1 without the font and 2 without the
//screen.
#define SPRITES_FONT_FILE		"\\OS\\Fonts\\CONSOLE.SFT"
#define SPRITES_FONT_MAX_SIZE	0x4000
#define SPRITES_WIDTH			640
#define SPRITES_HEIGHT			480
#define SPRITES_TILE			32
#define SPRITES_TILES			8
#define SPRITES_WAIT_TICKS		KERNAL_DEVICE_TICKS_PER_SECOND


uint8	gauiSpritesFont[SPRITES_FONT_MAX_SIZE];
uint32	guiSpritesFontSize;
uint8	gauiSpritesColours[KERNAL_SCREEN_COLOURS_SIZE];
uint8	gauiSpritesRow[SPRITES_WIDTH];
uint8	gauiSpritesTile[sizeof(SSFTImage) + SPRITES_TILE * SPRITES_TILE];

static const uint8	gauiSpritesTileColours[SPRITES_TILES] = { 0x07, 0x38, 0xC0, 0x3F, 0xF8, 0xC7, 0xFF, 0x49 };


//////////////////////////////////////////////////////////////////////////
//
// The image of uiSize bytes at pvImage with its top left at (x, y).
//
//////////////////////////////////////////////////////////////////////////
static void SpritesBlit(const void* pvImage, uint32 uiSize, int16 x, int16 y)
{
	SKernalBlit	sBlit;

	memset(&sBlit, 0, sizeof(SKernalBlit));
	sBlit.uiImage = (uint32)pvImage;
	sBlit.uiSize = uiSize;
	sBlit.iX = x;
	sBlit.iY = y;
	KernalScreenBlit(&sBlit);
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
			SpritesBlit(pvGlyph, uiSize, x, y);
		}
		x = (int16)(x + 8);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// An opaque tile, all uiColour, with its top left at (x, y).
//
//////////////////////////////////////////////////////////////////////////
static void SpritesTile(uint8 uiColour, int16 x, int16 y)
{
	SSFTImage*	psImage;

	psImage = (SSFTImage*)gauiSpritesTile;
	psImage->uiType = SFT_TYPE_OPAQUE;
	psImage->uiWidth = SPRITES_TILE;
	psImage->uiHeight = SPRITES_TILE;
	memset(&gauiSpritesTile[sizeof(SSFTImage)], uiColour, SPRITES_TILE * SPRITES_TILE);
	SpritesBlit(gauiSpritesTile, sizeof(gauiSpritesTile), x, y);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
int main(void)
{
	uint32	uiDone;
	uint16	i;
	char	c;

	guiSpritesFontSize = KernalFileSize(SPRITES_FONT_FILE);
	if ((guiSpritesFontSize == KERNAL_FILE_NOT_FOUND) || (guiSpritesFontSize > SPRITES_FONT_MAX_SIZE) ||
		(KernalFileLoad(SPRITES_FONT_FILE, gauiSpritesFont, SPRITES_FONT_MAX_SIZE) != KERNAL_RESULT_SUCCESS))
	{
		return 1;
	}
	if (KernalScreenSetMode(KERNAL_SCREEN_640_480) != KERNAL_DEVICE_SUCCESS)
	{
		return 2;
	}

	//Palette 0 the colours pixels' bits are, and every pixel black in it.
	SFTColours(&gauiSpritesColours[0], &gauiSpritesColours[256], &gauiSpritesColours[512]);
	KernalScreenSetColours(0, gauiSpritesColours);
	KernalScreenSetPalette(0);
	memset(gauiSpritesRow, 0, SPRITES_WIDTH);
	for (i = 0; i < SPRITES_HEIGHT; i++)
	{
		KernalScreenWrite((uint32)i * SPRITES_WIDTH, gauiSpritesRow, SPRITES_WIDTH);
	}

	SpritesText("HCP65 sprites", 64, 32);
	for (i = 0; i < SPRITES_TILES; i++)
	{
		SpritesTile(gauiSpritesTileColours[i], (int16)(64 + i * 48), 64);
	}
	SpritesText("Blits", 72, 72);
	SpritesTile(0xFF, -16, 128);
	SpritesTile(0x49, SPRITES_WIDTH - 16, SPRITES_HEIGHT - 16);

	KernalScreenFlip();
	KernalScreenWaitBlank(SPRITES_WAIT_TICKS);

	//Until a key.
	for (;;)
	{
		KernalDeviceWait(KERNAL_STANDARD_INPUT, KERNAL_CONSOLE_WAIT_INPUT, SPRITES_WAIT_TICKS);
		uiDone = 0;
		KernalDeviceRead(KERNAL_STANDARD_INPUT, 0, &c, 1, &uiDone);
		if (uiDone > 0)
		{
			break;
		}
	}
	return 0;
}

