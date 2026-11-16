#include <string.h>
#include "HCP65User.h"


//\OS\INIT.EXE: the HCP65's start up program, which the kernal runs when the card has no \OS\STARTUP.TXT.  It is built
//without the maths tables (MathTables="false") because it is what loads them, for everything after it.  Then it runs the
//shell, and runs it again each time it exits, so that there is always a command line.  It only exits if the tables
//can't be loaded or there is no shell, with INIT_NO_TABLES or INIT_NO_SHELL plus why.
#define INIT_SHELL			"\\OS\\SHELL.EXE"
#define INIT_NO_TABLES		100
#define INIT_NO_SHELL		200


//////////////////////////////////////////////////////////////////////////
//
// szText on the console.
//
//////////////////////////////////////////////////////////////////////////
static void InitWrite(const char* szText)
{
	KernalDeviceWrite(KERNAL_STANDARD_OUTPUT, 0, szText, (uint32)strlen(szText), NULL);
}


//////////////////////////////////////////////////////////////////////////
//
// MULT, and INVERT's upper half, from the SD card into main memory where
// every process has them (HCP65User.h's KERNAL_MATH_*).
//
//////////////////////////////////////////////////////////////////////////
static uint16 InitLoadTables(void)
{
	uint16	uiResult;

	uiResult = KernalFileLoad(KERNAL_MATH_MULT_PATH, (void*)KERNAL_MATH_MULT_ADDRESS, KERNAL_MATH_MULT_SIZE);
	if (uiResult != KERNAL_RESULT_SUCCESS)
	{
		return uiResult;
	}
	return KernalFileRead(KERNAL_MATH_INVERT_PATH, KERNAL_MATH_INVERT_OFFSET, (void*)KERNAL_MATH_INVERT_ADDRESS, KERNAL_MATH_INVERT_SIZE);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
int main(void)
{
	uint16	uiResult;
	uint16	uiExitCode;

	uiResult = InitLoadTables();
	if (uiResult != KERNAL_RESULT_SUCCESS)
	{
		InitWrite("INIT: the maths tables (\\OS\\Math) couldn't be loaded.\n");
		return INIT_NO_TABLES + uiResult;
	}

	for (;;)
	{
		uiExitCode = 0;
		uiResult = KernalExecute(INIT_SHELL, 0, &uiExitCode);
		if (uiResult != KERNAL_RESULT_EXITED)
		{
			InitWrite("INIT: " INIT_SHELL " couldn't be run.\n");
			return INIT_NO_SHELL + uiResult;
		}
	}
}
