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
#ifndef __X65C64_EMULATOR_H__
#define __X65C64_EMULATOR_H__
#include "BaseLib/Chars.h"
#include "Assembler65Lib/Asm65Memory.h"
#include "Assembler65Lib/Asm65HostFiles.h"
#include "EmulatorDebugLib/Flat65C64DebugTarget.h"
#include "EmulatorDebugLib/EmulatorDebugger.h"


#define X65C64_EMULATOR_PROGRAM_ADDRESS		0x10000		//Where a 65C64 program is linked (CPP65_PROGRAM_ADDRESS) and loaded.
#define X65C64_EMULATOR_STEPS				100000		//The instructions run between looks at the debugger and the console.
#define X65C64_EMULATOR_ARGUMENT_LOAD		"--load"	//Then an address: where the program file goes, and reset starts.
#define X65C64_EMULATOR_ARGUMENT_HOST		"--host"	//Then a directory: the host file device's (the current one if not given).


//A 65C64 with nothing but memory (CAsm65Memory: 2MB at 0000:0000 and the vector page), its console (00FF:0100, to
//standard output, and its input from standard input) and the host file device (00FF:0200, files in a directory of the host's): what the 65C64's tests
//run programs on, as a console program a debugger can start (EmulatorDebugLib's launch arguments).  The program is
//the file the command line names, loaded at --load's address (0001:0000 if not given), or what the debugger loads;
//reset starts it at that address.  It runs until it stops (stp), or, with a debugger attached, until the debugger
//goes; main's result (A) is the exit code.
//
//	X65C64Emulator [program.bin] [--load <address>] [--host <directory>] [--debug-shm <name> | --debug-tcp <port>]
//		[--debug-wait] [--exit-on-detach]
class CX65C64Emulator
{
protected:
	CAsm65Memory			mcMemory;
	CAsm65HostFiles			mcHostFiles;
	CChars					mszConsole;
	CFlat65C64DebugTarget	mcTarget;
	CEmulatorDebugger		mcDebugger;
	uint32					muiLoad;

public:
	bool	Init(int argc, char* argv[]);
	void	Kill(void);
	int		Run(void);

protected:
	bool	ParseArguments(int argc, char* argv[], const char** pszProgram);
	bool	LoadProgram(const char* szProgram);
	void	ShowConsole(void);

	static int	ReadConsole(void* pvEmulator);
};


#endif // __X65C64_EMULATOR_H__

