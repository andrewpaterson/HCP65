#ifndef __HCP65_DRIVER_BUILDER_H__
#define __HCP65_DRIVER_BUILDER_H__
#include "BaseLib/Chars.h"
#include "CppParserLib/CPP65Builder.h"
#include "Assembler65Lib/Asm65Module.h"


#define HCP65_DRIVER_ENTRY_SYMBOL	"_KernalCreateDriver"	//extern "C" CKernalDriver* KernalCreateDriver(void).
#define HCP65_DRIVER_LIMIT			0x20000				//Linked from $10000; a driver is kept in one bank.


//Builds an HCP65 kernal driver (HCP65/Drivers) into the module the kernal loads from \OS\Drivers: its units, BaseLib
//and Libc65's runtime linked relocatably, the kernal's functions it calls left as imports.  As the kernal, it runs
//in kernel mode with no maths tables and no floats.
class CHCP65DriverBuilder
{
protected:
	CCPP65Builder	mcBuilder;
	CAsm65Module	mcModule;
	CChars			mszErrors;

public:
	void			Init(void);
	void			Kill(void);

	bool			Build(const char* szCommonLibraries, const char* szHCP65Libraries, const char* szKernal, const char* szProjectFile);

	bool			Write(const char* szFilename);
	uint8*			GetModule(void);
	size			GetModuleSize(void);
	CAsm65Linker*	GetLinker(void);
	const char*		GetErrors(void);
};


#endif // __HCP65_DRIVER_BUILDER_H__
