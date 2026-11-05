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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "BaseLib/StringHelper.h"
#include "BaseLib/MemoryFile.h"
#include "BaseLib/PointerFunctions.h"
#include "Asm65Attributes.h"
#include "Asm65Object.h"
#include "Asm65Executable.h"


#define ASM65_EXECUTABLE_TAG			"Executable"
#define ASM65_EXECUTABLE_FILE			"File"
#define ASM65_EXECUTABLE_ENTRY			"Entry"
#define ASM65_EXECUTABLE_LIMIT			"Limit"
#define ASM65_EXECUTABLE_SYMBOL_TAG		"Symbol"
#define ASM65_EXECUTABLE_SYMBOL_NAME	"Name"
#define ASM65_EXECUTABLE_SYMBOL_VALUE	"Value"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CAsm65Executable::Init(void)
{
	Init("", "", 0);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CAsm65Executable::Init(const char* szFile, const char* szEntry, uint32 uiLimit)
{
	mszFile.Init(szFile);
	mszEntry.Init(szEntry);
	muiLimit = uiLimit;
	maszSymbols.Init();
	maiValues.Init();
	macBytes.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CAsm65Executable::Kill(void)
{
	macBytes.Kill();
	maiValues.Kill();
	maszSymbols.Kill();
	mszEntry.Kill();
	mszFile.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// An absolute global the executable has, with its value.
//
//////////////////////////////////////////////////////////////////////////
void CAsm65Executable::AddSymbol(const char* szName, int64 iValue)
{
	maszSymbols.Add(szName);
	*maiValues.Add() = iValue;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CAsm65Executable::SaveXML(CXMLWriter* pcWriter)
{
	size	i;

	pcWriter->Begin(ASM65_EXECUTABLE_TAG);
	pcWriter->Attribute(ASM65_EXECUTABLE_FILE, mszFile.Text());
	pcWriter->Attribute(ASM65_EXECUTABLE_ENTRY, mszEntry.Text());
	Asm65WriteHexAttribute(pcWriter, ASM65_EXECUTABLE_LIMIT, muiLimit);
	for (i = 0; i < maszSymbols.NumElements(); i++)
	{
		pcWriter->Begin(ASM65_EXECUTABLE_SYMBOL_TAG);
		pcWriter->Attribute(ASM65_EXECUTABLE_SYMBOL_NAME, maszSymbols.GetText(i));
		Asm65WriteHexAttribute(pcWriter, ASM65_EXECUTABLE_SYMBOL_VALUE, *maiValues.Get(i));
		pcWriter->End();
	}
	pcWriter->End();
}


//////////////////////////////////////////////////////////////////////////
//
// False if pcTag isn't an executable with a file and an entry, or a
// symbol hasn't a name and a value.  Limit (default none) may be left
// out.  Call it on an executable just Init'ed.
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65Executable::LoadXML(CMarkupTag* pcTag)
{
	char*			szFile;
	char*			szEntry;
	char*			szName;
	int64			iValue;
	CMarkupTag*		pcSymbol;
	STagIterator	sIterator;

	if (StringCompare(pcTag->GetName(), ASM65_EXECUTABLE_TAG) != 0)
	{
		return false;
	}
	szFile = pcTag->GetStringAttribute(ASM65_EXECUTABLE_FILE);
	szEntry = pcTag->GetStringAttribute(ASM65_EXECUTABLE_ENTRY);
	if (!szFile || (szFile[0] == '\0') || !szEntry || (szEntry[0] == '\0'))
	{
		return false;
	}
	mszFile.Set(szFile);
	mszEntry.Set(szEntry);

	muiLimit = 0;
	if (pcTag->GetStringAttribute(ASM65_EXECUTABLE_LIMIT))
	{
		if (!Asm65ReadUint32Attribute(pcTag, ASM65_EXECUTABLE_LIMIT, &muiLimit))
		{
			return false;
		}
	}

	maszSymbols.Kill();
	maszSymbols.Init();
	maiValues.Kill();
	maiValues.Init();
	for (pcSymbol = pcTag->GetTag(ASM65_EXECUTABLE_SYMBOL_TAG, &sIterator); pcSymbol; pcSymbol = pcTag->GetNextTag(&sIterator))
	{
		szName = pcSymbol->GetStringAttribute(ASM65_EXECUTABLE_SYMBOL_NAME);
		if (!szName || (szName[0] == '\0') || !Asm65ReadNumberAttribute(pcSymbol, ASM65_EXECUTABLE_SYMBOL_VALUE, &iValue))
		{
			return false;
		}
		AddSymbol(szName, iValue);
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The executable of pcOutput's segments, with the entry and bss from
// pcLinker's symbols (GetBytes).  False, with why appended to pszErrors,
// if the entry isn't there or it can't be written.
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65Executable::Make(CAsm65Output* pcOutput, CAsm65Linker* pcLinker, CChars* pszErrors)
{
	CAsm65Object		cObject;
	SAsm65Segment*		psSegment;
	SAsm65Section*		psSection;
	SAsm65ObjectSymbol*	psSymbol;
	CMemoryFile*		pcMemoryFile;
	CChars				szName;
	int64				lliEntry;
	int64				lliBssStart;
	int64				lliBssEnd;
	uint32				uiSection;
	uint32				uiSymbol;
	size				i;
	bool				bResult;

	macBytes.Kill();
	macBytes.Init();
	if (!pcLinker->GetSymbol(mszEntry.Text(), &lliEntry))
	{
		pszErrors->Append("No ")->Append(mszEntry)->Append(".\n");
		return false;
	}

	cObject.Init("Executable");
	for (i = 0; i < pcOutput->NumSegments(); i++)
	{
		psSegment = pcOutput->GetSegment(i);
		szName.Init("segment");
		szName.Append((int)i);
		uiSection = cObject.AddSection(szName.Text(), "", ASK65_Data);
		szName.Kill();
		psSection = cObject.GetSection(uiSection);
		psSection->bAt = true;
		psSection->uiAt = psSegment->uiAddress;
		psSection->bKeep = true;
		psSection->uiSize = (uint32)psSegment->cBytes.NumElements();
		psSection->cBytes.Resize(psSegment->cBytes.NumElements());
		memcpy(psSection->cBytes.GetData(), psSegment->cBytes.GetData(), psSegment->cBytes.NumElements());
	}
	//bss has no bytes: a section of its size and no bytes, which the loader clears.
	if (pcLinker->GetSymbol("__bss_start", &lliBssStart) && pcLinker->GetSymbol("__bss_end", &lliBssEnd) && (lliBssEnd > lliBssStart))
	{
		uiSection = cObject.AddSection("bss", "", ASK65_Bss);
		psSection = cObject.GetSection(uiSection);
		psSection->bAt = true;
		psSection->uiAt = (uint32)lliBssStart;
		psSection->bKeep = true;
		psSection->uiSize = (uint32)(lliBssEnd - lliBssStart);
	}

	uiSymbol = cObject.PutSymbol(mszEntry.Text());
	psSymbol = cObject.GetSymbol(uiSymbol);
	psSymbol->iSection = -1;
	psSymbol->iValue = lliEntry;
	psSymbol->eBinding = AB65_Global;

	for (i = 0; i < maszSymbols.NumElements(); i++)
	{
		uiSymbol = cObject.PutSymbol(maszSymbols.GetText(i));
		psSymbol = cObject.GetSymbol(uiSymbol);
		psSymbol->iSection = -1;
		psSymbol->iValue = *maiValues.Get(i);
		psSymbol->eBinding = AB65_Global;
	}

	pcMemoryFile = MemoryFile();
	pcMemoryFile->mbBasicFileMustFree = false;
	bResult = cObject.Write(pcMemoryFile);
	if (bResult)
	{
		macBytes.Resize((size)pcMemoryFile->GetBufferSize());
		memcpy(macBytes.GetData(), pcMemoryFile->GetBufferPointer(), (size)pcMemoryFile->GetBufferSize());
	}
	else
	{
		pszErrors->Append("Could not write the executable.\n");
	}
	SafeKill(pcMemoryFile);
	cObject.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The executable Made, to szFilename.
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65Executable::Write(const char* szFilename)
{
	FILE*	pcFile;
	bool	bResult;

	if (macBytes.NumElements() == 0)
	{
		return false;
	}
	pcFile = fopen(szFilename, "wb");
	if (!pcFile)
	{
		return false;
	}
	bResult = fwrite(macBytes.GetData(), 1, macBytes.NumElements(), pcFile) == macBytes.NumElements();
	bResult &= fclose(pcFile) == 0;
	return bResult;
}


char* CAsm65Executable::GetFile(void)						{ return mszFile.Text(); }
char* CAsm65Executable::GetEntry(void)						{ return mszEntry.Text(); }
uint32 CAsm65Executable::GetLimit(void)						{ return muiLimit; }
size CAsm65Executable::NumSymbols(void)						{ return maszSymbols.NumElements(); }
char* CAsm65Executable::GetSymbolName(size uiIndex)			{ return maszSymbols.GetText(uiIndex); }
int64 CAsm65Executable::GetSymbolValue(size uiIndex)		{ return *maiValues.Get(uiIndex); }
uint8* CAsm65Executable::GetBytes(void)						{ return macBytes.GetData(); }
size CAsm65Executable::GetSize(void)						{ return macBytes.NumElements(); }

