#ifndef __HCP65_KERNAL_BUILDER_H__
#define __HCP65_KERNAL_BUILDER_H__
#include "BaseLib/Chars.h"
#include "CppCompilerLib/CPP65Builder.h"
#include "Assembler65Lib/Asm65Image.h"


#define HCP65_ROM_SIZE				0x80000		//512KB of flash: the boot loader.
#define HCP65_BOOT_TEXT				0x40000		//The boot loader runs from MPU RAM banks 4 and 5 (HCP65Boot.hcp65build).
#define HCP65_BOOT_END				0x60000		//Its code, data and bss, below the heap.
#define HCP65_OS_START				0x00FF00	//OS.SYS: bank 0's stubs and vectors, then banks 1 to 3 (HCP65Kernal.hcp65build).
#define HCP65_OS_CODE_END			0x40000
#define HCP65_OS_SIZE				(HCP65_OS_CODE_END - HCP65_OS_START)
#define HCP65_RAM_END				0x07FC00	//MPU RAM below the remap table: the kernal's code, data and bss.
#define HCP65_OS_PATH				"\\OS\\OS.SYS"
#define HCP65_TABLES_START			0x400000	//The maths tables in main memory (boot.exe loads them: UserMathTables.asm).
#define HCP65_TABLES_END			0x440000	//MULT, then INVERT's upper half (HCP65User.h's KERNAL_MATH_...).
#define HCP65_MATH_MULT_SIZE		0x20000
#define HCP65_MATH_INVERT_SIZE		0x40000		//INVERT.BIN on the card: all of it.
#define HCP65_MATH_INVERT_OFFSET	0x20000		//The half boot.exe loads.
#define HCP65_MATH_MULT_PATH		"\\OS\\Math\\MULT.BIN"
#define HCP65_MATH_INVERT_PATH		"\\OS\\Math\\INVERT.BIN"


//What a CHCP65KernalBuilder builds.
enum EHCP65KernalImage
{
	HKI_Boot,	//The boot loader ROM, from HCP65/Boot.
	HKI_OS		//OS.SYS, the kernal, from HCP65/Kernal.
};


//Builds the boot loader's ROM image or the kernal's OS.SYS from their source, as HCP65-IDE builds HCP65Boot.sln and
//HCP65Kernal.sln (their .hcp65build files have the same layouts): the start up (BootStart.asm or Start.asm) and the C++
//(compiled for the W65C816) with BaseLib and Libc65, without floats or the maths tables (CCPP65Builder::
//SetMathTables(false): the start up program loads them).  The boot loader's ROM is 512KB, $FF where nothing is
//(erased flash): the stubs and vectors at $FF00, and banks 4 and 5 what is linked to run there.  OS.SYS is the bytes
//from $00FF00 to the end of bank 3, which the boot loader puts in MPU RAM.  The boot loader shares the kernal's units
//(HCP65Boot.vcxproj names them), with HCP65/Kernal as its include directory.
class CHCP65KernalBuilder
{
protected:
	CCPP65Builder	mcBuilder;
	CAsm65Image		mcImage;
	EHCP65KernalImage	meImage;
	uint32			muiCodeEnd;		//The end of the code and data, above bank 0.
	CChars			mszErrors;

public:
	void			Init(EHCP65KernalImage eImage);
	void			Kill(void);

	bool			Build(const char* szCommonLibraries, const char* szHCP65Libraries, const char* szKernal, const char* szBoot);
	bool			Write(const char* szFilename);

	uint8*			GetImage(void);
	uint32			GetImageSize(void);
	uint8*			GetImageAt(uint32 uiAddress);
	uint32			GetCodeEnd(void);
	bool			GetSymbol(const char* szName, uint32* puiAddress);
	const char*		GetErrors(void);
	CCPP65Builder*	GetBuilder(void);

protected:
	bool			MakeImage(void);
};


#endif // __HCP65_KERNAL_BUILDER_H__
