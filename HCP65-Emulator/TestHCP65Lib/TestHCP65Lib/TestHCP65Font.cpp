#include "BaseLib/DiskFile.h"
#include "BaseLib/FileBasic.h"
#include "BaseLib/FileUtil.h"
#include "BaseLib/GlobalDataTypesIO.h"
#include "StandardLib/Objects.h"
#include "SupportLib/Image.h"
#include "SupportLib/SFTCommon.h"
#include "SupportLib/SFTReader.h"
#include "TestLib/Assert.h"


#define TEST_HCP65_FONT_SFT			".." _FS_ ".." _FS_ ".." _FS_ "Fonts" _FS_ "CONSOLE.SFT"
#define TEST_HCP65_FONT_BITMAP		".." _FS_ ".." _FS_ ".." _FS_ "Fonts" _FS_ "CONSOLE.bmp"
#define TEST_FONT_GLYPHS			256
#define TEST_FONT_WIDTH				8
#define TEST_FONT_HEIGHT			16
#define TEST_FONT_DRAWN			189		//The glyphs of Terminus that have a pixel.
#define TEST_FONT_BITMAP_WIDTH		128		//CONSOLE.bmp: sixteen glyphs to a row, 24 bits a pixel, its rows bottom first.
#define TEST_FONT_BITMAP_HEIGHT		256
#define TEST_FONT_BITMAP_HEADER		54
#define TEST_FONT_BITMAP_SIZE		(TEST_FONT_BITMAP_HEADER + TEST_FONT_BITMAP_WIDTH * TEST_FONT_BITMAP_HEIGHT * 3)


//////////////////////////////////////////////////////////////////////////
//
// CONSOLE.SFT (which Fonts/MakeConsoleFont.py writes without SupportLib)
// is an SFT container as SupportLib reads one (this layout, SFT_VERSION,
// and no palette): 256 transparent images of
// 8 by 16 whose pixels are white ($FF), each the glyph CONSOLE.bmp (the
// same font as a picture) has.  The control characters and the space are
// blank, and H is Terminus's.
//
//////////////////////////////////////////////////////////////////////////
void TestHCP65FontSFT(void)
{
	static const uint8	auiH[TEST_FONT_HEIGHT] = { 0x00, 0x00, 0x42, 0x42, 0x42, 0x42, 0x7E, 0x42, 0x42, 0x42, 0x42, 0x42, 0x00, 0x00, 0x00, 0x00 };
	static uint8		auiBitmap[TEST_FONT_BITMAP_SIZE];
	CFileBasic		cBitmap;
	CFileBasic		cFile;
	SSFTContainer	sContainer;
	SSFTContained	sContained;
	filePos			uiPosition;
	uint32*			puiPixels;
	uint8			auiRows[TEST_FONT_HEIGHT];
	size			uiGlyph;
	size			x;
	size			y;
	size			uiWrong;
	size			uiDrawn;
	size			uiPixel;
	bool			bBlank;
	bool			bBitmap;
	Ptr<CImage>		pImage;

	cBitmap.Init(DiskFile(TEST_HCP65_FONT_BITMAP));
	AssertTrue(cBitmap.Open(EFM_Read));
	AssertSize(TEST_FONT_BITMAP_SIZE, cBitmap.Read(auiBitmap, 1, TEST_FONT_BITMAP_SIZE));
	cBitmap.Close();
	cBitmap.Kill();
	AssertInt('B', auiBitmap[0]);
	AssertInt('M', auiBitmap[1]);
	AssertInt(TEST_FONT_BITMAP_HEADER, auiBitmap[10]);
	AssertInt(TEST_FONT_BITMAP_WIDTH, auiBitmap[18]);
	AssertInt(TEST_FONT_BITMAP_HEIGHT, auiBitmap[22] | (auiBitmap[23] << 8));
	AssertInt(24, auiBitmap[28]);

	cFile.Init(DiskFile(TEST_HCP65_FONT_SFT));
	AssertTrue(cFile.Open(EFM_Read));
	AssertTrue(sContainer.Read(&cFile));
	AssertTrue(sContainer.IsValid());
	AssertInt(TEST_FONT_GLYPHS, sContainer.uiCount);

	uiWrong = 0;
	uiDrawn = 0;
	for (uiGlyph = 0; uiGlyph < TEST_FONT_GLYPHS; uiGlyph++)
	{
		uiPosition = cFile.Tell();
		AssertTrue(sContained.Read(&cFile));
		pImage = LoadSFTTransparent(&cFile);
		if (pImage.IsNull() || (pImage->GetWidth() != TEST_FONT_WIDTH) || (pImage->GetHeight() != TEST_FONT_HEIGHT))
		{
			uiWrong++;
		}
		else
		{
			//Each row a byte, its leftmost pixel bit 7.  A pixel is white or isn't there.
			puiPixels = (uint32*)pImage->GetData();
			bBlank = true;
			for (y = 0; y < TEST_FONT_HEIGHT; y++)
			{
				auiRows[y] = 0;
				for (x = 0; x < TEST_FONT_WIDTH; x++)
				{
					if (puiPixels[y * TEST_FONT_WIDTH + x] == 0xFFFFFFFF)
					{
						auiRows[y] |= (uint8)(0x80 >> x);
						bBlank = false;
					}
					else if (puiPixels[y * TEST_FONT_WIDTH + x] != 0)
					{
						uiWrong++;
					}

					//The picture's pixel: white where the glyph has one.
					uiPixel = TEST_FONT_BITMAP_HEADER + ((TEST_FONT_BITMAP_HEIGHT - 1 - ((uiGlyph / 16) * TEST_FONT_HEIGHT + y)) * TEST_FONT_BITMAP_WIDTH + (uiGlyph % 16) * TEST_FONT_WIDTH + x) * 3;
					bBitmap = (auiBitmap[uiPixel] == 0xFF) && (auiBitmap[uiPixel + 1] == 0xFF) && (auiBitmap[uiPixel + 2] == 0xFF);
					if (bBitmap != (puiPixels[y * TEST_FONT_WIDTH + x] == 0xFFFFFFFF))
					{
						uiWrong++;
					}
				}
			}
			if (!bBlank)
			{
				uiDrawn++;
			}
			if (((uiGlyph <= ' ') || ((uiGlyph >= 0x7F) && (uiGlyph < 0xA0))) && !bBlank)
			{
				uiWrong++;
			}
			if ((uiGlyph == 'H') && (memcmp(auiRows, auiH, TEST_FONT_HEIGHT) != 0))
			{
				uiWrong++;
			}
		}
		pImage = NULL;

		//The last image's distance to the next is 0; each other's is from its own start.
		if (uiGlyph == TEST_FONT_GLYPHS - 1)
		{
			AssertInt(0, sContained.uiSkipToNext);
		}
		else if (sContained.uiSkipToNext == 0)
		{
			uiWrong++;
			break;
		}
		else
		{
			cFile.Seek(uiPosition + sContained.uiSkipToNext);
		}
	}
	AssertSize(0, uiWrong);
	AssertSize(TEST_FONT_DRAWN, uiDrawn);
	cFile.Close();
	cFile.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void TestHCP65Font(void)
{
	BeginTests();

	DataIOInit();
	ObjectsInit();
	ImageChannelDescriptorInit();

	TestHCP65FontSFT();

	ImageChannelDescriptorKill();
	ObjectsKill(false);
	DataIOKill();

	TestStatistics();
}
