#ifndef __HCP65_KERNAL_BUILDER_H__
#define __HCP65_KERNAL_BUILDER_H__
#include "BaseLib/Chars.h"
#include "CppParserLib/CPP65Builder.h"
#include "Assembler65Lib/Asm65Image.h"


#define HCP65_ROM_SIZE				0x80000		//512KB of flash.
#define HCP65_ROM_CODE_END			0x40000		//The kernal's code and data: ROM banks 0-3, copied to MPU RAM.
#define HCP65_RAM_END				0x07FC00	//MPU RAM below the remap table: the kernal's code, data and bss.
#define HCP65_TABLES_START			0x400000	//The maths tables in main memory (boot.exe loads them: UserMathTables.asm).
#define HCP65_TABLES_END			0x440000	//MULT, then INVERT's upper half (HCP65User.h's KERNAL_MATH_...).
#define HCP65_MATH_MULT_SIZE		0x20000
#define HCP65_MATH_INVERT_SIZE		0x40000		//INVERT.BIN on the card: all of it.
#define HCP65_MATH_INVERT_OFFSET	0x20000		//The half boot.exe loads.
#define HCP65_MATH_MULT_PATH		"\\OS\\Math\\MULT.BIN"
#define HCP65_MATH_INVERT_PATH		"\\OS\\Math\\INVERT.BIN"


//Builds the HCP65 kernal's ROM image from its source in HCP65/Kernal: the start up (Start.asm) and the kernal's C++
//(compiled for the W65C816) with BaseLib and Libc65, without the maths tables (CCPP65Builder::SetMathTables(false):
//boot.exe loads them).  The image is 512KB of ROM, $FF where nothing is (erased flash): banks 0-3 hold what is linked
//at $000000-$03FFFF (the vectors and the kernal); banks 4-7 are free.  See Start.asm for how it starts.  The units are
//HCP65Kernal.vcxproj's, as HCP65-IDE builds HCP65Kernal.sln (its HCP65Kernal.hcp65build has the same layout).
class CHCP65KernalBuilder
{
protected:
	CCPP65Builder	mcBuilder;
	CAsm65Image		mcImage;
	uint32			muiCodeEnd;		//The end of the kernal's code and data.
	CChars			mszErrors;

public:
	void			Init(void);
	void			Kill(void);

	bool			Build(const char* szCommonLibraries, const char* szHCP65Libraries, const char* szKernal);
	bool			Write(const char* szFilename);

	uint8*			GetROM(void);
	uint32			GetCodeEnd(void);
	bool			GetSymbol(const char* szName, uint32* puiAddress);
	const char*		GetErrors(void);
	CCPP65Builder*	GetBuilder(void);

protected:
	bool			MakeImage(void);
};


#endif // __HCP65_KERNAL_BUILDER_H__
