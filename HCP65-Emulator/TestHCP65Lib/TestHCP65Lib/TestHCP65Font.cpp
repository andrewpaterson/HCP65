#include "BaseLib/DiskFile.h"
#include "BaseLib/FileBasic.h"
#include "BaseLib/FileUtil.h"
#include "BaseLib/GlobalDataTypesIO.h"
#include "StandardLib/Objects.h"
#include "SupportLib/Image.h"
#include "SupportLib/SFTCommon.h"
#include "SupportLib/SFTReader.h"
#include "TestLib/Assert.h"


#define TEST_HCP65_FONT_BITMAP		".." _FS_ ".." _FS_ ".." _FS_ "Fonts" _FS_ "CONSOLE.FNT"
#define TEST_HCP65_FONT_SFT			".." _FS_ ".." _FS_ ".." _FS_ "Fonts" _FS_ "CONSOLE.SFT"
#define TEST_FONT_GLYPHS			256
#define TEST_FONT_WIDTH				8
#define TEST_FONT_HEIGHT			16


//////////////////////////////////////////////////////////////////////////
//
// CONSOLE.SFT (which Fonts/MakeConsoleFont.py writes without SupportLib)
// is an SFT container as SupportLib reads one: 256 transparent images of
// 8 by 16, each CONSOLE.FNT's glyph with its pixels white ($FF) and the
// rest not there.
//
//////////////////////////////////////////////////////////////////////////
void TestHCP65FontSFT(void)
{
	CFileBasic		cBitmap;
	CFileBasic		cFile;
	uint8			auiBitmap[TEST_FONT_GLYPHS * TEST_FONT_HEIGHT];
	SSFTContainer	sContainer;
	SSFTContained	sContained;
	filePos			uiPosition;
	uint32*			puiPixels;
	uint32			auiExpected[TEST_FONT_WIDTH * TEST_FONT_HEIGHT];
	size			uiGlyph;
	size			x;
	size			y;
	size			uiWrong;
	Ptr<CImage>		pImage;

	cBitmap.Init(DiskFile(TEST_HCP65_FONT_BITMAP));
	AssertTrue(cBitmap.Open(EFM_Read));
	AssertSize(sizeof(auiBitmap), cBitmap.Read(auiBitmap, 1, sizeof(auiBitmap)));
	cBitmap.Close();
	cBitmap.Kill();

	cFile.Init(DiskFile(TEST_HCP65_FONT_SFT));
	AssertTrue(cFile.Open(EFM_Read));
	AssertTrue(sContainer.Read(&cFile));
	AssertInt(SFT_TYPE_CONTAINER, sContainer.uiType);
	AssertInt(TEST_FONT_GLYPHS, sContainer.uiCount);

	uiWrong = 0;
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
			for (y = 0; y < TEST_FONT_HEIGHT; y++)
			{
				for (x = 0; x < TEST_FONT_WIDTH; x++)
				{
					auiExpected[y * TEST_FONT_WIDTH + x] = (auiBitmap[uiGlyph * TEST_FONT_HEIGHT + y] & (0x80 >> x)) ? 0xFFFFFFFF : 0;
				}
			}
			puiPixels = (uint32*)pImage->GetData();
			if (memcmp(auiExpected, puiPixels, sizeof(auiExpected)) != 0)
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
