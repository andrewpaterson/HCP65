#include <string.h>
#include "BaseLib/FileUtil.h"
#include "BaseLib/DiskFile.h"
#include "Assembler65Lib/Asm65Image.h"
#include "TestLib/Assert.h"


//////////////////////////////////////////////////////////////////////////
//
// Segments go to their regions' offsets, the rest is the fill, and the
// image is written whole.
//
//////////////////////////////////////////////////////////////////////////
void TestAsm65ImageMake(void)
{
	CAsm65Image		cImage;
	CAsm65Output	cOutput;
	CChars			szErrors;
	CFileUtil		cFileUtil;
	CDiskFile		cFile;
	uint8*			puiImage;
	uint8			auiRead[0x40];

	cOutput.Init();
	cOutput.Write(0x0010, 0x11);
	cOutput.Write(0x0011, 0x22);
	cOutput.Write(0x8000, 0x33);
	cOutput.Write(0x8001, 0x44);

	cImage.Init("Image.rom", 0x40, 0xEA, 0x9000, 0x100);
	cImage.AddRegion(0x0000, 0x0020, 0x0000);
	cImage.AddRegion(0x8000, 0x8020, 0x0020);
	AssertString("Image.rom", cImage.GetFile());
	AssertInt(0x40, cImage.GetSize());
	AssertInt(0xEA, cImage.GetFill());
	AssertInt(0x9000, cImage.GetLimit());
	AssertInt(0x100, cImage.GetLoad());
	AssertSize(2, cImage.NumRegions());
	AssertTrue(cImage.GetImage() == NULL);

	szErrors.Init();
	AssertTrue(cImage.Make(&cOutput, &szErrors));
	AssertString("", szErrors.Text());
	puiImage = cImage.GetImage();
	AssertTrue(puiImage != NULL);
	AssertIntHex(0xEA, puiImage[0x0F]);
	AssertIntHex(0x11, puiImage[0x10]);
	AssertIntHex(0x22, puiImage[0x11]);
	AssertIntHex(0xEA, puiImage[0x12]);
	AssertIntHex(0x33, puiImage[0x20]);
	AssertIntHex(0x44, puiImage[0x21]);
	AssertIntHex(0xEA, puiImage[0x3F]);

	cFileUtil.MakeDir("Output");
	AssertTrue(cImage.Write("Output" _FS_ "Image.rom"));
	cFile.Init("Output" _FS_ "Image.rom");
	AssertTrue(cFile.Open(EFM_Read));
	AssertLong(0x40, cFile.Size());
	AssertTrue(cFile.Read(auiRead, 0x40, 1) == 1);
	cFile.Close();
	cFile.Kill();
	AssertTrue(memcmp(auiRead, puiImage, 0x40) == 0);

	//A segment just past a region's end is in none.
	cOutput.Write(0x0020, 0x55);
	AssertFalse(cImage.Make(&cOutput, &szErrors));
	AssertString("A segment at 0x20 is outside the image's regions.\n", szErrors.Text());
	szErrors.Kill();

	cImage.Kill();
	cOutput.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// An image that hasn't been made writes nothing.
//
//////////////////////////////////////////////////////////////////////////
void TestAsm65ImageUnmade(void)
{
	CAsm65Image	cImage;

	cImage.Init();
	AssertInt(0xFF, cImage.GetFill());
	AssertSize(0, cImage.NumRegions());
	AssertFalse(cImage.Write("Output" _FS_ "Unmade.rom"));
	cImage.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void TestAsm65Image(void)
{
	BeginTests();

	TestAsm65ImageMake();
	TestAsm65ImageUnmade();

	TestStatistics();
}

