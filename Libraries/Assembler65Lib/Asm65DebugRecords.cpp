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
#include "Asm65Object.h"
#include "Asm65DebugRecords.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CAsm65DebugRecords::Init(void)
{
	masFunctions.Init();
	masVariables.Init();
	masGlobals.Init();
	masTypes.Init();
	masMembers.Init();
	mcTypeKeys.Init();
	mauiFunctionOrder.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CAsm65DebugRecords::Kill(void)
{
	size	ui;

	for (ui = 0; ui < masFunctions.NumElements(); ui++)
	{
		masFunctions.Get(ui)->szName.Kill();
	}
	for (ui = 0; ui < masVariables.NumElements(); ui++)
	{
		masVariables.Get(ui)->szName.Kill();
	}
	for (ui = 0; ui < masGlobals.NumElements(); ui++)
	{
		masGlobals.Get(ui)->szName.Kill();
	}
	for (ui = 0; ui < masTypes.NumElements(); ui++)
	{
		masTypes.Get(ui)->szKey.Kill();
		masTypes.Get(ui)->szName.Kill();
	}
	for (ui = 0; ui < masMembers.NumElements(); ui++)
	{
		masMembers.Get(ui)->szName.Kill();
	}
	mauiFunctionOrder.Kill();
	mcTypeKeys.Kill();
	masMembers.Kill();
	masTypes.Kill();
	masGlobals.Kill();
	masVariables.Kill();
	masFunctions.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CAsm65DebugRecords::Clear(void)
{
	Kill();
	Init();
}


//////////////////////////////////////////////////////////////////////////
//
// A function starting at the place, got to as eEntry says, with no frame
// (its body is all of it) until SetFrame, running to its section's end
// unless EndFunction ends it first.  Its variables are the ones added
// after it.
//
//////////////////////////////////////////////////////////////////////////
SAsm65DebugFunction* CAsm65DebugRecords::AddFunction(const char* szName, uint32 uiSection, uint32 uiOffset, EAsm65DebugEntry eEntry)
{
	SAsm65DebugFunction*	psFunction;

	psFunction = masFunctions.Add();
	psFunction->szName.Init(szName);
	psFunction->sStart.uiSection = uiSection;
	psFunction->sStart.uiOffset = uiOffset;
	psFunction->uiEnd = ASM65_DEBUG_NONE;
	psFunction->uiBody = uiOffset;
	psFunction->uiEntry = (uint8)eEntry;
	psFunction->uiLink = ADL_None;
	psFunction->iLink = 0;
	psFunction->uiFirstVariable = (uint32)masVariables.NumElements();
	psFunction->uiNumVariables = 0;
	return psFunction;
}


//////////////////////////////////////////////////////////////////////////
//
// The last function's body starts at uiBody (its section's offset): its
// frame is made, as eLink and iLink say.
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65DebugRecords::SetFrame(uint32 uiBody, EAsm65DebugLink eLink, int32 iLink)
{
	SAsm65DebugFunction*	psFunction;

	psFunction = LastFunction();
	if (!psFunction)
	{
		return false;
	}
	psFunction->uiBody = uiBody;
	psFunction->uiLink = (uint8)eLink;
	psFunction->iLink = iLink;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The last function ends at uiEnd (its section's offset), before code
// the records don't describe.  False with no function, or one ended.
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65DebugRecords::EndFunction(uint32 uiEnd)
{
	SAsm65DebugFunction*	psFunction;

	psFunction = LastFunction();
	if (!psFunction || (psFunction->uiEnd != ASM65_DEBUG_NONE))
	{
		return false;
	}
	psFunction->uiEnd = uiEnd;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The functions in uiSection not ended (EndFunction) end at uiEnd, the
// section's size.  A function found by its address (FindFunction) is the
// last to start at or before it, so one section may hold several.
//
//////////////////////////////////////////////////////////////////////////
void CAsm65DebugRecords::EndFunctions(uint32 uiSection, uint32 uiEnd)
{
	size					ui;
	SAsm65DebugFunction*	psFunction;

	for (ui = 0; ui < masFunctions.NumElements(); ui++)
	{
		psFunction = masFunctions.Get(ui);
		if ((psFunction->sStart.uiSection == uiSection) && (psFunction->uiEnd == ASM65_DEBUG_NONE))
		{
			psFunction->uiEnd = uiEnd;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A variable of the last function.
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65DebugRecords::AddVariable(const char* szName, uint32 uiType, EAsm65DebugWhere eWhere, bool bParameter, int32 iOffset, uint32 uiFirstLine, uint32 uiLastLine)
{
	SAsm65DebugFunction*	psFunction;
	SAsm65DebugVariable*	psVariable;

	psFunction = LastFunction();
	if (!psFunction || (uiFirstLine > uiLastLine))
	{
		return false;
	}
	psVariable = masVariables.Add();
	psVariable->szName.Init(szName);
	psVariable->uiType = uiType;
	psVariable->uiWhere = (uint8)eWhere;
	psVariable->bParameter = bParameter;
	psVariable->iOffset = iOffset;
	psVariable->uiFirstLine = uiFirstLine;
	psVariable->uiLastLine = uiLastLine;
	psFunction->uiNumVariables++;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// A global at the place; bStatic, a static local of the last function.
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65DebugRecords::AddGlobal(const char* szName, uint32 uiType, uint32 uiSection, uint32 uiOffset, bool bStatic)
{
	SAsm65DebugGlobal*	psGlobal;

	if (bStatic && (masFunctions.NumElements() == 0))
	{
		return false;
	}
	psGlobal = masGlobals.Add();
	psGlobal->szName.Init(szName);
	psGlobal->uiType = uiType;
	psGlobal->sPlace.uiSection = uiSection;
	psGlobal->sPlace.uiOffset = uiOffset;
	psGlobal->uiFunction = bStatic ? (uint32)(masFunctions.NumElements() - 1) : ASM65_DEBUG_NONE;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The next type (its index is the number of types before it).  Its
// members are the ones added after it.  False for a key already added.
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65DebugRecords::AddType(const char* szKey, const char* szName, EAsm65DebugType eKind, uint8 uiFlags, uint32 uiBytes, uint32 uiTarget, uint32 uiCount)
{
	SAsm65DebugType*	psType;
	uint32				uiIndex;

	if (mcTypeKeys.HasKey(szKey))
	{
		return false;
	}
	uiIndex = (uint32)masTypes.NumElements();
	psType = masTypes.Add();
	psType->szKey.Init(szKey);
	psType->szName.Init(szName);
	psType->uiKind = (uint8)eKind;
	psType->uiFlags = uiFlags;
	psType->uiBytes = uiBytes;
	psType->uiTarget = uiTarget;
	psType->uiCount = uiCount;
	psType->uiFirstMember = (uint32)masMembers.NumElements();
	psType->uiNumMembers = 0;
	mcTypeKeys.Put(szKey, &uiIndex);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// A member of the last type.
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65DebugRecords::AddMember(const char* szName, EAsm65DebugMember eKind, uint32 uiType, int64 iValue, uint8 uiBitOffset, uint8 uiBits)
{
	SAsm65DebugType*	psType;
	SAsm65DebugMember*	psMember;

	psType = LastType();
	if (!psType)
	{
		return false;
	}
	psMember = masMembers.Add();
	psMember->szName.Init(szName);
	psMember->uiKind = (uint8)eKind;
	psMember->uiType = uiType;
	psMember->iValue = iValue;
	psMember->uiBitOffset = uiBitOffset;
	psMember->uiBits = uiBits;
	psType->uiNumMembers++;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether every type named is one of the types (a unit's may be named
// before they are added, so this is asked once they all are).
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65DebugRecords::IsValid(void)
{
	size	ui;

	for (ui = 0; ui < masVariables.NumElements(); ui++)
	{
		if (!IsTypeOrNone(masVariables.Get(ui)->uiType))
		{
			return false;
		}
	}
	for (ui = 0; ui < masGlobals.NumElements(); ui++)
	{
		if (!IsTypeOrNone(masGlobals.Get(ui)->uiType))
		{
			return false;
		}
	}
	for (ui = 0; ui < masTypes.NumElements(); ui++)
	{
		if (!IsTypeOrNone(masTypes.Get(ui)->uiTarget))
		{
			return false;
		}
	}
	for (ui = 0; ui < masMembers.NumElements(); ui++)
	{
		if (!IsTypeOrNone(masMembers.Get(ui)->uiType))
		{
			return false;
		}
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65DebugRecords::IsTypeOrNone(uint32 uiType)
{
	return (uiType == ASM65_DEBUG_NONE) || (uiType < masTypes.NumElements());
}


//////////////////////////////////////////////////////////////////////////
//
// pcSource's records (an object's) at their addresses:
// pauiSectionAddresses has each of its sections' address, ASM65_DEBUG_NONE
// for one not placed, whose functions and globals are left out.  A type
// is added unless one of its key already is; an incomplete one is
// replaced by a complete one.
//
//////////////////////////////////////////////////////////////////////////
void CAsm65DebugRecords::Append(CAsm65DebugRecords* pcSource, CArrayUint32* pauiSectionAddresses)
{
	CArrayUint32			auiTypes;
	CArrayUint32			auiFunctions;
	CArrayTemplate<bool>	abDescribe;
	size					ui;
	size					uiMember;
	uint32*					puiIndex;
	uint32					uiIndex;
	bool					bDescribe;
	SAsm65DebugType*		psSourceType;
	SAsm65DebugType*		psType;
	SAsm65DebugMember*		psSourceMember;
	SAsm65DebugFunction*	psSourceFunction;
	SAsm65DebugFunction*	psFunction;
	SAsm65DebugVariable*	psSourceVariable;
	SAsm65DebugGlobal*		psSourceGlobal;
	uint32					uiAddress;

	//Each of the source's types is one of these, by its key: an existing one, or one added to be described next.
	auiTypes.Init();
	abDescribe.Init();
	for (ui = 0; ui < pcSource->masTypes.NumElements(); ui++)
	{
		psSourceType = pcSource->masTypes.Get(ui);
		puiIndex = mcTypeKeys.Get(psSourceType->szKey.Text());
		if (puiIndex)
		{
			uiIndex = *puiIndex;
			bDescribe = (masTypes.Get(uiIndex)->uiFlags & ADTF_Incomplete) && !(psSourceType->uiFlags & ADTF_Incomplete);
		}
		else
		{
			uiIndex = (uint32)masTypes.NumElements();
			psType = masTypes.Add();
			psType->szKey.Init(psSourceType->szKey);
			psType->szName.Init();
			mcTypeKeys.Put(psSourceType->szKey.Text(), &uiIndex);
			bDescribe = true;
		}
		auiTypes.Add(uiIndex);
		abDescribe.Add(&bDescribe);
	}
	for (ui = 0; ui < pcSource->masTypes.NumElements(); ui++)
	{
		if (!*abDescribe.Get(ui))
		{
			continue;
		}
		psSourceType = pcSource->masTypes.Get(ui);
		psType = masTypes.Get(auiTypes.GetValue(ui));
		psType->szName.Set(psSourceType->szName);
		psType->uiKind = psSourceType->uiKind;
		psType->uiFlags = psSourceType->uiFlags;
		psType->uiBytes = psSourceType->uiBytes;
		psType->uiTarget = MapType(psSourceType->uiTarget, &auiTypes);
		psType->uiCount = psSourceType->uiCount;
		psType->uiFirstMember = (uint32)masMembers.NumElements();
		psType->uiNumMembers = psSourceType->uiNumMembers;
		for (uiMember = 0; uiMember < psSourceType->uiNumMembers; uiMember++)
		{
			psSourceMember = pcSource->GetMember(psSourceType, uiMember);
			AddMemberTo(psSourceMember, MapType(psSourceMember->uiType, &auiTypes));
		}
	}

	//The placed functions, each with its variables; the source's function indices mapped for its static locals.
	auiFunctions.Init();
	for (ui = 0; ui < pcSource->masFunctions.NumElements(); ui++)
	{
		psSourceFunction = pcSource->masFunctions.Get(ui);
		uiAddress = SectionAddress(psSourceFunction->sStart.uiSection, pauiSectionAddresses);
		if (uiAddress == ASM65_DEBUG_NONE)
		{
			auiFunctions.Add(ASM65_DEBUG_NONE);
			continue;
		}
		auiFunctions.Add((uint32)masFunctions.NumElements());
		psFunction = AddFunction(psSourceFunction->szName.Text(), ASM65_DEBUG_ABSOLUTE, uiAddress + psSourceFunction->sStart.uiOffset, (EAsm65DebugEntry)psSourceFunction->uiEntry);
		psFunction->uiEnd = uiAddress + psSourceFunction->uiEnd;
		psFunction->uiBody = uiAddress + psSourceFunction->uiBody;
		psFunction->uiLink = psSourceFunction->uiLink;
		psFunction->iLink = psSourceFunction->iLink;
		for (uiMember = 0; uiMember < psSourceFunction->uiNumVariables; uiMember++)
		{
			psSourceVariable = pcSource->GetVariable(psSourceFunction, uiMember);
			AddVariable(psSourceVariable->szName.Text(), MapType(psSourceVariable->uiType, &auiTypes), (EAsm65DebugWhere)psSourceVariable->uiWhere, psSourceVariable->bParameter,
						psSourceVariable->iOffset, psSourceVariable->uiFirstLine, psSourceVariable->uiLastLine);
		}
	}

	for (ui = 0; ui < pcSource->masGlobals.NumElements(); ui++)
	{
		psSourceGlobal = pcSource->masGlobals.Get(ui);
		uiAddress = SectionAddress(psSourceGlobal->sPlace.uiSection, pauiSectionAddresses);
		uiIndex = (psSourceGlobal->uiFunction == ASM65_DEBUG_NONE) ? ASM65_DEBUG_NONE : auiFunctions.GetValue(psSourceGlobal->uiFunction);
		if ((uiAddress == ASM65_DEBUG_NONE) || ((psSourceGlobal->uiFunction != ASM65_DEBUG_NONE) && (uiIndex == ASM65_DEBUG_NONE)))
		{
			continue;
		}
		AddGlobalTo(psSourceGlobal->szName.Text(), MapType(psSourceGlobal->uiType, &auiTypes), uiAddress + psSourceGlobal->sPlace.uiOffset, uiIndex);
	}

	auiFunctions.Kill();
	abDescribe.Kill();
	auiTypes.Kill();
	mauiFunctionOrder.ReInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint32 CAsm65DebugRecords::MapType(uint32 uiType, CArrayUint32* pauiTypes)
{
	return (uiType < pauiTypes->NumElements()) ? pauiTypes->GetValue(uiType) : ASM65_DEBUG_NONE;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint32 CAsm65DebugRecords::SectionAddress(uint32 uiSection, CArrayUint32* pauiSectionAddresses)
{
	return (uiSection < pauiSectionAddresses->NumElements()) ? pauiSectionAddresses->GetValue(uiSection) : ASM65_DEBUG_NONE;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CAsm65DebugRecords::AddMemberTo(SAsm65DebugMember* psSource, uint32 uiType)
{
	SAsm65DebugMember*	psMember;

	psMember = masMembers.Add();
	psMember->szName.Init(psSource->szName);
	psMember->uiKind = psSource->uiKind;
	psMember->uiType = uiType;
	psMember->iValue = psSource->iValue;
	psMember->uiBitOffset = psSource->uiBitOffset;
	psMember->uiBits = psSource->uiBits;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CAsm65DebugRecords::AddGlobalTo(const char* szName, uint32 uiType, uint32 uiAddress, uint32 uiFunction)
{
	SAsm65DebugGlobal*	psGlobal;

	psGlobal = masGlobals.Add();
	psGlobal->szName.Init(szName);
	psGlobal->uiType = uiType;
	psGlobal->sPlace.uiSection = ASM65_DEBUG_ABSOLUTE;
	psGlobal->sPlace.uiOffset = uiAddress;
	psGlobal->uiFunction = uiFunction;
}


//////////////////////////////////////////////////////////////////////////
//
// The functions in address order, for FindFunction.  Their indices don't
// change (variables and static locals name them).
//
//////////////////////////////////////////////////////////////////////////
void CAsm65DebugRecords::Sort(void)
{
	size	ui;
	size	uiInsert;
	uint32	uiStart;

	mauiFunctionOrder.ReInit();
	for (ui = 0; ui < masFunctions.NumElements(); ui++)
	{
		uiStart = masFunctions.Get(ui)->sStart.uiOffset;
		for (uiInsert = mauiFunctionOrder.NumElements(); (uiInsert > 0) && (masFunctions.Get(mauiFunctionOrder.GetValue(uiInsert - 1))->sStart.uiOffset > uiStart); uiInsert--)
		{
		}
		mauiFunctionOrder.InsertAt((uint32)ui, uiInsert);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Every address moved by iOffset: a module's records from its link's
// addresses to where it was loaded.
//
//////////////////////////////////////////////////////////////////////////
void CAsm65DebugRecords::Move(int64 iOffset)
{
	size					ui;
	SAsm65DebugFunction*	psFunction;
	SAsm65DebugGlobal*		psGlobal;

	for (ui = 0; ui < masFunctions.NumElements(); ui++)
	{
		psFunction = masFunctions.Get(ui);
		psFunction->sStart.uiOffset = (uint32)(psFunction->sStart.uiOffset + iOffset);
		psFunction->uiEnd = (uint32)(psFunction->uiEnd + iOffset);
		psFunction->uiBody = (uint32)(psFunction->uiBody + iOffset);
	}
	for (ui = 0; ui < masGlobals.NumElements(); ui++)
	{
		psGlobal = masGlobals.Get(ui);
		psGlobal->sPlace.uiOffset = (uint32)(psGlobal->sPlace.uiOffset + iOffset);
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
size CAsm65DebugRecords::NumFunctions(void) const
{
	return masFunctions.NumElements();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
SAsm65DebugFunction* CAsm65DebugRecords::GetFunction(size uiIndex)
{
	return masFunctions.SafeGet(uiIndex);
}


//////////////////////////////////////////////////////////////////////////
//
// The function whose code holds uiAddress, NULL if none does (once
// linked, after Sort).
//
//////////////////////////////////////////////////////////////////////////
SAsm65DebugFunction* CAsm65DebugRecords::FindFunction(uint32 uiAddress)
{
	size					uiLow;
	size					uiHigh;
	size					uiMiddle;
	SAsm65DebugFunction*	psFunction;

	if (mauiFunctionOrder.NumElements() != masFunctions.NumElements())
	{
		Sort();
	}
	uiLow = 0;
	uiHigh = mauiFunctionOrder.NumElements();
	while (uiLow < uiHigh)
	{
		uiMiddle = (uiLow + uiHigh) / 2;
		if (masFunctions.Get(mauiFunctionOrder.GetValue(uiMiddle))->sStart.uiOffset <= uiAddress)
		{
			uiLow = uiMiddle + 1;
		}
		else
		{
			uiHigh = uiMiddle;
		}
	}
	if (uiLow == 0)
	{
		return NULL;
	}
	psFunction = masFunctions.Get(mauiFunctionOrder.GetValue(uiLow - 1));
	return (uiAddress < psFunction->uiEnd) ? psFunction : NULL;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
size CAsm65DebugRecords::NumVariables(void) const
{
	return masVariables.NumElements();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
SAsm65DebugVariable* CAsm65DebugRecords::GetVariable(size uiIndex)
{
	return masVariables.SafeGet(uiIndex);
}


//////////////////////////////////////////////////////////////////////////
//
// The function's uiIndex'th variable.
//
//////////////////////////////////////////////////////////////////////////
SAsm65DebugVariable* CAsm65DebugRecords::GetVariable(SAsm65DebugFunction* psFunction, size uiIndex)
{
	return (uiIndex < psFunction->uiNumVariables) ? masVariables.SafeGet(psFunction->uiFirstVariable + uiIndex) : NULL;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
size CAsm65DebugRecords::NumGlobals(void) const
{
	return masGlobals.NumElements();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
SAsm65DebugGlobal* CAsm65DebugRecords::GetGlobal(size uiIndex)
{
	return masGlobals.SafeGet(uiIndex);
}


//////////////////////////////////////////////////////////////////////////
//
// The global named szName that code in psFunction sees: its static local
// of that name first, else a global (NULL psFunction: globals only).
//
//////////////////////////////////////////////////////////////////////////
SAsm65DebugGlobal* CAsm65DebugRecords::FindGlobal(const char* szName, SAsm65DebugFunction* psFunction)
{
	size				ui;
	uint32				uiFunction;
	SAsm65DebugGlobal*	psGlobal;
	SAsm65DebugGlobal*	psFound;

	uiFunction = psFunction ? (uint32)masFunctions.GetIndex(psFunction) : ASM65_DEBUG_NONE;
	psFound = NULL;
	for (ui = 0; ui < masGlobals.NumElements(); ui++)
	{
		psGlobal = masGlobals.Get(ui);
		if (!psGlobal->szName.Equals(szName))
		{
			continue;
		}
		if ((psGlobal->uiFunction != ASM65_DEBUG_NONE) && (psGlobal->uiFunction == uiFunction))
		{
			return psGlobal;
		}
		if ((psGlobal->uiFunction == ASM65_DEBUG_NONE) && !psFound)
		{
			psFound = psGlobal;
		}
	}
	return psFound;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
size CAsm65DebugRecords::NumTypes(void) const
{
	return masTypes.NumElements();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
SAsm65DebugType* CAsm65DebugRecords::GetType(uint32 uiIndex)
{
	return (uiIndex == ASM65_DEBUG_NONE) ? NULL : masTypes.SafeGet(uiIndex);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
SAsm65DebugType* CAsm65DebugRecords::FindType(const char* szKey)
{
	uint32*	puiIndex;

	puiIndex = mcTypeKeys.Get(szKey);
	return puiIndex ? masTypes.Get(*puiIndex) : NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The type's uiIndex'th member.
//
//////////////////////////////////////////////////////////////////////////
SAsm65DebugMember* CAsm65DebugRecords::GetMember(SAsm65DebugType* psType, size uiIndex)
{
	return (uiIndex < psType->uiNumMembers) ? masMembers.SafeGet(psType->uiFirstMember + uiIndex) : NULL;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
SAsm65DebugFunction* CAsm65DebugRecords::LastFunction(void)
{
	return masFunctions.Tail();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
SAsm65DebugType* CAsm65DebugRecords::LastType(void)
{
	return masTypes.Tail();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
static bool Asm65WritePlace(CChunkFile* pcChunkFile, SAsm65DebugPlace* psPlace)
{
	return pcChunkFile->WriteInt32(psPlace->uiSection) && pcChunkFile->WriteInt32(psPlace->uiOffset);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
static bool Asm65ReadPlace(CChunkFile* pcChunkFile, SAsm65DebugPlace* psPlace)
{
	return pcChunkFile->ReadInt32(&psPlace->uiSection) && pcChunkFile->ReadInt32(&psPlace->uiOffset);
}


//////////////////////////////////////////////////////////////////////////
//
// The records as chunks, after a chunk file's other chunks (an object's,
// a debug file's line table).
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65DebugRecords::WriteChunks(CChunkFile* pcChunkFile)
{
	size					ui;
	SAsm65DebugFunction*	psFunction;
	SAsm65DebugVariable*	psVariable;
	SAsm65DebugGlobal*		psGlobal;
	SAsm65DebugType*		psType;
	SAsm65DebugMember*		psMember;
	bool					bResult;

	bResult = pcChunkFile->WriteChunkBegin();
	bResult &= Asm65WriteMagic(pcChunkFile, ASM65_DEBUG_RECORDS_MAGIC, ASM65_DEBUG_RECORDS_VERSION);
	bResult &= pcChunkFile->WriteChunkEnd("DebugHeader");

	bResult &= pcChunkFile->WriteChunkBegin();
	bResult &= pcChunkFile->WriteInt32((uint32)masFunctions.NumElements());
	for (ui = 0; ui < masFunctions.NumElements(); ui++)
	{
		psFunction = masFunctions.Get(ui);
		bResult &= Asm65WriteText(pcChunkFile, psFunction->szName.Text());
		bResult &= Asm65WritePlace(pcChunkFile, &psFunction->sStart);
		bResult &= pcChunkFile->WriteInt32(psFunction->uiEnd);
		bResult &= pcChunkFile->WriteInt32(psFunction->uiBody);
		bResult &= pcChunkFile->WriteInt8(psFunction->uiEntry);
		bResult &= pcChunkFile->WriteInt8(psFunction->uiLink);
		bResult &= pcChunkFile->WriteInt32(psFunction->iLink);
		bResult &= pcChunkFile->WriteInt32(psFunction->uiFirstVariable);
		bResult &= pcChunkFile->WriteInt32(psFunction->uiNumVariables);
	}
	bResult &= pcChunkFile->WriteChunkEnd("Functions");

	bResult &= pcChunkFile->WriteChunkBegin();
	bResult &= pcChunkFile->WriteInt32((uint32)masVariables.NumElements());
	for (ui = 0; ui < masVariables.NumElements(); ui++)
	{
		psVariable = masVariables.Get(ui);
		bResult &= Asm65WriteText(pcChunkFile, psVariable->szName.Text());
		bResult &= pcChunkFile->WriteInt32(psVariable->uiType);
		bResult &= pcChunkFile->WriteInt8(psVariable->uiWhere);
		bResult &= pcChunkFile->WriteBool(psVariable->bParameter);
		bResult &= pcChunkFile->WriteInt32(psVariable->iOffset);
		bResult &= pcChunkFile->WriteInt32(psVariable->uiFirstLine);
		bResult &= pcChunkFile->WriteInt32(psVariable->uiLastLine);
	}
	bResult &= pcChunkFile->WriteChunkEnd("Variables");

	bResult &= pcChunkFile->WriteChunkBegin();
	bResult &= pcChunkFile->WriteInt32((uint32)masGlobals.NumElements());
	for (ui = 0; ui < masGlobals.NumElements(); ui++)
	{
		psGlobal = masGlobals.Get(ui);
		bResult &= Asm65WriteText(pcChunkFile, psGlobal->szName.Text());
		bResult &= pcChunkFile->WriteInt32(psGlobal->uiType);
		bResult &= Asm65WritePlace(pcChunkFile, &psGlobal->sPlace);
		bResult &= pcChunkFile->WriteInt32(psGlobal->uiFunction);
	}
	bResult &= pcChunkFile->WriteChunkEnd("Globals");

	bResult &= pcChunkFile->WriteChunkBegin();
	bResult &= pcChunkFile->WriteInt32((uint32)masTypes.NumElements());
	for (ui = 0; ui < masTypes.NumElements(); ui++)
	{
		psType = masTypes.Get(ui);
		bResult &= Asm65WriteText(pcChunkFile, psType->szKey.Text());
		bResult &= Asm65WriteText(pcChunkFile, psType->szName.Text());
		bResult &= pcChunkFile->WriteInt8(psType->uiKind);
		bResult &= pcChunkFile->WriteInt8(psType->uiFlags);
		bResult &= pcChunkFile->WriteInt32(psType->uiBytes);
		bResult &= pcChunkFile->WriteInt32(psType->uiTarget);
		bResult &= pcChunkFile->WriteInt32(psType->uiCount);
		bResult &= pcChunkFile->WriteInt32(psType->uiFirstMember);
		bResult &= pcChunkFile->WriteInt32(psType->uiNumMembers);
	}
	bResult &= pcChunkFile->WriteChunkEnd("Types");

	bResult &= pcChunkFile->WriteChunkBegin();
	bResult &= pcChunkFile->WriteInt32((uint32)masMembers.NumElements());
	for (ui = 0; ui < masMembers.NumElements(); ui++)
	{
		psMember = masMembers.Get(ui);
		bResult &= Asm65WriteText(pcChunkFile, psMember->szName.Text());
		bResult &= pcChunkFile->WriteInt8(psMember->uiKind);
		bResult &= pcChunkFile->WriteInt32(psMember->uiType);
		bResult &= pcChunkFile->WriteInt64(psMember->iValue);
		bResult &= pcChunkFile->WriteInt8(psMember->uiBitOffset);
		bResult &= pcChunkFile->WriteInt8(psMember->uiBits);
	}
	bResult &= pcChunkFile->WriteChunkEnd("Members");
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Records WriteChunks wrote, into empty records.  False if they aren't
// whole, or name what isn't there.
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65DebugRecords::ReadChunks(CChunkFile* pcChunkFile)
{
	uint32					uiCount;
	uint32					ui;
	SAsm65DebugFunction*	psFunction;
	SAsm65DebugVariable*	psVariable;
	SAsm65DebugGlobal*		psGlobal;
	SAsm65DebugType*		psType;
	SAsm65DebugMember*		psMember;

	if (!pcChunkFile->ReadChunkBegin("DebugHeader") || !Asm65ReadMagic(pcChunkFile, ASM65_DEBUG_RECORDS_MAGIC, ASM65_DEBUG_RECORDS_VERSION) || !pcChunkFile->ReadChunkEnd())
	{
		return false;
	}

	if (!pcChunkFile->ReadChunkBegin("Functions") || !pcChunkFile->ReadInt32(&uiCount))
	{
		return false;
	}
	for (ui = 0; ui < uiCount; ui++)
	{
		psFunction = masFunctions.Add();
		psFunction->szName.Init();
		if (!Asm65ReadText(pcChunkFile, &psFunction->szName) || !Asm65ReadPlace(pcChunkFile, &psFunction->sStart) || !pcChunkFile->ReadInt32(&psFunction->uiEnd) ||
			!pcChunkFile->ReadInt32(&psFunction->uiBody) || !pcChunkFile->ReadInt8(&psFunction->uiEntry) || !pcChunkFile->ReadInt8(&psFunction->uiLink) || !pcChunkFile->ReadInt32(&psFunction->iLink) ||
			!pcChunkFile->ReadInt32(&psFunction->uiFirstVariable) || !pcChunkFile->ReadInt32(&psFunction->uiNumVariables))
		{
			return false;
		}
	}
	if (!pcChunkFile->ReadChunkEnd())
	{
		return false;
	}

	if (!pcChunkFile->ReadChunkBegin("Variables") || !pcChunkFile->ReadInt32(&uiCount))
	{
		return false;
	}
	for (ui = 0; ui < uiCount; ui++)
	{
		psVariable = masVariables.Add();
		psVariable->szName.Init();
		if (!Asm65ReadText(pcChunkFile, &psVariable->szName) || !pcChunkFile->ReadInt32(&psVariable->uiType) || !pcChunkFile->ReadInt8(&psVariable->uiWhere) ||
			!pcChunkFile->ReadBool(&psVariable->bParameter) || !pcChunkFile->ReadInt32(&psVariable->iOffset) || !pcChunkFile->ReadInt32(&psVariable->uiFirstLine) ||
			!pcChunkFile->ReadInt32(&psVariable->uiLastLine))
		{
			return false;
		}
	}
	if (!pcChunkFile->ReadChunkEnd())
	{
		return false;
	}

	if (!pcChunkFile->ReadChunkBegin("Globals") || !pcChunkFile->ReadInt32(&uiCount))
	{
		return false;
	}
	for (ui = 0; ui < uiCount; ui++)
	{
		psGlobal = masGlobals.Add();
		psGlobal->szName.Init();
		if (!Asm65ReadText(pcChunkFile, &psGlobal->szName) || !pcChunkFile->ReadInt32(&psGlobal->uiType) || !Asm65ReadPlace(pcChunkFile, &psGlobal->sPlace) ||
			!pcChunkFile->ReadInt32(&psGlobal->uiFunction))
		{
			return false;
		}
	}
	if (!pcChunkFile->ReadChunkEnd())
	{
		return false;
	}

	if (!pcChunkFile->ReadChunkBegin("Types") || !pcChunkFile->ReadInt32(&uiCount))
	{
		return false;
	}
	for (ui = 0; ui < uiCount; ui++)
	{
		psType = masTypes.Add();
		psType->szKey.Init();
		psType->szName.Init();
		if (!Asm65ReadText(pcChunkFile, &psType->szKey) || !Asm65ReadText(pcChunkFile, &psType->szName) || !pcChunkFile->ReadInt8(&psType->uiKind) ||
			!pcChunkFile->ReadInt8(&psType->uiFlags) || !pcChunkFile->ReadInt32(&psType->uiBytes) || !pcChunkFile->ReadInt32(&psType->uiTarget) ||
			!pcChunkFile->ReadInt32(&psType->uiCount) || !pcChunkFile->ReadInt32(&psType->uiFirstMember) || !pcChunkFile->ReadInt32(&psType->uiNumMembers))
		{
			return false;
		}
		mcTypeKeys.Put(psType->szKey.Text(), &ui);
	}
	if (!pcChunkFile->ReadChunkEnd())
	{
		return false;
	}

	if (!pcChunkFile->ReadChunkBegin("Members") || !pcChunkFile->ReadInt32(&uiCount))
	{
		return false;
	}
	for (ui = 0; ui < uiCount; ui++)
	{
		psMember = masMembers.Add();
		psMember->szName.Init();
		if (!Asm65ReadText(pcChunkFile, &psMember->szName) || !pcChunkFile->ReadInt8(&psMember->uiKind) || !pcChunkFile->ReadInt32(&psMember->uiType) ||
			!pcChunkFile->ReadInt64(&psMember->iValue) || !pcChunkFile->ReadInt8(&psMember->uiBitOffset) || !pcChunkFile->ReadInt8(&psMember->uiBits))
		{
			return false;
		}
	}
	return pcChunkFile->ReadChunkEnd() && IsWhole();
}


//////////////////////////////////////////////////////////////////////////
//
// Whether every index in the records is in range (after ReadChunks).
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65DebugRecords::IsWhole(void)
{
	size					ui;
	SAsm65DebugFunction*	psFunction;
	SAsm65DebugType*		psType;
	SAsm65DebugGlobal*		psGlobal;

	for (ui = 0; ui < masFunctions.NumElements(); ui++)
	{
		psFunction = masFunctions.Get(ui);
		if ((size)psFunction->uiFirstVariable + psFunction->uiNumVariables > masVariables.NumElements())
		{
			return false;
		}
	}
	for (ui = 0; ui < masTypes.NumElements(); ui++)
	{
		psType = masTypes.Get(ui);
		if ((size)psType->uiFirstMember + psType->uiNumMembers > masMembers.NumElements())
		{
			return false;
		}
	}
	for (ui = 0; ui < masGlobals.NumElements(); ui++)
	{
		psGlobal = masGlobals.Get(ui);
		if ((psGlobal->uiFunction != ASM65_DEBUG_NONE) && (psGlobal->uiFunction >= masFunctions.NumElements()))
		{
			return false;
		}
	}
	return IsValid();
}
