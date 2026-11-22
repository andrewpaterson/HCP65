#include <string.h>
#include "HCP65User.h"
#include "HCP65SFT.h"
#include "HCP65VideoBoard.h"


//TestHCP65KernalVideo runs this (\vidtest.exe) as its start up program with a Video Board, and checks the picture it leaves: 640x480, the pixel
//at (x, y) palette 0's colour (x + y) & $FF, but row 0, whose palette number is 5 (all red), and row 1, which the row
//table makes row 0 again.  Palette 0's colour i is red i, green 255 - i and blue $40.  Before it opens the board for that it
//checks the screen's calls, reading back the pixels blits should and shouldn't have drawn.
#define VIDEO_TEST_MAX_BOARDS	20
#define VIDEO_TEST_WAIT_TICKS	KERNAL_DEVICE_TICKS_PER_SECOND
#define VIDEO_TEST_WIDTH		640
#define VIDEO_TEST_HEIGHT		480
#define VIDEO_TEST_RED			5


#define VIDEO_TEST_GROUND		0x77	//What is under the blits.
#define VIDEO_TEST_IMAGES_X		300		//Where the images whose palettes TestHCP65KernalVideo reads are drawn.
#define VIDEO_TEST_IMAGES_Y		150


uint8	gauiVideoTestRow[VIDEO_TEST_WIDTH];
uint8	gauiVideoTestColours[SFT_COLOURS_SIZE];

//A container of two images and a palette between them.  The first image is opaque, 4 by 3:
//  01 02 03 04
//  05 06 07 08
//  09 0A 0B 0C
//The palette is four colours from $A1: reds $10 to $40, greens $11 to $41 and blues $12 to $42.  The second image is
//transparent, 4 by 4, with rows without a pixel:
//  .. A1 A2 ..
//  .. .. .. ..
//  A3 .. .. A4
//  .. .. .. ..
#define VIDEO_TEST_OPAQUE			8		//Where each record is in it.
#define VIDEO_TEST_OPAQUE_SIZE		20
#define VIDEO_TEST_PALETTE			30
#define VIDEO_TEST_PALETTE_HASH		0x32DE3A94
#define VIDEO_TEST_TRANSPARENT		56
#define VIDEO_TEST_TRANSPARENT_SIZE	19

static const uint8	gauiVideoTestImages[] =
{
	0x53, 0x46, SFT_VERSION, SFT_TYPE_CONTAINER, 3, 0,
	22, 0,
	0x53, 0x46, SFT_VERSION, SFT_TYPE_OPAQUE, 4, 0, 3, 0,
	0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C,
	26, 0,
	0x53, 0x46, SFT_VERSION, SFT_TYPE_PALETTE, 0xA1, 0, 4, 0, 0x94, 0x3A, 0xDE, 0x32,
	0x10, 0x20, 0x30, 0x40, 0x11, 0x21, 0x31, 0x41, 0x12, 0x22, 0x32, 0x42,
	0, 0,
	0x53, 0x46, SFT_VERSION, SFT_TYPE_TRANSPARENT, 4, 0, 4, 0,
	0x80, 2, 0xA1, 0xA2,			//Skip 1, then two pixels.
	0xC1,						//Down two rows.
	1, 0xA3,
	0x81, 1, 0xA4,				//Skip 2, then a pixel.
	0,
};

static const uint8	gauiVideoTestWide[] =		//A transparent image whose run goes past its right.
{
	0x53, 0x46, SFT_VERSION, SFT_TYPE_TRANSPARENT, 2, 0, 1, 0,
	0x80, 2, 0xB1, 0xB2,
	0,
};

static const uint8	gauiVideoTestTall[] =		//A transparent image whose run is below its last row.
{
	0x53, 0x46, SFT_VERSION, SFT_TYPE_TRANSPARENT, 2, 0, 1, 0,
	0xC0, 1, 0xB1,
	0,
};

static const uint8	gauiVideoTestOldImage[] =	//The layout before SFT_VERSION 1: refused.
{
	0x57, 0xE4, 1, 0, 1, 0,
	0x01,
};

//A palette record with no colours, one past index 255, and one of another version.
static const uint8	gauiVideoTestNoColours[] = { 0x53, 0x46, SFT_VERSION, SFT_TYPE_PALETTE, 0, 0, 0, 0, 0, 0, 0, 0 };
static const uint8	gauiVideoTestPastEnd[] = { 0x53, 0x46, SFT_VERSION, SFT_TYPE_PALETTE, 0xFF, 0, 2, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6 };
static const uint8	gauiVideoTestVersion[] = { 0x53, 0x46, SFT_VERSION + 1, SFT_TYPE_PALETTE, 0, 0, 1, 0, 0, 0, 0, 0, 1, 2, 3 };


//////////////////////////////////////////////////////////////////////////
//
// The board is there with its six units; it opens and refuses what it
// doesn't take.
//
//////////////////////////////////////////////////////////////////////////
uint16 VideoTestOpen(uint16* puiPixels, uint16* puiPalettes, uint16* puiNumbers, uint16* puiRows)
{
	SKernalDeviceInfo	sInfo;
	uint16				uiIndex;
	uint16				uiHandle;
	uint32				uiResult;

	for (uiIndex = 0; uiIndex < VIDEO_TEST_MAX_BOARDS; uiIndex++)
	{
		if (KernalDeviceInfo(uiIndex, &sInfo) != KERNAL_DEVICE_SUCCESS)
		{
			return 1;
		}
		if ((strcmp(sInfo.szManufacturer, VIDEO_BOARD_MANUFACTURER) == 0) && (strcmp(sInfo.szName, VIDEO_BOARD_NAME) == 0))
		{
			break;
		}
	}
	if ((sInfo.uiNumUnits != 6) || sInfo.uiOpen)
	{
		return 2;
	}
	if (KernalDeviceOpen(VIDEO_BOARD_MANUFACTURER, VIDEO_BOARD_NAME, 0, 6, &uiHandle) != KERNAL_DEVICE_BAD_UNIT)
	{
		return 3;
	}
	if ((KernalDeviceOpen(VIDEO_BOARD_MANUFACTURER, VIDEO_BOARD_NAME, 0, VIDEO_BOARD_PIXELS, puiPixels) != KERNAL_DEVICE_SUCCESS) ||
		(KernalDeviceOpen(VIDEO_BOARD_MANUFACTURER, VIDEO_BOARD_NAME, 0, VIDEO_BOARD_PALETTES, puiPalettes) != KERNAL_DEVICE_SUCCESS) ||
		(KernalDeviceOpen(VIDEO_BOARD_MANUFACTURER, VIDEO_BOARD_NAME, 0, VIDEO_BOARD_PALETTE_NUMBERS, puiNumbers) != KERNAL_DEVICE_SUCCESS) ||
		(KernalDeviceOpen(VIDEO_BOARD_MANUFACTURER, VIDEO_BOARD_NAME, 0, VIDEO_BOARD_ROWS, puiRows) != KERNAL_DEVICE_SUCCESS))
	{
		return 4;
	}

	uiResult = 1;
	if ((KernalDeviceControl(*puiPixels, VIDEO_BOARD_GET_WIDTH, 0, &uiResult) != KERNAL_DEVICE_SUCCESS) || (uiResult != 0))
	{
		return 5;
	}
	if ((KernalDeviceControl(*puiPixels, 99, 0, &uiResult) != KERNAL_DEVICE_NOT_SUPPORTED) ||
		(KernalDeviceControl(*puiPixels, VIDEO_BOARD_SET_MODE, VIDEO_NUM_MODES, NULL) != KERNAL_DEVICE_BAD_ARGUMENT) ||
		(KernalDeviceControl(*puiPixels, VIDEO_BOARD_SET_PALETTE, VIDEO_BOARD_NUM_PALETTES, NULL) != KERNAL_DEVICE_BAD_ARGUMENT) ||
		(KernalDeviceWait(*puiPixels, 1, 1) != KERNAL_DEVICE_BAD_ARGUMENT) ||
		(KernalDeviceWrite(*puiPixels, VIDEO_BOARD_FRAME_SIZE - 1, gauiVideoTestRow, 2, NULL) != KERNAL_DEVICE_BAD_ARGUMENT) ||
		(KernalDeviceWrite(*puiRows, VIDEO_BOARD_ROWS_SIZE, gauiVideoTestRow, 1, NULL) != KERNAL_DEVICE_BAD_ARGUMENT))
	{
		return 6;
	}
	return 0;
}


//////////////////////////////////////////////////////////////////////////
//
// 640x480, and palettes 0 (red i, green 255 - i, blue $40) and 5 (red).
//
//////////////////////////////////////////////////////////////////////////
uint16 VideoTestMode(uint16 uiPixels, uint16 uiPalettes)
{
	uint32	uiResult;
	uint16	i;

	if (KernalDeviceControl(uiPixels, VIDEO_BOARD_SET_MODE, VIDEO_MODE_640_480, NULL) != KERNAL_DEVICE_SUCCESS)
	{
		return 1;
	}
	if ((KernalDeviceControl(uiPixels, VIDEO_BOARD_GET_WIDTH, 0, &uiResult) != KERNAL_DEVICE_SUCCESS) || (uiResult != VIDEO_TEST_WIDTH) ||
		(KernalDeviceControl(uiPixels, VIDEO_BOARD_GET_HEIGHT, 0, &uiResult) != KERNAL_DEVICE_SUCCESS) || (uiResult != VIDEO_TEST_HEIGHT))
	{
		return 2;
	}

	for (i = 0; i < 256; i++)
	{
		gauiVideoTestRow[i] = (uint8)i;
	}
	if (KernalDeviceWrite(uiPalettes, 0, gauiVideoTestRow, 256, NULL) != KERNAL_DEVICE_SUCCESS)
	{
		return 3;
	}
	for (i = 0; i < 256; i++)
	{
		gauiVideoTestRow[i] = (uint8)(255 - i);
	}
	if (KernalDeviceWrite(uiPalettes, VIDEO_BOARD_GREEN, gauiVideoTestRow, 256, NULL) != KERNAL_DEVICE_SUCCESS)
	{
		return 4;
	}
	memset(gauiVideoTestRow, 0x40, 256);
	if (KernalDeviceWrite(uiPalettes, VIDEO_BOARD_BLUE, gauiVideoTestRow, 256, NULL) != KERNAL_DEVICE_SUCCESS)
	{
		return 5;
	}

	memset(gauiVideoTestRow, 0xFF, 256);
	if (KernalDeviceWrite(uiPalettes, VIDEO_TEST_RED * 256, gauiVideoTestRow, 256, NULL) != KERNAL_DEVICE_SUCCESS)
	{
		return 6;
	}
	memset(gauiVideoTestRow, 0, 256);
	if ((KernalDeviceWrite(uiPalettes, VIDEO_BOARD_GREEN + VIDEO_TEST_RED * 256, gauiVideoTestRow, 256, NULL) != KERNAL_DEVICE_SUCCESS) ||
		(KernalDeviceWrite(uiPalettes, VIDEO_BOARD_BLUE + VIDEO_TEST_RED * 256, gauiVideoTestRow, 256, NULL) != KERNAL_DEVICE_SUCCESS))
	{
		return 7;
	}
	return 0;
}


//////////////////////////////////////////////////////////////////////////
//
// uiLength pixels from (x, y) of the frame being drawn made the ground.
//
//////////////////////////////////////////////////////////////////////////
bool VideoTestGround(uint16 x, uint16 y, uint16 uiLength)
{
	memset(gauiVideoTestRow, VIDEO_TEST_GROUND, uiLength);
	return KernalScreenWrite((uint32)y * VIDEO_TEST_WIDTH + x, gauiVideoTestRow, uiLength) == KERNAL_DEVICE_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////
//
// The frame being drawn's pixels from (x, y) are the uiLength at
// puiExpected.
//
//////////////////////////////////////////////////////////////////////////
bool VideoTestPixels(uint16 x, uint16 y, const uint8* puiExpected, uint16 uiLength)
{
	if (KernalScreenRead((uint32)y * VIDEO_TEST_WIDTH + x, gauiVideoTestRow, uiLength) != KERNAL_DEVICE_SUCCESS)
	{
		return false;
	}
	return memcmp(gauiVideoTestRow, puiExpected, uiLength) == 0;
}


//////////////////////////////////////////////////////////////////////////
//
// The image of uiSize bytes at pvImage blitted to (x, y).
//
//////////////////////////////////////////////////////////////////////////
uint16 VideoTestBlitAt(const void* pvImage, uint32 uiSize, int16 x, int16 y)
{
	SKernalBlit	sBlit;

	memset(&sBlit, 0, sizeof(SKernalBlit));
	sBlit.uiImage = (uint32)pvImage;
	sBlit.uiSize = uiSize;
	sBlit.iX = x;
	sBlit.iY = y;
	return KernalScreenBlit(&sBlit);
}


//////////////////////////////////////////////////////////////////////////
//
// The image whose handle is uiImage blitted to (x, y), clipped to the
// rectangle from (iClipLeft, iClipTop) if bClip.
//
//////////////////////////////////////////////////////////////////////////
uint16 VideoTestBlitImage(uint16 uiImage, int16 x, int16 y, bool bClip, int16 iClipLeft, int16 iClipTop, int16 iClipRight, int16 iClipBottom)
{
	SKernalBlit	sBlit;

	memset(&sBlit, 0, sizeof(SKernalBlit));
	sBlit.uiImage = uiImage;
	sBlit.iX = x;
	sBlit.iY = y;
	sBlit.uiFlags = (uint16)(KERNAL_BLIT_IMAGE | (bClip ? KERNAL_BLIT_CLIP : 0));
	sBlit.iClipLeft = iClipLeft;
	sBlit.iClipTop = iClipTop;
	sBlit.iClipRight = iClipRight;
	sBlit.iClipBottom = iClipBottom;
	return KernalScreenBlit(&sBlit);
}


//////////////////////////////////////////////////////////////////////////
//
// Images: the container's two made into images (SFTImageMake) and drawn
// as the SFTs are, whole and through a clip rectangle; one whose rows
// cross from one of the program's banks to another that isn't next to it
// in main memory; handles that have gone or were never given, a size of
// 0 and memory the program hasn't are refused, and there are no more than
// KERNAL_SCREEN_MAX_IMAGES.  With the palette set 4, the opaque image is
// drawn with its palette 3 at (VIDEO_TEST_IMAGES_X, VIDEO_TEST_IMAGES_Y),
// the transparent one with none at 10 to its right and the SFT opaque at
// 20 to its right after them (TestHCP65KernalVideo reads their palette
// numbers: 3, 4, 4).  Two images are left for the kernal to take back
// when the program ends.
//
//////////////////////////////////////////////////////////////////////////
uint16 VideoTestImages(const uint8* puiOpaque, uint32 uiOpaqueSize, const uint8* puiTransparent, uint32 uiTransparentSize)
{
	static const uint8	auiOpaque0[] = { 0x77, 0x01, 0x02, 0x03, 0x04, 0x77 };
	static const uint8	auiOpaque1[] = { 0x77, 0x05, 0x06, 0x07, 0x08, 0x77 };
	static const uint8	auiOpaque2[] = { 0x77, 0x09, 0x0A, 0x0B, 0x0C, 0x77 };
	static const uint8	auiGround[] = { 0x77, 0x77, 0x77, 0x77, 0x77, 0x77 };
	static const uint8	auiClear0[] = { 0x77, 0x77, 0xA1, 0xA2, 0x77, 0x77 };
	static const uint8	auiClear2[] = { 0x77, 0xA3, 0x77, 0x77, 0xA4, 0x77 };
	static const uint8	auiClip1[] = { 0x77, 0x06, 0x07, 0x77, 0x77 };
	static const uint8	auiCross0[] = { 0x77, 0xD1, 0xD2, 0xD3, 0xD4, 0x77 };
	static const uint8	auiCross1[] = { 0x77, 0xD5, 0xD6, 0xD7, 0xD8, 0x77 };
	static uint8		auiTiny[1] = { 0x55 };
	uint16				auiImages[KERNAL_SCREEN_MAX_IMAGES];
	uint16				uiOpaqueImage;
	uint16				uiTransparentImage;
	uint16				uiCrossImage;
	uint16				uiImage;
	uint16				uiMore;
	uint8*				puiCross;
	void*				pvPixels;
	uint16				i;
	uint16				y;

	//Made from the SFTs, then drawn whole with the ground around them.
	if ((SFTImageMake(puiOpaque, uiOpaqueSize, KERNAL_SCREEN_NO_PALETTE, &uiOpaqueImage) != KERNAL_DEVICE_SUCCESS) ||
		(SFTImageMake(puiTransparent, uiTransparentSize, KERNAL_SCREEN_NO_PALETTE, &uiTransparentImage) != KERNAL_DEVICE_SUCCESS) ||
		(SFTImageMake(puiOpaque, uiOpaqueSize - 1, KERNAL_SCREEN_NO_PALETTE, &uiImage) != KERNAL_DEVICE_BAD_ARGUMENT) ||
		(SFTImageMake(gauiVideoTestWide, sizeof(gauiVideoTestWide), KERNAL_SCREEN_NO_PALETTE, &uiImage) != KERNAL_DEVICE_BAD_ARGUMENT) ||
		(SFTImageMake(gauiVideoTestImages, sizeof(gauiVideoTestImages), KERNAL_SCREEN_NO_PALETTE, &uiImage) != KERNAL_DEVICE_BAD_ARGUMENT))
	{
		return 1;
	}
	for (y = 60; y < 66; y++)
	{
		if (!VideoTestGround(100, y, 6) || !VideoTestGround(110, y, 6))
		{
			return 2;
		}
	}
	if ((VideoTestBlitImage(uiOpaqueImage, 101, 61, false, 0, 0, 0, 0) != KERNAL_DEVICE_SUCCESS) ||
		!VideoTestPixels(100, 60, auiGround, 6) ||
		!VideoTestPixels(100, 61, auiOpaque0, 6) ||
		!VideoTestPixels(100, 62, auiOpaque1, 6) ||
		!VideoTestPixels(100, 63, auiOpaque2, 6) ||
		!VideoTestPixels(100, 64, auiGround, 6))
	{
		return 3;
	}
	if ((VideoTestBlitImage(uiTransparentImage, 111, 61, false, 0, 0, 0, 0) != KERNAL_DEVICE_SUCCESS) ||
		!VideoTestPixels(110, 60, auiGround, 6) ||
		!VideoTestPixels(110, 61, auiClear0, 6) ||
		!VideoTestPixels(110, 62, auiGround, 6) ||
		!VideoTestPixels(110, 63, auiClear2, 6) ||
		!VideoTestPixels(110, 64, auiGround, 6) ||
		!VideoTestPixels(110, 65, auiGround, 6))
	{
		return 4;
	}

	//Through a clip rectangle that has only the middle row's middle two pixels.
	for (y = 110; y < 113; y++)
	{
		if (!VideoTestGround(200, y, 5))
		{
			return 5;
		}
	}
	if ((VideoTestBlitImage(uiOpaqueImage, 200, 110, true, 201, 111, 203, 112) != KERNAL_DEVICE_SUCCESS) ||
		!VideoTestPixels(200, 110, auiGround, 5) ||
		!VideoTestPixels(200, 111, auiClip1, 5) ||
		!VideoTestPixels(200, 112, auiGround, 5))
	{
		return 6;
	}

	//Rows across banks: $C0 is mapped, then $C2, then $C1, so $C0 and $C1 aren't next to each other in main memory.  The
	//image's first row is the last two bytes of $C0 and the first two of $C1.
	if ((KernalMapBanks((void*)0xC00000, 1) != KERNAL_RESULT_SUCCESS) ||
		(KernalMapBanks((void*)0xC20000, 1) != KERNAL_RESULT_SUCCESS) ||
		(KernalMapBanks((void*)0xC10000, 1) != KERNAL_RESULT_SUCCESS))
	{
		return 7;
	}
	puiCross = (uint8*)0xC0FFFE;
	for (i = 0; i < 8; i++)
	{
		puiCross[i] = (uint8)(0xD1 + i);
	}
	for (y = 70; y < 74; y++)
	{
		if (!VideoTestGround(100, y, 6))
		{
			return 8;
		}
	}
	if ((KernalScreenAddImage(puiCross, 4, 2, 0, KERNAL_SCREEN_NO_PALETTE, &uiCrossImage) != KERNAL_DEVICE_SUCCESS) ||
		(VideoTestBlitImage(uiCrossImage, 101, 71, false, 0, 0, 0, 0) != KERNAL_DEVICE_SUCCESS) ||
		!VideoTestPixels(100, 70, auiGround, 6) ||
		!VideoTestPixels(100, 71, auiCross0, 6) ||
		!VideoTestPixels(100, 72, auiCross1, 6) ||
		!VideoTestPixels(100, 73, auiGround, 6) ||
		(KernalScreenRemoveImage(uiCrossImage, &pvPixels) != KERNAL_DEVICE_SUCCESS) || (pvPixels != puiCross))
	{
		return 9;
	}

	//What isn't refused: an image on memory the program no longer has is, when it is blitted.
	if ((KernalScreenAddImage(puiCross, 4, 2, 0, KERNAL_SCREEN_NO_PALETTE, &uiCrossImage) != KERNAL_DEVICE_SUCCESS) ||
		(KernalUnmapBanks((void*)0xC00000, 3) != KERNAL_RESULT_SUCCESS) ||
		(VideoTestBlitImage(uiCrossImage, 101, 71, false, 0, 0, 0, 0) != KERNAL_DEVICE_BAD_ARGUMENT) ||
		(KernalScreenRemoveImage(uiCrossImage, NULL) != KERNAL_DEVICE_SUCCESS))
	{
		return 10;
	}

	//Refused: a size of 0, memory the program hasn't, flags and palettes there aren't, and handles that have gone or were
	//never given.
	if ((KernalScreenAddImage(auiTiny, 0, 1, 0, KERNAL_SCREEN_NO_PALETTE, &uiImage) != KERNAL_DEVICE_BAD_ARGUMENT) ||
		(KernalScreenAddImage(auiTiny, 1, 0, 0, KERNAL_SCREEN_NO_PALETTE, &uiImage) != KERNAL_DEVICE_BAD_ARGUMENT) ||
		(KernalScreenAddImage((void*)0xC00000, 1, 1, 0, KERNAL_SCREEN_NO_PALETTE, &uiImage) != KERNAL_DEVICE_BAD_ARGUMENT) ||
		(KernalScreenAddImage(auiTiny, 1, 1, 0x0002, KERNAL_SCREEN_NO_PALETTE, &uiImage) != KERNAL_DEVICE_BAD_ARGUMENT) ||
		(KernalScreenAddImage(auiTiny, 1, 1, 0, KERNAL_SCREEN_NUM_PALETTES, &uiImage) != KERNAL_DEVICE_BAD_ARGUMENT) ||
		(VideoTestBlitImage(0, 300, 300, false, 0, 0, 0, 0) != KERNAL_DEVICE_BAD_HANDLE) ||
		(VideoTestBlitImage(uiCrossImage, 300, 300, false, 0, 0, 0, 0) != KERNAL_DEVICE_BAD_HANDLE) ||
		(KernalScreenRemoveImage(uiCrossImage, NULL) != KERNAL_DEVICE_BAD_HANDLE) ||
		(KernalScreenRemoveImage(0, NULL) != KERNAL_DEVICE_BAD_HANDLE))
	{
		return 11;
	}

	//No more than KERNAL_SCREEN_MAX_IMAGES (the two above among them), and an entry used again has a new handle.
	uiMore = KERNAL_SCREEN_MAX_IMAGES - 2;
	for (i = 0; i < uiMore; i++)
	{
		if (KernalScreenAddImage(auiTiny, 1, 1, 0, KERNAL_SCREEN_NO_PALETTE, &auiImages[i]) != KERNAL_DEVICE_SUCCESS)
		{
			return 12;
		}
	}
	if (KernalScreenAddImage(auiTiny, 1, 1, 0, KERNAL_SCREEN_NO_PALETTE, &uiImage) != KERNAL_DEVICE_NO_HANDLES)
	{
		return 13;
	}
	if ((KernalScreenRemoveImage(auiImages[0], NULL) != KERNAL_DEVICE_SUCCESS) ||
		(KernalScreenAddImage(auiTiny, 1, 1, 0, KERNAL_SCREEN_NO_PALETTE, &uiImage) != KERNAL_DEVICE_SUCCESS) ||
		(uiImage == auiImages[0]) ||
		(KernalScreenRemoveImage(auiImages[0], NULL) != KERNAL_DEVICE_BAD_HANDLE) ||
		(KernalScreenRemoveImage(uiImage, NULL) != KERNAL_DEVICE_SUCCESS))
	{
		return 14;
	}
	for (i = 1; i < uiMore; i++)
	{
		if (KernalScreenRemoveImage(auiImages[i], NULL) != KERNAL_DEVICE_SUCCESS)
		{
			return 15;
		}
	}

	//The palettes they are drawn with (TestHCP65KernalVideo looks): the opaque image's own 3, the set palette 4 for the
	//transparent one, which has none, and 4 again for the SFT after them.
	if ((SFTImageFree(uiOpaqueImage) != KERNAL_DEVICE_SUCCESS) ||
		(SFTImageFree(uiOpaqueImage) != KERNAL_DEVICE_BAD_HANDLE) ||
		(SFTImageMake(puiOpaque, uiOpaqueSize, 3, &uiOpaqueImage) != KERNAL_DEVICE_SUCCESS) ||
		(KernalScreenSetPalette(4) != KERNAL_DEVICE_SUCCESS) ||
		(VideoTestBlitImage(uiOpaqueImage, VIDEO_TEST_IMAGES_X, VIDEO_TEST_IMAGES_Y, false, 0, 0, 0, 0) != KERNAL_DEVICE_SUCCESS) ||
		(VideoTestBlitImage(uiTransparentImage, VIDEO_TEST_IMAGES_X + 10, VIDEO_TEST_IMAGES_Y, false, 0, 0, 0, 0) != KERNAL_DEVICE_SUCCESS) ||
		(VideoTestBlitAt(puiOpaque, uiOpaqueSize, VIDEO_TEST_IMAGES_X + 20, VIDEO_TEST_IMAGES_Y) != KERNAL_DEVICE_SUCCESS) ||
		(KernalScreenSetPalette(0) != KERNAL_DEVICE_SUCCESS))
	{
		return 16;
	}
	return 0;
}


//////////////////////////////////////////////////////////////////////////
//
// The screen's calls.  The picture is this program's from when it sets a
// mode until it sets text: the kernal has the Video Board meanwhile, and
// lets it go after (the console isn't on it).  Blits: an opaque and a
// transparent image, whole, off each edge of the picture and through a
// clip rectangle; and what isn't an image.
//
//////////////////////////////////////////////////////////////////////////
uint16 VideoTestScreen(void)
{
	static const uint8	auiOpaque0[] = { 0x77, 0x01, 0x02, 0x03, 0x04, 0x77 };
	static const uint8	auiOpaque1[] = { 0x77, 0x05, 0x06, 0x07, 0x08, 0x77 };
	static const uint8	auiOpaque2[] = { 0x77, 0x09, 0x0A, 0x0B, 0x0C, 0x77 };
	static const uint8	auiGround[] = { 0x77, 0x77, 0x77, 0x77, 0x77, 0x77 };
	static const uint8	auiClear0[] = { 0x77, 0x77, 0xA1, 0xA2, 0x77, 0x77 };
	static const uint8	auiClear2[] = { 0x77, 0xA3, 0x77, 0x77, 0xA4, 0x77 };
	static const uint8	auiCorner0[] = { 0x07, 0x08, 0x77 };
	static const uint8	auiCorner1[] = { 0x0B, 0x0C, 0x77 };
	static const uint8	auiLast[] = { 0x77, 0x01, 0x02, 0x77 };
	static const uint8	auiClip1[] = { 0x77, 0x06, 0x07, 0x77, 0x77 };
	static const uint8	auiLeft0[] = { 0xA2, 0x77, 0x77 };
	static const uint8	auiLeft2[] = { 0x77, 0xA4, 0x77 };
	const uint8*	puiOpaque;
	const uint8*	puiTransparent;
	uint32			uiOpaqueSize;
	uint32			uiTransparentSize;
	uint32			uiSize;
	const SSFTPalette*	psPalette;
	const uint8*	puiColours;
	SKernalBlit		sBlit;
	uint16			uiResult;
	uint16			uiHandle;
	uint16			uiWidth;
	uint16			uiHeight;
	uint16			y;

	//Nothing but a mode before there is one; then the picture's size, and the board is the kernal's.
	if ((KernalScreenFlip() != KERNAL_DEVICE_NOT_OPEN) ||
		(KernalScreenSetText() != KERNAL_DEVICE_NOT_OPEN) ||
		(KernalScreenSetMode(VIDEO_NUM_MODES) != KERNAL_DEVICE_BAD_ARGUMENT) ||
		(KernalScreenSetMode(KERNAL_SCREEN_640_480) != KERNAL_DEVICE_SUCCESS) ||
		(KernalScreenGetSize(&uiWidth, &uiHeight) != KERNAL_DEVICE_SUCCESS) || (uiWidth != VIDEO_TEST_WIDTH) || (uiHeight != VIDEO_TEST_HEIGHT) ||
		(KernalDeviceOpen(VIDEO_BOARD_MANUFACTURER, VIDEO_BOARD_NAME, 0, VIDEO_BOARD_PIXELS, &uiHandle) != KERNAL_DEVICE_IN_USE))
	{
		return 15;
	}

	//The images are where they should be in their container.
	puiOpaque = (const uint8*)SFTContainedImage(gauiVideoTestImages, sizeof(gauiVideoTestImages), 0, &uiOpaqueSize);
	puiTransparent = (const uint8*)SFTContainedImage(gauiVideoTestImages, sizeof(gauiVideoTestImages), 1, &uiTransparentSize);
	psPalette = SFTContainedPalette(gauiVideoTestImages, sizeof(gauiVideoTestImages), 1);
	if ((puiOpaque != &gauiVideoTestImages[VIDEO_TEST_OPAQUE]) || (uiOpaqueSize != VIDEO_TEST_OPAQUE_SIZE) ||
		(puiTransparent != &gauiVideoTestImages[VIDEO_TEST_TRANSPARENT]) || (uiTransparentSize != VIDEO_TEST_TRANSPARENT_SIZE) ||
		(SFTContainedImage(gauiVideoTestImages, sizeof(gauiVideoTestImages), 2, &uiSize) != NULL) ||
		(SFTContainedImage(gauiVideoTestWide, sizeof(gauiVideoTestWide), 0, &uiSize) != NULL) ||
		(SFTContainedPalette(gauiVideoTestImages, sizeof(gauiVideoTestImages), 0) != NULL) ||
		((const uint8*)psPalette != &gauiVideoTestImages[VIDEO_TEST_PALETTE]) ||
		(psPalette->uiHash != VIDEO_TEST_PALETTE_HASH) || (SFTPaletteHash(psPalette) != VIDEO_TEST_PALETTE_HASH) ||
		!SFTIsRecord(puiOpaque, uiOpaqueSize, SFT_TYPE_OPAQUE) || SFTIsRecord(puiOpaque, uiOpaqueSize, SFT_TYPE_TRANSPARENT) ||
		SFTIsRecord(gauiVideoTestOldImage, sizeof(gauiVideoTestOldImage), SFT_TYPE_OPAQUE))
	{
		return 1;
	}

	//The colours a pixel's bits are: $FF white, $07 red, $38 green, $C0 blue, $49 a dark grey.  The palette's hash is
	//SFT_COLOURS_HASH.
	SFTColours(gauiVideoTestColours);
	puiColours = &gauiVideoTestColours[sizeof(SSFTPalette)];
	if ((puiColours[0xFF] != 0xFF) || (puiColours[0x07] != 0xFF) || (puiColours[0x38] != 0) || (puiColours[0x49] != 0x24) ||
		(puiColours[256 + 0x38] != 0xFF) || (puiColours[256 + 0x07] != 0) || (puiColours[256 + 0x49] != 0x24) ||
		(puiColours[512 + 0xC0] != 0xFF) || (puiColours[512 + 0x49] != 0x55) || (puiColours[512 + 0x3F] != 0) ||
		(SFTPaletteHash((const SSFTPalette*)gauiVideoTestColours) != SFT_COLOURS_HASH))
	{
		return 14;
	}

	//Whole, with the ground around them.
	for (y = 50; y < 56; y++)
	{
		if (!VideoTestGround(100, y, 6) || !VideoTestGround(110, y, 6))
		{
			return 2;
		}
	}
	if ((VideoTestBlitAt(puiOpaque, uiOpaqueSize, 101, 51) != KERNAL_DEVICE_SUCCESS) ||
		!VideoTestPixels(100, 50, auiGround, 6) ||
		!VideoTestPixels(100, 51, auiOpaque0, 6) ||
		!VideoTestPixels(100, 52, auiOpaque1, 6) ||
		!VideoTestPixels(100, 53, auiOpaque2, 6) ||
		!VideoTestPixels(100, 54, auiGround, 6))
	{
		return 3;
	}
	if ((VideoTestBlitAt(puiTransparent, uiTransparentSize, 111, 51) != KERNAL_DEVICE_SUCCESS) ||
		!VideoTestPixels(110, 50, auiGround, 6) ||
		!VideoTestPixels(110, 51, auiClear0, 6) ||
		!VideoTestPixels(110, 52, auiGround, 6) ||
		!VideoTestPixels(110, 53, auiClear2, 6) ||
		!VideoTestPixels(110, 54, auiGround, 6) ||
		!VideoTestPixels(110, 55, auiGround, 6))
	{
		return 4;
	}

	//Off the top left, and off the bottom right (the row after the picture's last isn't drawn on).
	if (!VideoTestGround(0, 0, 6) || !VideoTestGround(0, 1, 6) || !VideoTestGround(0, 2, 6) ||
		!VideoTestGround(VIDEO_TEST_WIDTH - 3, VIDEO_TEST_HEIGHT - 1, 3 + 6))
	{
		return 5;
	}
	if ((VideoTestBlitAt(puiOpaque, uiOpaqueSize, -2, -1) != KERNAL_DEVICE_SUCCESS) ||
		!VideoTestPixels(0, 0, auiCorner0, 3) ||
		!VideoTestPixels(0, 1, auiCorner1, 3) ||
		!VideoTestPixels(0, 2, auiGround, 3))
	{
		return 6;
	}
	if ((VideoTestBlitAt(puiOpaque, uiOpaqueSize, VIDEO_TEST_WIDTH - 2, VIDEO_TEST_HEIGHT - 1) != KERNAL_DEVICE_SUCCESS) ||
		!VideoTestPixels(VIDEO_TEST_WIDTH - 3, VIDEO_TEST_HEIGHT - 1, auiLast, 3) ||
		!VideoTestPixels(0, VIDEO_TEST_HEIGHT, auiGround, 6))
	{
		return 7;
	}
	if ((VideoTestBlitAt(puiOpaque, uiOpaqueSize, VIDEO_TEST_WIDTH, 0) != KERNAL_DEVICE_SUCCESS) ||
		(VideoTestBlitAt(puiOpaque, uiOpaqueSize, 0, VIDEO_TEST_HEIGHT) != KERNAL_DEVICE_SUCCESS) ||
		(VideoTestBlitAt(puiTransparent, uiTransparentSize, -4, -4) != KERNAL_DEVICE_SUCCESS) ||
		!VideoTestPixels(0, VIDEO_TEST_HEIGHT, auiGround, 6) ||
		!VideoTestPixels(0, 2, auiGround, 3))
	{
		return 8;
	}

	//A transparent image off the left: its first row's second pixel and its third row's last are left.
	for (y = 20; y < 24; y++)
	{
		if (!VideoTestGround(0, y, 3))
		{
			return 9;
		}
	}
	if ((VideoTestBlitAt(puiTransparent, uiTransparentSize, -2, 20) != KERNAL_DEVICE_SUCCESS) ||
		!VideoTestPixels(0, 20, auiLeft0, 3) ||
		!VideoTestPixels(0, 21, auiGround, 3) ||
		!VideoTestPixels(0, 22, auiLeft2, 3) ||
		!VideoTestPixels(0, 23, auiGround, 3))
	{
		return 10;
	}

	//Through a clip rectangle that has only the middle row's middle two pixels.
	for (y = 100; y < 103; y++)
	{
		if (!VideoTestGround(200, y, 5))
		{
			return 11;
		}
	}
	memset(&sBlit, 0, sizeof(SKernalBlit));
	sBlit.uiImage = (uint32)puiOpaque;
	sBlit.uiSize = uiOpaqueSize;
	sBlit.iX = 200;
	sBlit.iY = 100;
	sBlit.uiFlags = KERNAL_BLIT_CLIP;
	sBlit.iClipLeft = 201;
	sBlit.iClipTop = 101;
	sBlit.iClipRight = 203;
	sBlit.iClipBottom = 102;
	if ((KernalScreenBlit(&sBlit) != KERNAL_DEVICE_SUCCESS) ||
		!VideoTestPixels(200, 100, auiGround, 5) ||
		!VideoTestPixels(200, 101, auiClip1, 5) ||
		!VideoTestPixels(200, 102, auiGround, 5))
	{
		return 12;
	}

	//What isn't an image, is cut short or has a run outside it; a unit that isn't a picture; no blit at all.
	if ((VideoTestBlitAt(gauiVideoTestImages, sizeof(gauiVideoTestImages), 300, 300) != KERNAL_DEVICE_BAD_ARGUMENT) ||
		(VideoTestBlitAt(puiOpaque, 5, 300, 300) != KERNAL_DEVICE_BAD_ARGUMENT) ||
		(VideoTestBlitAt(puiOpaque, uiOpaqueSize - 1, 300, 300) != KERNAL_DEVICE_BAD_ARGUMENT) ||
		(VideoTestBlitAt(puiTransparent, uiTransparentSize - 1, 300, 300) != KERNAL_DEVICE_BAD_ARGUMENT) ||
		(VideoTestBlitAt(puiTransparent, 6 + 2 + 1, 300, 300) != KERNAL_DEVICE_BAD_ARGUMENT) ||
		(VideoTestBlitAt(gauiVideoTestTall, sizeof(gauiVideoTestTall), 300, 300) != KERNAL_DEVICE_BAD_ARGUMENT) ||
		(VideoTestBlitAt(gauiVideoTestWide, sizeof(gauiVideoTestWide), 300, 300) != KERNAL_DEVICE_BAD_ARGUMENT) ||
		(VideoTestBlitAt(gauiVideoTestOldImage, sizeof(gauiVideoTestOldImage), 300, 300) != KERNAL_DEVICE_BAD_ARGUMENT) ||
		(VideoTestBlitAt((const void*)0xF00000, 100, 300, 300) != KERNAL_DEVICE_BAD_ARGUMENT) ||
		(KernalScreenBlit((const SKernalBlit*)0xF00000) != KERNAL_DEVICE_BAD_ARGUMENT))
	{
		return 13;
	}

	//Images.
	uiResult = VideoTestImages(puiOpaque, uiOpaqueSize, puiTransparent, uiTransparentSize);
	if (uiResult != 0)
	{
		return 20 + uiResult;
	}

	//A palette's colours, the palette drawn with, and the frames flipped; then the picture is given back.
	//Palette 2 all SFTColours' but the container's palette's four colours at $A1 (TestHCP65KernalVideo looks).
	if ((KernalScreenSetColours(1, gauiVideoTestColours) != KERNAL_DEVICE_SUCCESS) ||
		(KernalScreenSetColours(KERNAL_SCREEN_NUM_PALETTES, gauiVideoTestColours) != KERNAL_DEVICE_BAD_ARGUMENT) ||
		(KernalScreenSetColours(2, gauiVideoTestColours) != KERNAL_DEVICE_SUCCESS) ||
		(KernalScreenSetColours(2, psPalette) != KERNAL_DEVICE_SUCCESS) ||
		(KernalScreenSetColours(2, gauiVideoTestNoColours) != KERNAL_DEVICE_BAD_ARGUMENT) ||
		(KernalScreenSetColours(2, gauiVideoTestPastEnd) != KERNAL_DEVICE_BAD_ARGUMENT) ||
		(KernalScreenSetColours(2, gauiVideoTestVersion) != KERNAL_DEVICE_BAD_ARGUMENT) ||
		(KernalScreenSetPalette(1) != KERNAL_DEVICE_SUCCESS) ||
		(KernalScreenSetPalette(KERNAL_SCREEN_NUM_PALETTES) != KERNAL_DEVICE_BAD_ARGUMENT) ||
		(KernalScreenSetPalette(KERNAL_SCREEN_NO_PALETTE) != KERNAL_DEVICE_SUCCESS) ||
		(KernalScreenFlip() != KERNAL_DEVICE_SUCCESS) ||
		(KernalScreenWaitBlank(VIDEO_TEST_WAIT_TICKS) != KERNAL_DEVICE_SUCCESS))
	{
		return 16;
	}
	if ((KernalScreenSetText() != KERNAL_DEVICE_SUCCESS) ||
		(KernalScreenFlip() != KERNAL_DEVICE_NOT_OPEN))
	{
		return 17;
	}
	return 0;
}


//////////////////////////////////////////////////////////////////////////
//
// The frame being drawn: (x + y) & $FF with palette 0 (the pixel writes
// set it), then row 0's palette numbers 5.  It is shown at the next
// vertical blank.
//
//////////////////////////////////////////////////////////////////////////
uint16 VideoTestDraw(uint16 uiPixels, uint16 uiNumbers)
{
	uint32	uiShown;
	uint32	uiNowShown;
	uint32	uiDone;
	uint16	x;
	uint16	y;

	if (KernalDeviceControl(uiPixels, VIDEO_BOARD_SET_PALETTE, 0, NULL) != KERNAL_DEVICE_SUCCESS)
	{
		return 1;
	}
	for (y = 0; y < VIDEO_TEST_HEIGHT; y++)
	{
		for (x = 0; x < VIDEO_TEST_WIDTH; x++)
		{
			gauiVideoTestRow[x] = (uint8)(x + y);
		}
		uiDone = 0;
		if ((KernalDeviceWrite(uiPixels, (uint32)y * VIDEO_TEST_WIDTH, gauiVideoTestRow, VIDEO_TEST_WIDTH, &uiDone) != KERNAL_DEVICE_SUCCESS) || (uiDone != VIDEO_TEST_WIDTH))
		{
			return 2;
		}
	}
	memset(gauiVideoTestRow, VIDEO_TEST_RED, VIDEO_TEST_WIDTH);
	if (KernalDeviceWrite(uiNumbers, 0, gauiVideoTestRow, VIDEO_TEST_WIDTH, NULL) != KERNAL_DEVICE_SUCCESS)
	{
		return 3;
	}

	//Read back what was written.
	if ((KernalDeviceRead(uiPixels, VIDEO_TEST_WIDTH + 10, gauiVideoTestRow, 1, NULL) != KERNAL_DEVICE_SUCCESS) || (gauiVideoTestRow[0] != 11))
	{
		return 4;
	}

	if (KernalDeviceControl(uiPixels, VIDEO_BOARD_GET_SHOWN, 0, &uiShown) != KERNAL_DEVICE_SUCCESS)
	{
		return 5;
	}
	if (KernalDeviceControl(uiPixels, VIDEO_BOARD_FLIP, 0, NULL) != KERNAL_DEVICE_SUCCESS)
	{
		return 6;
	}
	if (KernalDeviceWait(uiPixels, VIDEO_WAIT_VBLANK, VIDEO_TEST_WAIT_TICKS) != KERNAL_DEVICE_SUCCESS)
	{
		return 7;
	}
	if ((KernalDeviceControl(uiPixels, VIDEO_BOARD_GET_SHOWN, 0, &uiNowShown) != KERNAL_DEVICE_SUCCESS) || (uiNowShown == uiShown))
	{
		return 8;
	}
	return 0;
}


//////////////////////////////////////////////////////////////////////////
//
// Row 1 is made row 0 again through the row table, and the picture is
// left as it is once two more frames have been drawn.
//
//////////////////////////////////////////////////////////////////////////
uint16 VideoTestRows(uint16 uiPixels, uint16 uiRows)
{
	uint32	uiVBlanks;
	uint16	i;

	memset(gauiVideoTestRow, 0, 4);
	if (KernalDeviceWrite(uiRows, 4, gauiVideoTestRow, 4, NULL) != KERNAL_DEVICE_SUCCESS)
	{
		return 1;
	}
	if ((KernalDeviceRead(uiRows, 8, gauiVideoTestRow, 4, NULL) != KERNAL_DEVICE_SUCCESS) || (gauiVideoTestRow[0] != (uint8)(2 * VIDEO_TEST_WIDTH)) || (gauiVideoTestRow[1] != (uint8)((2 * VIDEO_TEST_WIDTH) >> 8)))
	{
		return 2;
	}
	for (i = 0; i < 2; i++)
	{
		if (KernalDeviceWait(uiPixels, VIDEO_WAIT_VBLANK, VIDEO_TEST_WAIT_TICKS) != KERNAL_DEVICE_SUCCESS)
		{
			return 3;
		}
	}
	if ((KernalDeviceControl(uiPixels, VIDEO_BOARD_GET_VBLANKS, 0, &uiVBlanks) != KERNAL_DEVICE_SUCCESS) || (uiVBlanks < 3))
	{
		return 4;
	}
	return 0;
}


//////////////////////////////////////////////////////////////////////////
//
// The Video Board driver's test (TestHCP65KernalVideo's card starts it:
// \OS\STARTUP.TXT names it).  Exits 0 if everything did what it should, otherwise 100
// times the step that failed plus which check in it.  It leaves the
// picture shown.
//
//////////////////////////////////////////////////////////////////////////
int main(void)
{
	uint16	uiPixels;
	uint16	uiPalettes;
	uint16	uiNumbers;
	uint16	uiRows;
	uint16	uiResult;

	uiResult = VideoTestScreen();
	if (uiResult != 0)
	{
		return 500 + uiResult;
	}
	uiResult = VideoTestOpen(&uiPixels, &uiPalettes, &uiNumbers, &uiRows);
	if (uiResult != 0)
	{
		return 100 + uiResult;
	}
	uiResult = VideoTestMode(uiPixels, uiPalettes);
	if (uiResult != 0)
	{
		return 200 + uiResult;
	}
	uiResult = VideoTestDraw(uiPixels, uiNumbers);
	if (uiResult != 0)
	{
		return 300 + uiResult;
	}
	uiResult = VideoTestRows(uiPixels, uiRows);
	if (uiResult != 0)
	{
		return 400 + uiResult;
	}
	return 0;
}

