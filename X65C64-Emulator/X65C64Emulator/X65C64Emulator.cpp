/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela X65C64Emulator

Codaphela X65C64Emulator is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela X65C64Emulator is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with Codaphela X65C64Emulator.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "BaseLib/FileUtil.h"
#include "BaseLib/NativeThread.h"
#include "BaseLib/Logger.h"
#include "MicroprocessorLib/65C64.h"
#include "X65C64Emulator.h"


//////////////////////////////////////////////////////////////////////////
//
// The machine, its program loaded (if the command line names one) and
// reset, and the debugger's channel made.  False, said, if an argument is
// wrong or the program or the channel can't be had.
//
//////////////////////////////////////////////////////////////////////////
bool CX65C64Emulator::Init(int argc, char* argv[])
{
	const char*	szProgram;
	bool		bResult;

	mcMemory.Init();
	mszConsole.Init();
	mcMemory.SetConsole(&mszConsole);
	mcMemory.SetConsoleInput(ReadConsole, this);
	mcHostFiles.Init();
	mcMemory.SetHostFiles(&mcHostFiles);
	mcTarget.Init(mcMemory.Byte(0), mcMemory.GetLowSize());
	mcMemory.InitFast65C64(mcTarget.GetMPU());
	mcDebugger.Init();
	muiLoad = X65C64_EMULATOR_PROGRAM_ADDRESS;

	szProgram = NULL;
	bResult = mcDebugger.ParseArguments(argc, argv) && ParseArguments(argc, argv, &szProgram);
	bResult = bResult && (!szProgram || LoadProgram(szProgram));
	mcMemory.SetLong(VECTOR_65C64_BASE + 4 * VECTOR_65C64_RESET, muiLoad);
	mcTarget.Reset();
	if (bResult && mcDebugger.IsEnabled())
	{
		bResult = mcDebugger.Start(&mcTarget);
	}
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CX65C64Emulator::Kill(void)
{
	ShowConsole();
	mcDebugger.Kill();
	mcTarget.Kill();
	mcMemory.SetHostFiles(NULL);
	mcHostFiles.Kill();
	mcMemory.SetConsoleInput(NULL, NULL);
	mcMemory.SetConsole(NULL);
	mszConsole.Kill();
	mcMemory.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The emulator's own arguments (the debugger's are CEmulatorDebugger's):
// --load, --host and the program file.  False, said, if one is wrong.
//
//////////////////////////////////////////////////////////////////////////
bool CX65C64Emulator::ParseArguments(int argc, char* argv[], const char** pszProgram)
{
	char*	szEnd;
	int		i;

	szEnd = NULL;
	for (i = 1; i < argc; i++)
	{
		if ((strcmp(argv[i], DEBUG_ARGUMENT_SHARED_MEMORY) == 0) || (strcmp(argv[i], DEBUG_ARGUMENT_NETWORK) == 0))
		{
			i++;
		}
		else if (strcmp(argv[i], X65C64_EMULATOR_ARGUMENT_LOAD) == 0)
		{
			muiLoad = (i + 1 < argc) ? (uint32)strtoul(argv[i + 1], &szEnd, 0) : 0;
			if ((i + 1 >= argc) || (*szEnd != '\0') || (muiLoad >= mcMemory.GetLowSize()))
			{
				gcLogger.Error2(__METHOD__, " " X65C64_EMULATOR_ARGUMENT_LOAD " needs an address in the memory.", NULL);
				return false;
			}
			i++;
		}
		else if (strcmp(argv[i], X65C64_EMULATOR_ARGUMENT_HOST) == 0)
		{
			if (i + 1 >= argc)
			{
				gcLogger.Error2(__METHOD__, " " X65C64_EMULATOR_ARGUMENT_HOST " needs a directory.", NULL);
				return false;
			}
			i++;
			mcHostFiles.SetDirectory(argv[i]);
		}
		else if (strncmp(argv[i], "--", 2) != 0)
		{
			if (*pszProgram)
			{
				gcLogger.Error2(__METHOD__, " Only one program can be run [", argv[i], "].", NULL);
				return false;
			}
			*pszProgram = argv[i];
		}
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The program file's bytes at the load address.
//
//////////////////////////////////////////////////////////////////////////
bool CX65C64Emulator::LoadProgram(const char* szProgram)
{
	CFileUtil	cFileUtil;
	void*		pvData;
	size		uiSize;

	pvData = cFileUtil.ReadAll(szProgram, &uiSize);
	if (!pvData || (muiLoad + uiSize > mcMemory.GetLowSize()))
	{
		gcLogger.Error2(__METHOD__, " Could not load [", szProgram, "] into the memory.", NULL);
		free(pvData);
		return false;
	}
	memcpy(mcMemory.Byte(muiLoad), pvData, uiSize);
	free(pvData);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Runs the program until it stops (or, attached, until the debugger goes
// or quits it), its console shown as it writes.  Returns main's result.
//
//////////////////////////////////////////////////////////////////////////
int CX65C64Emulator::Run(void)
{
	for (;;)
	{
		if (mcDebugger.IsEnabled())
		{
			if (!mcDebugger.Tick(X65C64_EMULATOR_STEPS))
			{
				break;
			}
			if (!mcDebugger.IsRunning())
			{
				//Stopped by the debugger, or waiting for one: no time to spend.
				ThreadSleep(5);
			}
		}
		else
		{
			mcTarget.Run(X65C64_EMULATOR_STEPS);
		}
		ShowConsole();
		if (mcTarget.IsStopped() && !mcDebugger.IsAttached())
		{
			break;
		}
	}
	return (int)(int32)mcTarget.GetMPU()->GetA();
}


//////////////////////////////////////////////////////////////////////////
//
// What the program wrote to its console since, on standard output.
//
//////////////////////////////////////////////////////////////////////////
void CX65C64Emulator::ShowConsole(void)
{
	if (mszConsole.Length() > 0)
	{
		fwrite(mszConsole.Text(), 1, mszConsole.Length(), stdout);
		fflush(stdout);
		mszConsole.Clear();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The console's input: standard input's next byte, waited for, once what
// the program wrote before asking is shown (a prompt).  -1 at its end.
//
//////////////////////////////////////////////////////////////////////////
int CX65C64Emulator::ReadConsole(void* pvEmulator)
{
	int		iByte;

	((CX65C64Emulator*)pvEmulator)->ShowConsole();
	iByte = getchar();
	return (iByte == EOF) ? -1 : iByte;
}

