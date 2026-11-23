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
#include "BaseLib/DiskFile.h"
#include "BaseLib/FileUtil.h"
#include "Asm65DebugInfo.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CAsm65DebugInfo::Init(void)
{
	mcLines.Init();
	mcRecords.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CAsm65DebugInfo::Kill(void)
{
	mcRecords.Kill();
	mcLines.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CAsm65DebugInfo::Clear(void)
{
	mcLines.Clear();
	mcRecords.Clear();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
CAsm65LineTable* CAsm65DebugInfo::GetLines(void)
{
	return &mcLines;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
CAsm65DebugRecords* CAsm65DebugInfo::GetRecords(void)
{
	return &mcRecords;
}


//////////////////////////////////////////////////////////////////////////
//
// Every address moved by iOffset: a module's from its link's addresses to
// its own (or to where it was loaded).
//
//////////////////////////////////////////////////////////////////////////
void CAsm65DebugInfo::Move(int64 iOffset)
{
	mcLines.Move(iOffset);
	mcRecords.Move(iOffset);
}


//////////////////////////////////////////////////////////////////////////
//
// Writes it as a chunk file.  pcFile is freed by the chunk file when it
// is a must free file (DiskFile, MemoryFile).
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65DebugInfo::Write(CAbstractFile* pcFile)
{
	CChunkFile	cChunkFile;
	bool		bResult;

	cChunkFile.Init(pcFile);
	bResult = cChunkFile.WriteOpen();
	if (bResult)
	{
		bResult = mcLines.WriteChunks(&cChunkFile);
		bResult &= mcRecords.WriteChunks(&cChunkFile);
		bResult &= cChunkFile.WriteClose();
	}
	cChunkFile.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Reads what Write wrote into initialised, empty debug info.
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65DebugInfo::Read(CAbstractFile* pcFile)
{
	CChunkFile	cChunkFile;
	bool		bResult;

	//ReadClose frees what a failed ReadOpen allocated too.
	cChunkFile.Init(pcFile);
	bResult = cChunkFile.ReadOpen();
	bResult = bResult && mcLines.ReadChunks(&cChunkFile) && mcRecords.ReadChunks(&cChunkFile);
	bResult &= cChunkFile.ReadClose();
	cChunkFile.Kill();
	if (bResult)
	{
		mcRecords.Sort();
	}
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65DebugInfo::WriteFile(const char* szFilename)
{
	return Write(DiskFile(szFilename));
}


//////////////////////////////////////////////////////////////////////////
//
// False, with it emptied, if the file can't be read or isn't debug info
// of this version.
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65DebugInfo::ReadFile(const char* szFilename)
{
	CFileUtil	cFileUtil;

	Clear();
	if (!cFileUtil.Exists(szFilename))
	{
		return false;
	}
	if (!Read(DiskFile(szFilename)))
	{
		Clear();
		return false;
	}
	return true;
}
