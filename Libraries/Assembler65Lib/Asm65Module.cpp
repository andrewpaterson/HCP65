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
#include "BaseLib/StringHelper.h"
#include "Asm65Attributes.h"
#include "Asm65Object.h"
#include "Asm65Module.h"


#define ASM65_MODULE_TAG			"Module"
#define ASM65_MODULE_FILE			"File"
#define ASM65_MODULE_ENTRY			"Entry"
#define ASM65_MODULE_LIMIT			"Limit"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CAsm65Module::Init(void)
{
	Init("", "", 0);
}


//////////////////////////////////////////////////////////////////////////
//
// The module szFile; szEntry is the symbol the loader calls once it is in
// place.
//
//////////////////////////////////////////////////////////////////////////
void CAsm65Module::Init(const char* szFile, const char* szEntry, uint32 uiLimit)
{
	mszFile.Init(szFile);
	mszEntry.Init(szEntry);
	muiLimit = uiLimit;
	muiBase = 0;
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
	mszFile.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CAsm65Module::SaveXML(CXMLWriter* pcWriter)
{
	pcWriter->Begin(ASM65_MODULE_TAG);
	pcWriter->Attribute(ASM65_MODULE_FILE, mszFile.Text());
	pcWriter->Attribute(ASM65_MODULE_ENTRY, mszEntry.Text());
	Asm65WriteHexAttribute(pcWriter, ASM65_MODULE_LIMIT, muiLimit);
	pcWriter->End();
}


//////////////////////////////////////////////////////////////////////////
//
// False if pcTag isn't a module with a file and an entry.  Limit
// (default none) may be left out.  Call it on a module just Init'ed.
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65Module::LoadXML(CMarkupTag* pcTag)
{
	char*	szFile;
	char*	szEntry;

	if (StringCompare(pcTag->GetName(), ASM65_MODULE_TAG) != 0)
	{
		return false;
	}
	szFile = pcTag->GetStringAttribute(ASM65_MODULE_FILE);
	szEntry = pcTag->GetStringAttribute(ASM65_MODULE_ENTRY);
	if (!szFile || (szFile[0] == '\0') || !szEntry || (szEntry[0] == '\0'))
	{
		return false;
	}
	mszFile.Set(szFile);
	mszEntry.Set(szEntry);

	muiLimit = 0;
	if (pcTag->GetStringAttribute(ASM65_MODULE_LIMIT))
	{
		return Asm65ReadUint32Attribute(pcTag, ASM65_MODULE_LIMIT, &muiLimit);
	}
	return true;
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
	muiBase = 0;
	if (!pcLinker->GetSymbol("__text_start", &iBase) || !pcLinker->GetSymbol("__end", &iEnd) ||
		!pcLinker->GetSymbol(ASM65_MODULE_INIT_START, &iInitStart) || !pcLinker->GetSymbol(ASM65_MODULE_INIT_END, &iInitEnd))
	{
		pszErrors->Append("The link has no __text_start, __end, __init_start or __init_end.\n");
		return false;
	}
	if (!pcLinker->GetSymbol(mszEntry.Text(), &iEntry))
	{
		pcLinker->DescribeMissing("The entry", mszEntry.Text(), pszErrors);
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

	bResult = cObject.WriteBytes(&macBytes);
	if (bResult)
	{
		muiBase = (uint32)iBase;
	}
	else
	{
		pszErrors->Append("Could not write the module.\n");
	}
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
	return Asm65WriteFile(szFilename, &macBytes);
}


char* CAsm65Module::GetFile(void)		{ return mszFile.Text(); }
char* CAsm65Module::GetEntry(void)		{ return mszEntry.Text(); }
uint32 CAsm65Module::GetLimit(void)		{ return muiLimit; }
uint32 CAsm65Module::GetBase(void)		{ return muiBase; }
uint8* CAsm65Module::GetBytes(void)		{ return macBytes.GetData(); }
size CAsm65Module::GetSize(void)		{ return macBytes.NumElements(); }
