/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela Assembler65Lib

Codaphela Assembler65Lib is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela Assembler65Lib is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with Codaphela Assembler65Lib.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#include <string.h>
#include "BaseLib/FileUtil.h"
#include "Asm65Source.h"


//////////////////////////////////////////////////////////////////////////
//
// The text must outlive the source.
//
//////////////////////////////////////////////////////////////////////////
void CAsm65TextSource::Init(const char* szText)
{
	mszText = szText;
	Rewind();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CAsm65TextSource::Kill(void)
{
	mszText = NULL;
	mszPos = NULL;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CAsm65TextSource::Rewind(void)
{
	mszPos = mszText;
	mbDone = false;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
const char* CAsm65TextSource::NextLine(void)
{
	const char*	szLine;
	const char*	szEnd;

	if (mbDone)
	{
		return NULL;
	}
	szLine = mszPos;
	szEnd = strchr(mszPos, '\n');
	if (szEnd)
	{
		mszPos = szEnd + 1;
	}
	else
	{
		mbDone = true;
	}
	return szLine;
}


//////////////////////////////////////////////////////////////////////////
//
// pcSource must outlive the include source.
//
//////////////////////////////////////////////////////////////////////////
void CAsm65IncludeSource::Init(CAsm65Source* pcSource, const char* szDirectory)
{
	mpcSource = pcSource;
	mszDirectory.Init(szDirectory);
	mszName.Init();
	muiDepth = 0;
	mauiLines[0] = 0;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CAsm65IncludeSource::Kill(void)
{
	while (muiDepth > 0)
	{
		Pop();
	}
	mszName.Kill();
	mszDirectory.Kill();
	mpcSource = NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The name GetFile gives for the source's own lines (its file's path).
//
//////////////////////////////////////////////////////////////////////////
void CAsm65IncludeSource::SetName(const char* szName)
{
	mszName.Kill();
	mszName.Init(szName);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CAsm65IncludeSource::Rewind(void)
{
	while (muiDepth > 0)
	{
		Pop();
	}
	mauiLines[0] = 0;
	mpcSource->Rewind();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
const char* CAsm65IncludeSource::NextLine(void)
{
	const char*	szLine;

	for (;;)
	{
		if (muiDepth > 0)
		{
			//An included file's empty last line (at its terminating zero) would end the assembler's pass.
			szLine = macSources[muiDepth - 1].NextLine();
			if ((szLine == NULL) || (*szLine == '\0'))
			{
				Pop();
				continue;
			}
		}
		else
		{
			szLine = mpcSource->NextLine();
			if (szLine == NULL)
			{
				return NULL;
			}
		}
		mauiLines[muiDepth]++;
		if (!Include(szLine))
		{
			return szLine;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// If szLine is .include "file" (and the file can be read), its lines are
// read next.
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65IncludeSource::Include(const char* szLine)
{
	CFileUtil	cFileUtil;
	CChars		szPath;
	CChars		szName;
	const char*	szEnd;
	bool		bRead;

	while ((*szLine == ' ') || (*szLine == '\t'))
	{
		szLine++;
	}
	if ((strncmp(szLine, ".include", 8) != 0) || ((szLine[8] != ' ') && (szLine[8] != '\t')))
	{
		return false;
	}
	szLine += 8;
	while ((*szLine == ' ') || (*szLine == '\t'))
	{
		szLine++;
	}
	if (*szLine != '"')
	{
		return false;
	}
	szLine++;
	for (szEnd = szLine; (*szEnd != '"') && (*szEnd != '\n') && (*szEnd != '\0'); szEnd++)
	{
	}
	if ((*szEnd != '"') || (muiDepth >= ASM65_MAX_INCLUDE_DEPTH))
	{
		return false;
	}

	szName.Init();
	szName.Append(szLine, (size)(szEnd - szLine));
	if (!mszDirectory.Empty() && !cFileUtil.IsAbsolutePath(szName.Text()))
	{
		szPath.Init(mszDirectory);
		cFileUtil.AppendToPath(&szPath, szName.Text());
	}
	else
	{
		szPath.Init(szName);
	}
	szName.Kill();

	macFiles[muiDepth].Init();
	bRead = macFiles[muiDepth].Read(szPath.Text());
	if (!bRead)
	{
		szPath.Kill();
		macFiles[muiDepth].Kill();
		return false;
	}
	macFiles[muiDepth].PassifyNewlines();
	macSources[muiDepth].Init(macFiles[muiDepth].Text());
	maszPaths[muiDepth].Init(szPath);
	szPath.Kill();
	muiDepth++;
	mauiLines[muiDepth] = 0;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CAsm65IncludeSource::Pop(void)
{
	muiDepth--;
	maszPaths[muiDepth].Kill();
	macSources[muiDepth].Kill();
	macFiles[muiDepth].Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The file of the line NextLine last gave: an included file's path, or the
// source's name (empty if SetName wasn't called).
//
//////////////////////////////////////////////////////////////////////////
char* CAsm65IncludeSource::GetFile(void)
{
	return (muiDepth > 0) ? maszPaths[muiDepth - 1].Text() : mszName.Text();
}


//////////////////////////////////////////////////////////////////////////
//
// The line (1 based) in its file of the line NextLine last gave.
//
//////////////////////////////////////////////////////////////////////////
uint32 CAsm65IncludeSource::GetLine(void)
{
	return mauiLines[muiDepth];
}
