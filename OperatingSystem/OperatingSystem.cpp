#include "HCP65User.h"


#define BOOT_PROGRAM	"\\modplay.exe"
#define BOOT_NO_TABLES	200		//The exit code when the maths tables couldn't be loaded, plus why.


//In globals so the compiler can't work the answers out itself.
uint16	guiA = 300;
uint16	guiB = 200;
uint32	guiProduct;


//////////////////////////////////////////////////////////////////////////
//
// The test's boot.exe: loads the maths tables from the SD card into main
// memory (MULT, and INVERT's upper half; it is built without them,
// MathTables="false", so it can),
// multiplies and divides and gives the kernal a value, then runs
// modplay.exe (built with the tables) with its own argument and exits
// with modplay's exit code (or 100 and why it couldn't run).
//
//////////////////////////////////////////////////////////////////////////
int main(void)
{
	uint16	uiResult;
	uint16	uiExitCode;

	uiResult = KernalFileLoad(KERNAL_MATH_MULT_PATH, (void*)KERNAL_MATH_MULT_ADDRESS, KERNAL_MATH_MULT_SIZE);
	if (uiResult != KERNAL_RESULT_SUCCESS)
	{
		return BOOT_NO_TABLES + uiResult;
	}
	uiResult = KernalFileRead(KERNAL_MATH_INVERT_PATH, KERNAL_MATH_INVERT_OFFSET, (void*)KERNAL_MATH_INVERT_ADDRESS, KERNAL_MATH_INVERT_SIZE);
	if (uiResult != KERNAL_RESULT_SUCCESS)
	{
		return BOOT_NO_TABLES + uiResult;
	}

	guiProduct = (uint32)guiA * guiB;
	KernalPutValue((uint16)(guiProduct / 3));

	uiExitCode = 0;
	uiResult = KernalExecute(BOOT_PROGRAM, KernalGetArgument(), &uiExitCode);
	if (uiResult != KERNAL_RESULT_EXITED)
	{
		return 100 + uiResult;
	}
	return uiExitCode;
}
