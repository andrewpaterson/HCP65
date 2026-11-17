#include <string.h>
#include "HCP65User.h"


//\OS\SHELL.EXE: the HCP65's command line, which \OS\INIT.EXE keeps running.  It reads a line from the console
//(KERNAL_STANDARD_INPUT), echoing it, and does what it says: one of the commands below (SHELL_COMMANDS), or else runs
//the program it names.  The commands have their Linux names (ls, cat, cp ...) and paths are written with '/', as
//Linux's are ('\' is taken too): the SD card's FAT paths, which the shell gives the kernal with '\'.  One that doesn't
//start with '/' is from the current directory, which the prompt shows before its '$'.  A program is looked for as
//given, then in \OS, with ".EXE" added if it has no extension; a number after it is its argument (KernalGetArgument).
//"exit" ends the shell, and INIT.EXE starts it again.
#define SHELL_LINE_LENGTH		128
#define SHELL_PATH_LENGTH		128
#define SHELL_MAX_WORDS			8
#define SHELL_READ_SIZE			256			//What type and copy move at a time.
#define SHELL_COPY_SIZE			0x8000		//The largest file copy copies.
#define SHELL_PROGRAM_DIRECTORY	"\\OS\\"
#define SHELL_VERSION			"HCP65 shell 1"

#define SHELL_KEY_ENTER			'\r'
#define SHELL_KEY_NEW_LINE		'\n'
#define SHELL_KEY_BACKSPACE		0x08
#define SHELL_KEY_DELETE		0x7F
#define SHELL_KEY_CANCEL		0x03		//Ctrl-C: the line is dropped.
#define SHELL_KEY_ESCAPE		0x1B		//A terminal's escape sequence (an arrow key): skipped.


typedef bool	(*ShellCommandFunction)(uint16 uiWords, char** aszWords);


struct SShellCommand
{
	const char*				szName;
	ShellCommandFunction	fCommand;
	const char*				szHelp;
};


char	gszShellDirectory[SHELL_PATH_LENGTH];	//The current directory as FAT has it: "\" or "\NAME\NAME", no '\' at the end.
uint8	gauiShellBuffer[SHELL_COPY_SIZE];


//////////////////////////////////////////////////////////////////////////
//
// The console's output.
//
//////////////////////////////////////////////////////////////////////////
static void ShellWriteBytes(const void* pvBytes, uint32 uiLength)
{
	KernalDeviceWrite(KERNAL_STANDARD_OUTPUT, 0, pvBytes, uiLength, NULL);
}


static void ShellWrite(const char* szText)
{
	ShellWriteBytes(szText, (uint32)strlen(szText));
}


static void ShellWriteChar(char c)
{
	ShellWriteBytes(&c, 1);
}


//////////////////////////////////////////////////////////////////////////
//
// A FAT path as the shell shows it: with '/' for each '\'.
//
//////////////////////////////////////////////////////////////////////////
static void ShellWritePath(const char* szPath)
{
	for (; *szPath; szPath++)
	{
		ShellWriteChar((*szPath == '\\') ? '/' : *szPath);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Whether c separates a path's parts as it is typed: '/', or FAT's '\'.
//
//////////////////////////////////////////////////////////////////////////
static bool ShellIsSeparator(char c)
{
	return (c == '/') || (c == '\\');
}


//////////////////////////////////////////////////////////////////////////
//
// uiValue in decimal, right aligned in at least uiWidth characters.
//
//////////////////////////////////////////////////////////////////////////
static void ShellWriteNumber(uint32 uiValue, uint16 uiWidth)
{
	char	sz[12];
	uint16	uiLength;
	uint16	i;

	uiLength = 0;
	do
	{
		sz[uiLength++] = (char)('0' + (uiValue % 10));
		uiValue /= 10;
	}
	while (uiValue != 0);
	for (i = uiLength; i < uiWidth; i++)
	{
		ShellWriteChar(' ');
	}
	while (uiLength > 0)
	{
		ShellWriteChar(sz[--uiLength]);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// szText in at least uiWidth characters, padded after.
//
//////////////////////////////////////////////////////////////////////////
static void ShellWritePadded(const char* szText, uint16 uiWidth)
{
	uint16	uiLength;

	ShellWrite(szText);
	for (uiLength = (uint16)strlen(szText); uiLength < uiWidth; uiLength++)
	{
		ShellWriteChar(' ');
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The next byte typed, sleeping until there is one.
//
//////////////////////////////////////////////////////////////////////////
static char ShellReadChar(void)
{
	uint32	uiDone;
	char	c;

	for (;;)
	{
		uiDone = 0;
		KernalDeviceRead(KERNAL_STANDARD_INPUT, 0, &c, 1, &uiDone);
		if (uiDone == 1)
		{
			return c;
		}
		KernalDeviceWait(KERNAL_STANDARD_INPUT, KERNAL_CONSOLE_WAIT_INPUT, KERNAL_DEVICE_TICKS_PER_SECOND);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A line typed, shown as it is typed, into szLine (SHELL_LINE_LENGTH).
// Backspace takes back the last character; Ctrl-C drops the line.
//
//////////////////////////////////////////////////////////////////////////
static void ShellReadLine(char* szLine)
{
	uint16	uiLength;
	char	c;

	uiLength = 0;
	for (;;)
	{
		c = ShellReadChar();
		if ((c == SHELL_KEY_ENTER) || (c == SHELL_KEY_NEW_LINE))
		{
			ShellWriteChar('\n');
			break;
		}
		else if ((c == SHELL_KEY_BACKSPACE) || (c == SHELL_KEY_DELETE))
		{
			if (uiLength > 0)
			{
				uiLength--;
				ShellWrite("\b \b");
			}
		}
		else if (c == SHELL_KEY_CANCEL)
		{
			ShellWrite("^C\n");
			uiLength = 0;
			break;
		}
		else if (c == SHELL_KEY_ESCAPE)
		{
			//ESC [ then anything up to a letter.
			c = ShellReadChar();
			if (c == '[')
			{
				do
				{
					c = ShellReadChar();
				}
				while (!(((c >= 'A') && (c <= 'Z')) || ((c >= 'a') && (c <= 'z')) || (c == '~')));
			}
		}
		else if ((((uint8)c >= 0x20) && ((uint8)c < 0x7F)) || ((uint8)c >= 0xA0))
		{
			if (uiLength < SHELL_LINE_LENGTH - 1)
			{
				szLine[uiLength++] = c;
				ShellWriteChar(c);
			}
		}
	}
	szLine[uiLength] = '\0';
}


//////////////////////////////////////////////////////////////////////////
//
// szLine's words, split at spaces, into aszWords (SHELL_MAX_WORDS): the
// spaces become '\0'.  Returns how many.
//
//////////////////////////////////////////////////////////////////////////
static uint16 ShellSplit(char* szLine, char** aszWords)
{
	uint16	uiWords;

	uiWords = 0;
	for (;;)
	{
		while ((*szLine == ' ') || (*szLine == '\t'))
		{
			*szLine++ = '\0';
		}
		if ((*szLine == '\0') || (uiWords == SHELL_MAX_WORDS))
		{
			return uiWords;
		}
		aszWords[uiWords++] = szLine;
		while ((*szLine != '\0') && (*szLine != ' ') && (*szLine != '\t'))
		{
			szLine++;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// szName as a full FAT path in szPath (SHELL_PATH_LENGTH): from the
// current directory unless it starts with '/', its parts joined with '\',
// and with "." and ".." taken out.
// False if it is too long.
//
//////////////////////////////////////////////////////////////////////////
static bool ShellMakePath(const char* szName, char* szPath)
{
	char		szJoined[SHELL_PATH_LENGTH * 2];
	const char*	szPart;
	uint16		uiPartLength;
	uint16		uiLength;

	szJoined[0] = '\0';
	if (!ShellIsSeparator(szName[0]))
	{
		strcpy(szJoined, gszShellDirectory);
		strcat(szJoined, "\\");
	}
	if (strlen(szJoined) + strlen(szName) >= sizeof(szJoined))
	{
		return false;
	}
	strcat(szJoined, szName);

	//Each part between '\'s onto the path, which never ends in '\' (but is "\" for the root).
	uiLength = 0;
	szPart = szJoined;
	for (;;)
	{
		while (ShellIsSeparator(*szPart))
		{
			szPart++;
		}
		if (*szPart == '\0')
		{
			break;
		}
		uiPartLength = 0;
		while ((szPart[uiPartLength] != '\0') && !ShellIsSeparator(szPart[uiPartLength]))
		{
			uiPartLength++;
		}
		if ((uiPartLength == 1) && (szPart[0] == '.'))
		{
		}
		else if ((uiPartLength == 2) && (szPart[0] == '.') && (szPart[1] == '.'))
		{
			while ((uiLength > 0) && (szPath[uiLength - 1] != '\\'))
			{
				uiLength--;
			}
			if (uiLength > 0)
			{
				uiLength--;
			}
		}
		else
		{
			if (uiLength + 1 + uiPartLength >= SHELL_PATH_LENGTH)
			{
				return false;
			}
			szPath[uiLength++] = '\\';
			memcpy(&szPath[uiLength], szPart, uiPartLength);
			uiLength += uiPartLength;
		}
		szPart += uiPartLength;
	}
	if (uiLength == 0)
	{
		szPath[uiLength++] = '\\';
	}
	szPath[uiLength] = '\0';
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The path of szName, or a message saying it can't be had.
//
//////////////////////////////////////////////////////////////////////////
static bool ShellPathOf(const char* szName, char* szPath)
{
	if (!ShellMakePath(szName, szPath))
	{
		ShellWrite("That path is too long.\n");
		return false;
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether the shell's command was given at least uiNeeded words after its
// name; if not, how it is used.
//
//////////////////////////////////////////////////////////////////////////
static bool ShellNeeds(uint16 uiWords, uint16 uiNeeded, const char* szUsage)
{
	if (uiWords > uiNeeded)
	{
		return true;
	}
	ShellWrite("Use: ");
	ShellWrite(szUsage);
	ShellWriteChar('\n');
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether the directory szPath is there.
//
//////////////////////////////////////////////////////////////////////////
static bool ShellIsDirectory(const char* szPath)
{
	SKernalDirectoryEntry	sEntry;
	uint16					uiResult;

	uiResult = KernalDirectoryEntry(szPath, 0, &sEntry);
	return (uiResult == KERNAL_RESULT_SUCCESS) || (uiResult == KERNAL_RESULT_END);
}


static bool ShellHelp(uint16 uiWords, char** aszWords);


//////////////////////////////////////////////////////////////////////////
//
// ls [path]: the directory's entries, a file's size before its name and a
// directory's name with a '/' after it.
//
//////////////////////////////////////////////////////////////////////////
static bool ShellLs(uint16 uiWords, char** aszWords)
{
	SKernalDirectoryEntry	sEntry;
	char					szPath[SHELL_PATH_LENGTH];
	uint16					uiIndex;
	uint16					uiResult;

	if (!ShellPathOf((uiWords > 1) ? aszWords[1] : ".", szPath))
	{
		return true;
	}
	for (uiIndex = 0;; uiIndex++)
	{
		uiResult = KernalDirectoryEntry(szPath, uiIndex, &sEntry);
		if (uiResult != KERNAL_RESULT_SUCCESS)
		{
			break;
		}
		if (sEntry.uiAttributes & KERNAL_FILE_DIRECTORY)
		{
			ShellWrite("            ");
			ShellWrite(sEntry.szName);
			ShellWriteChar('/');
		}
		else
		{
			ShellWriteNumber(sEntry.uiSize, 10);
			ShellWrite("  ");
			ShellWrite(sEntry.szName);
		}
		ShellWriteChar('\n');
	}
	if (uiResult != KERNAL_RESULT_END)
	{
		ShellWrite("ls: ");
		ShellWritePath(szPath);
		ShellWrite(" isn't a directory.\n");
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// cd [path]: the current directory changed, to the root without a path.
//
//////////////////////////////////////////////////////////////////////////
static bool ShellCd(uint16 uiWords, char** aszWords)
{
	char	szPath[SHELL_PATH_LENGTH];

	if (!ShellPathOf((uiWords > 1) ? aszWords[1] : "\\", szPath))
	{
		return true;
	}
	if (!ShellIsDirectory(szPath))
	{
		ShellWrite("cd: ");
		ShellWritePath(szPath);
		ShellWrite(" isn't a directory.\n");
		return true;
	}
	strcpy(gszShellDirectory, szPath);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// pwd: the current directory.
//
//////////////////////////////////////////////////////////////////////////
static bool ShellPwd(uint16 uiWords, char** aszWords)
{
	ShellWritePath(gszShellDirectory);
	ShellWriteChar('\n');
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// cat file: the file's bytes, to the console.
//
//////////////////////////////////////////////////////////////////////////
static bool ShellCat(uint16 uiWords, char** aszWords)
{
	char	szPath[SHELL_PATH_LENGTH];
	uint32	uiSize;
	uint32	uiOffset;
	uint32	uiChunk;

	if (!ShellNeeds(uiWords, 1, "cat <file>") || !ShellPathOf(aszWords[1], szPath))
	{
		return true;
	}
	uiSize = KernalFileSize(szPath);
	if (uiSize == KERNAL_FILE_NOT_FOUND)
	{
		ShellWritePath(szPath);
		ShellWrite(" isn't there.\n");
		return true;
	}
	for (uiOffset = 0; uiOffset < uiSize; uiOffset += uiChunk)
	{
		uiChunk = ((uiSize - uiOffset) < SHELL_READ_SIZE) ? (uiSize - uiOffset) : SHELL_READ_SIZE;
		if (KernalFileRead(szPath, uiOffset, gauiShellBuffer, uiChunk) != KERNAL_RESULT_SUCCESS)
		{
			ShellWrite("\nIt couldn't be read.\n");
			return true;
		}
		ShellWriteBytes(gauiShellBuffer, uiChunk);
	}
	if ((uiSize > 0) && (gauiShellBuffer[uiChunk - 1] != '\n'))
	{
		ShellWriteChar('\n');
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// cp from to: a file of up to SHELL_COPY_SIZE bytes, replacing to if it
// is there.
//
//////////////////////////////////////////////////////////////////////////
static bool ShellCp(uint16 uiWords, char** aszWords)
{
	char	szFrom[SHELL_PATH_LENGTH];
	char	szTo[SHELL_PATH_LENGTH];
	uint32	uiSize;

	if (!ShellNeeds(uiWords, 2, "cp <from> <to>") || !ShellPathOf(aszWords[1], szFrom) || !ShellPathOf(aszWords[2], szTo))
	{
		return true;
	}
	uiSize = KernalFileSize(szFrom);
	if (uiSize == KERNAL_FILE_NOT_FOUND)
	{
		ShellWritePath(szFrom);
		ShellWrite(" isn't there.\n");
		return true;
	}
	if (uiSize > SHELL_COPY_SIZE)
	{
		ShellWrite("It is too big to copy.\n");
		return true;
	}
	if ((uiSize > 0) && (KernalFileRead(szFrom, 0, gauiShellBuffer, uiSize) != KERNAL_RESULT_SUCCESS))
	{
		ShellWrite("It couldn't be read.\n");
		return true;
	}
	if (KernalFileWrite(szTo, gauiShellBuffer, uiSize) != KERNAL_RESULT_SUCCESS)
	{
		ShellWritePath(szTo);
		ShellWrite(" couldn't be written.\n");
		return true;
	}
	ShellWriteNumber(uiSize, 0);
	ShellWrite(" bytes copied.\n");
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// rm file.
//
//////////////////////////////////////////////////////////////////////////
static bool ShellRm(uint16 uiWords, char** aszWords)
{
	char	szPath[SHELL_PATH_LENGTH];

	if (!ShellNeeds(uiWords, 1, "rm <file>") || !ShellPathOf(aszWords[1], szPath))
	{
		return true;
	}
	if (KernalFileDelete(szPath) != KERNAL_RESULT_SUCCESS)
	{
		ShellWritePath(szPath);
		ShellWrite(" couldn't be deleted.\n");
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// mkdir directory.
//
//////////////////////////////////////////////////////////////////////////
static bool ShellMkdir(uint16 uiWords, char** aszWords)
{
	char	szPath[SHELL_PATH_LENGTH];

	if (!ShellNeeds(uiWords, 1, "mkdir <directory>") || !ShellPathOf(aszWords[1], szPath))
	{
		return true;
	}
	if (KernalMakeDirectory(szPath) != KERNAL_RESULT_SUCCESS)
	{
		ShellWritePath(szPath);
		ShellWrite(" couldn't be made.\n");
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// lsdev: each board in the device sockets, and its driver.
//
//////////////////////////////////////////////////////////////////////////
static bool ShellLsdev(uint16 uiWords, char** aszWords)
{
	SKernalDeviceInfo	sInfo;
	uint16				uiIndex;

	ShellWrite("Socket  Board                 Driver        Units\n");
	for (uiIndex = 0; KernalDeviceInfo(uiIndex, &sInfo) == KERNAL_DEVICE_SUCCESS; uiIndex++)
	{
		ShellWriteNumber(sInfo.uiSlot, 6);
		ShellWrite("  ");
		ShellWritePadded(sInfo.szName, 22);
		ShellWritePadded((sInfo.uiNumUnits == 0) ? "-" : (sInfo.szDriverFile[0] ? sInfo.szDriverFile : "built in"), 12);
		ShellWriteNumber(sInfo.uiNumUnits, 7);
		if (sInfo.uiOpen)
		{
			ShellWrite("  open");
		}
		ShellWriteChar('\n');
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// lsmod: each driver file loaded, and the boards it drives.
//
//////////////////////////////////////////////////////////////////////////
static bool ShellLsmod(uint16 uiWords, char** aszWords)
{
	SKernalDeviceInfo	sInfo;
	SKernalDeviceInfo	sOther;
	uint16				uiIndex;
	uint16				uiOther;
	uint16				uiBoards;
	bool				bShown;

	for (uiIndex = 0; KernalDeviceInfo(uiIndex, &sInfo) == KERNAL_DEVICE_SUCCESS; uiIndex++)
	{
		if ((sInfo.uiNumUnits == 0) || (sInfo.szDriverFile[0] == '\0'))
		{
			continue;
		}

		//Once, for the first board that names it.
		bShown = false;
		for (uiOther = 0; uiOther < uiIndex; uiOther++)
		{
			if ((KernalDeviceInfo(uiOther, &sOther) == KERNAL_DEVICE_SUCCESS) && (sOther.uiNumUnits != 0) && (strcmp(sOther.szDriverFile, sInfo.szDriverFile) == 0))
			{
				bShown = true;
			}
		}
		if (bShown)
		{
			continue;
		}
		uiBoards = 0;
		for (uiOther = uiIndex; KernalDeviceInfo(uiOther, &sOther) == KERNAL_DEVICE_SUCCESS; uiOther++)
		{
			if ((sOther.uiNumUnits != 0) && (strcmp(sOther.szDriverFile, sInfo.szDriverFile) == 0))
			{
				uiBoards++;
			}
		}
		ShellWritePadded(sInfo.szDriverFile, 14);
		ShellWrite(sInfo.szName);
		if (uiBoards > 1)
		{
			ShellWrite(" x ");
			ShellWriteNumber(uiBoards, 0);
		}
		ShellWriteChar('\n');
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// free: main memory and the kernal's heap.
//
//////////////////////////////////////////////////////////////////////////
static bool ShellFree(uint16 uiWords, char** aszWords)
{
	SKernalMemoryInfo	sInfo;

	if (KernalMemoryInfo(&sInfo) != KERNAL_RESULT_SUCCESS)
	{
		ShellWrite("The kernal didn't say.\n");
		return true;
	}
	ShellWrite("Main memory      ");
	ShellWriteNumber(sInfo.uiMainBanks * 64, 6);
	ShellWrite("KB (");
	ShellWriteNumber(sInfo.uiMainBanks, 0);
	ShellWrite(" banks)\nFor programs     ");
	ShellWriteNumber(sInfo.uiProcessBanks * 64, 6);
	ShellWrite("KB, ");
	ShellWriteNumber(sInfo.uiFreeBanks * 64, 0);
	ShellWrite("KB free\nKernal heap      ");
	ShellWriteNumber(sInfo.uiHeapUsed, 6);
	ShellWrite(" of ");
	ShellWriteNumber(sInfo.uiHeapSize, 0);
	ShellWrite(" bytes used\n");
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// mode [n]: the console's size, or its video mode (KERNAL_CONSOLE_MODE_*:
// 0 640x480, 1 800x600, 2 1024x768).
//
//////////////////////////////////////////////////////////////////////////
static bool ShellMode(uint16 uiWords, char** aszWords)
{
	uint32	uiColumns;
	uint32	uiRows;
	uint32	uiMode;
	char*	pc;

	if (uiWords > 1)
	{
		uiMode = 0;
		for (pc = aszWords[1]; (*pc >= '0') && (*pc <= '9'); pc++)
		{
			uiMode = uiMode * 10 + (uint32)(*pc - '0');
		}
		if ((*pc != '\0') || (KernalDeviceControl(KERNAL_STANDARD_OUTPUT, KERNAL_CONSOLE_SET_MODE, uiMode, NULL) != KERNAL_DEVICE_SUCCESS))
		{
			ShellWrite("The console hasn't that mode (0 640x480, 1 800x600, 2 1024x768 on the Video Board).\n");
			return true;
		}
	}
	uiColumns = 0;
	uiRows = 0;
	KernalDeviceControl(KERNAL_STANDARD_OUTPUT, KERNAL_CONSOLE_GET_COLUMNS, 0, &uiColumns);
	KernalDeviceControl(KERNAL_STANDARD_OUTPUT, KERNAL_CONSOLE_GET_ROWS, 0, &uiRows);
	ShellWriteNumber(uiColumns, 0);
	ShellWrite(" columns, ");
	ShellWriteNumber(uiRows, 0);
	ShellWrite(" rows\n");
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
static bool ShellClear(uint16 uiWords, char** aszWords)
{
	KernalDeviceControl(KERNAL_STANDARD_OUTPUT, KERNAL_CONSOLE_CLEAR, 0, NULL);
	return true;
}


static bool ShellUname(uint16 uiWords, char** aszWords)
{
	ShellWrite(SHELL_VERSION "\n");
	return true;
}


static bool ShellExit(uint16 uiWords, char** aszWords)
{
	return false;
}


static const SShellCommand	gasShellCommands[] =
{
	{ "help",		ShellHelp,		"What the commands are" },
	{ "ls",			ShellLs,		"ls [path]: a directory's files" },
	{ "cd",			ShellCd,		"cd [path]: change the current directory (to the root without a path)" },
	{ "pwd",		ShellPwd,		"The current directory" },
	{ "cat",		ShellCat,		"cat <file>: show a file" },
	{ "cp",			ShellCp,		"cp <from> <to>: copy a file" },
	{ "rm",			ShellRm,		"rm <file>: delete a file" },
	{ "mkdir",		ShellMkdir,		"mkdir <directory>: make a directory" },
	{ "lsdev",		ShellLsdev,		"The boards in the device sockets" },
	{ "lsmod",		ShellLsmod,		"The drivers loaded" },
	{ "free",		ShellFree,		"Main memory and the kernal's heap" },
	{ "mode",		ShellMode,		"mode [0|1|2]: the console's size, or its video mode" },
	{ "clear",		ShellClear,		"Clear the screen" },
	{ "uname",		ShellUname,		"The shell's version" },
	{ "exit",		ShellExit,		"End the shell" },
	{ NULL,			NULL,			NULL }
};


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
static bool ShellHelp(uint16 uiWords, char** aszWords)
{
	const SShellCommand*	psCommand;

	for (psCommand = gasShellCommands; psCommand->szName; psCommand++)
	{
		ShellWritePadded(psCommand->szName, 9);
		ShellWrite(psCommand->szHelp);
		ShellWriteChar('\n');
	}
	ShellWrite("Anything else is a program to run, with a number for its argument.\n");
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// szName's program in szPath: as given, then in \OS, with ".EXE" if it has
// no extension.  False if it isn't there.
//
//////////////////////////////////////////////////////////////////////////
static bool ShellFindProgram(const char* szName, char* szPath)
{
	char	szWithExtension[SHELL_PATH_LENGTH];
	char	szInOS[SHELL_PATH_LENGTH];

	if (strlen(szName) + 5 >= SHELL_PATH_LENGTH)
	{
		return false;
	}
	strcpy(szWithExtension, szName);
	if (!strchr(szName, '.'))
	{
		strcat(szWithExtension, ".EXE");
	}
	if (ShellMakePath(szWithExtension, szPath) && (KernalFileSize(szPath) != KERNAL_FILE_NOT_FOUND))
	{
		return true;
	}
	if (strchr(szName, '\\') || strchr(szName, '/') || (strlen(SHELL_PROGRAM_DIRECTORY) + strlen(szWithExtension) >= SHELL_PATH_LENGTH))
	{
		return false;
	}
	strcpy(szInOS, SHELL_PROGRAM_DIRECTORY);
	strcat(szInOS, szWithExtension);
	return ShellMakePath(szInOS, szPath) && (KernalFileSize(szPath) != KERNAL_FILE_NOT_FOUND);
}


//////////////////////////////////////////////////////////////////////////
//
// Runs the program aszWords[0] names, with aszWords[1] as its argument if
// it is a number, and waits for it to exit.
//
//////////////////////////////////////////////////////////////////////////
static void ShellRun(uint16 uiWords, char** aszWords)
{
	char	szPath[SHELL_PATH_LENGTH];
	uint16	uiArgument;
	uint16	uiExitCode;
	uint16	uiResult;
	char*	pc;

	if (!ShellFindProgram(aszWords[0], szPath))
	{
		ShellWrite(aszWords[0]);
		ShellWrite(" isn't a command or a program.\n");
		return;
	}
	uiArgument = 0;
	if (uiWords > 1)
	{
		for (pc = aszWords[1]; (*pc >= '0') && (*pc <= '9'); pc++)
		{
			uiArgument = (uint16)(uiArgument * 10 + (*pc - '0'));
		}
	}
	uiExitCode = 0;
	uiResult = KernalExecute(szPath, uiArgument, &uiExitCode);
	if (uiResult != KERNAL_RESULT_EXITED)
	{
		ShellWritePath(szPath);
		ShellWrite(" couldn't be run (");
		ShellWriteNumber(uiResult, 0);
		ShellWrite(").\n");
	}
	else if (uiExitCode != 0)
	{
		ShellWrite("Exit code ");
		ShellWriteNumber(uiExitCode, 0);
		ShellWriteChar('\n');
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Does what szLine says.  False if it says to end the shell.
//
//////////////////////////////////////////////////////////////////////////
static bool ShellDo(char* szLine)
{
	char*					aszWords[SHELL_MAX_WORDS];
	uint16					uiWords;
	const SShellCommand*	psCommand;

	uiWords = ShellSplit(szLine, aszWords);
	if (uiWords == 0)
	{
		return true;
	}
	for (psCommand = gasShellCommands; psCommand->szName; psCommand++)
	{
		if (strcmp(aszWords[0], psCommand->szName) == 0)
		{
			return psCommand->fCommand(uiWords, aszWords);
		}
	}
	ShellRun(uiWords, aszWords);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Prompts with the current directory, reads a line and does it, until
// "exit".
//
//////////////////////////////////////////////////////////////////////////
int main(void)
{
	char	szLine[SHELL_LINE_LENGTH];

	strcpy(gszShellDirectory, "\\");
	ShellWrite(SHELL_VERSION ".  Type help for the commands.\n");
	for (;;)
	{
		ShellWritePath(gszShellDirectory);
		ShellWrite(" $ ");
		ShellReadLine(szLine);
		if (!ShellDo(szLine))
		{
			return 0;
		}
	}
}
