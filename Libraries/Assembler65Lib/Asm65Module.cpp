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
#include <string.h>
#include "BaseLib/MemoryFile.h"
#include "BaseLib/PointerFunctions.h"
#include "Asm65Object.h"
#include "Asm65Module.h"


//////////////////////////////////////////////////////////////////////////
//
// szEntry is the symbol the loader calls once the module is in place.
//
//////////////////////////////////////////////////////////////////////////
void CAsm65Module::Init(const char* szEntry)
{
	mszEntry.Init(szEntry);
	macBytes.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CAsm65Module::Kill(void)
{
	macBytes.Kill();
	mszEntry.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A global symbol at the linked address iValue, as an offset in the
// module's section, which starts at iBase.
//
//////////////////////////////////////////////////////////////////////////
static void Asm65ModuleSymbol(CAsm65Object* pcObject, const char* szName, int64 iValue, int64 iBase)
{
	SAsm65ObjectSymbol*	psSymbol;

	psSymbol = pcObject->GetSymbol(pcObject->PutSymbol(szName));
	psSymbol->iSection = 0;
	psSymbol->iValue = iValue - iBase;
	psSymbol->eBinding = AB65_Global;
}


//////////////////////////////////////////////////////////////////////////
//
// The module from a relocatable link (pcLinker, Linked with
// SetRelocatable).  False, with pszErrors saying why, if it can't be one.
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65Module::Make(CAsm65Output* pcOutput, CAsm65Linker* pcLinker, CChars* pszErrors)
{
	CAsm65Object		cObject;
	SAsm65Section*		psSection;
	SAsm65Segment*		psSegment;
	SAsm65ObjectSymbol*	psSymbol;
	SAsm65Relocation*	psRelocation;
	SAsm65Fixup*		psFixup;
	CMemoryFile*		pcMemoryFile;
	int64				iBase;
	int64				iEnd;
	int64				iEntry;
	int64				iInitStart;
	int64				iInitEnd;
	int64				iBytesEnd;
	uint32				uiImport;
	size				i;
	bool				bResult;

	macBytes.Kill();
	macBytes.Init();
	if (!pcLinker->GetSymbol("__text_start", &iBase) || !pcLinker->GetSymbol("__end", &iEnd) ||
		!pcLinker->GetSymbol(ASM65_MODULE_INIT_START, &iInitStart) || !pcLinker->GetSymbol(ASM65_MODULE_INIT_END, &iInitEnd))
	{
		pszErrors->Append("The link has no __text_start, __end, __init_start or __init_end.\n");
		return false;
	}
	if (!pcLinker->GetSymbol(mszEntry.Text(), &iEntry))
	{
		pszErrors->Append("No ")->Append(mszEntry)->Append(".\n");
		return false;
	}
	if (iEnd - iBase > ASM65_MODULE_MAX_SIZE)
	{
		pszErrors->Append("The module is more than a bank.\n");
		return false;
	}

	//The bytes from the text up to the last byte the link wrote (the end of its data); bss follows.
	iBytesEnd = iBase;
	for (i = 0; i < pcOutput->NumSegments(); i++)
	{
		psSegment = pcOutput->GetSegment(i);
		if ((psSegment->uiAddress < iBase) || (psSegment->uiAddress + (int64)psSegment->cBytes.NumElements() > iEnd))
		{
			pszErrors->Append("The link wrote outside the module.\n");
			return false;
		}
		if (psSegment->uiAddress + (int64)psSegment->cBytes.NumElements() > iBytesEnd)
		{
			iBytesEnd = psSegment->uiAddress + (int64)psSegment->cBytes.NumElements();
		}
	}

	cObject.Init("Module");
	psSection = cObject.GetSection(cObject.AddSection(ASM65_MODULE_SECTION, "", ASK65_Text));
	psSection->bKeep = true;
	psSection->uiAlignment = 4;
	psSection->uiSize = (uint32)(iEnd - iBase);
	psSection->cBytes.Resize((size)(iBytesEnd - iBase));
	memset(psSection->cBytes.GetData(), 0, (size)(iBytesEnd - iBase));
	for (i = 0; i < pcOutput->NumSegments(); i++)
	{
		psSegment = pcOutput->GetSegment(i);
		memcpy(psSection->cBytes.GetData() + (psSegment->uiAddress - iBase), psSegment->cBytes.GetData(), psSegment->cBytes.NumElements());
	}

	Asm65ModuleSymbol(&cObject, mszEntry.Text(), iEntry, iBase);
	Asm65ModuleSymbol(&cObject, ASM65_MODULE_INIT_START, iInitStart, iBase);
	Asm65ModuleSymbol(&cObject, ASM65_MODULE_INIT_END, iInitEnd, iBase);
	for (i = 0; i < pcLinker->NumMissing(); i++)
	{
		psSymbol = cObject.GetSymbol(cObject.PutSymbol(pcLinker->GetMissing(i)));
		psSymbol->eBinding = AB65_Global;
	}

	for (i = 0; i < pcLinker->NumFixups(); i++)
	{
		psFixup = pcLinker->GetFixup(i);
		psRelocation = cObject.AddRelocation();
		psRelocation->uiSection = 0;
		psRelocation->uiOffset = (uint32)(psFixup->uiAddress - iBase);
		if (psFixup->iImport >= 0)
		{
			uiImport = cObject.PutSymbol(pcLinker->GetMissing((size)psFixup->iImport));
			psRelocation->iTargetSection = ASM65_TARGET_SYMBOL;
			psRelocation->uiTargetSymbol = uiImport;
			psRelocation->iAddend = psFixup->iValue;
		}
		else
		{
			psRelocation->iTargetSection = 0;
			psRelocation->iAddend = psFixup->iValue - iBase;
		}
		psRelocation->uiBytes = psFixup->uiBytes;
		psRelocation->bSigned = psFixup->bSigned;
		psRelocation->bRelative = psFixup->bRelative;
		psRelocation->uiForm = psFixup->uiForm;
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
		pszErrors->Append("Could not write the module.\n");
	}
	SafeKill(pcMemoryFile);
	cObject.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The module Made, to szFilename.
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65Module::Write(const char* szFilename)
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


char* CAsm65Module::GetEntry(void)		{ return mszEntry.Text(); }
uint8* CAsm65Module::GetBytes(void)		{ return macBytes.GetData(); }
size CAsm65Module::GetSize(void)		{ return macBytes.NumElements(); }
