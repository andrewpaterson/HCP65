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
#include "Asm65BaseAssembler.h"


//The words the debug directives take for their kinds, in each enum's order.
static const char*				gaszAsm65DebugEntries[] = { "long", "short", "interrupt", "userinterrupt", NULL };
static const char*				gaszAsm65DebugWheres[] = { "none", "frame", "indirect", "ranges", NULL };
static const char*				gaszAsm65DebugRangePlaces[] = { "frame", "register", "memory", "address", "value", "frameaddress", NULL };
static const EAsm65DebugWhere	gaeAsm65DebugRangePlaces[] = { ADW_Frame, ADW_Register, ADW_Memory, ADW_Address, ADW_Value, ADW_FrameAddress };
static const char*				gaszAsm65DebugVariableKinds[] = { "local", "parameter", NULL };
static const char*				gaszAsm65DebugTypes[] = { "void", "bool", "integer", "float", "enum", "pointer", "reference", "array", "class", "function", NULL };
static const char*				gaszAsm65DebugMembers[] = { "base", "field", "vtable", "enumerator", NULL };


//////////////////////////////////////////////////////////////////////////
//
// The debug directives, which describe a unit's functions, variables and
// types for a debugger (CAsm65DebugRecords, kept in the object).  Each is
// only allowed in an object, and adds its record in the final pass:
//
//   .dfunction "name"[, entry[, type]] A function starts here, got to by
//                                      entry: long (jsl, as when left out),
//                                      short (jsr), interrupt or
//                                      userinterrupt (a kernel's from user
//                                      mode), returning type (none when left
//                                      out).
//   .dframe none                       Its prologue ends here, and its frame
//   .dframe R, offset                  address (S before its call) is found
//   .dframe [R + offset], adjust       by following its instructions, or is
//                                      register R plus offset, or the address
//                                      held there plus adjust
//                                      (EAsm65DebugFrame).  R is one of the
//                                      processor's registers (S, D, ... ).
//   .dsaved R, offset                  It saved register R (or bytes at an
//   .dsaved [address], bytes, offset   address) for its caller at its frame
//                                      address plus offset.
//   .dbase R | frame                   Its variables are from register R, or
//                                      its frame address (as when left out).
//   .dend                              It ends here, before code with no
//                                      records (else at its section's end).
//   .dvariable local|parameter, "name", type, where, offset, first, last[, inline]
//                                      A variable of the function (or of its
//                                      inline): where is none, frame,
//                                      indirect or ranges (the .dranges
//                                      after it), in scope from line first
//                                      to last.
//   .drange from, to, frame, offset    Where the last variable (of ranges) is
//   .drange from, to, register, R      from the label from up to the label
//   .drange from, to, memory, R, offset  to: at the base + offset, in
//   .drange from, to, address, n       register R, at R + offset, or at
//   .drange from, to, value, n         the address n; or its value there:
//   .drange from, to, frameaddress, offset  n, or the address base +
//                                      offset.
//   .dinline parent, "name", "file", line  A call of name inlined into the
//                                      function, in its inline parent (-1
//                                      for none), at the file's line.
//   .dinspan from, to                  The last inline's code from the
//                                      label from up to the label to.
//   .dglobal "name", type, label       A global at the label.
//   .dstatic "name", type, label[, inline]  A static local of the function
//                                      (or of its inline).
//   .dtype index, "key", "name", kind, flags, bytes, target, count
//                                      The unit's index'th type.
//   .dmember kind, "name", type, value, bit offset, bits
//                                      A member of the last type: base,
//                                      field, vtable or enumerator.
//
// A type is its index in the unit's types, -1 for none.  False if
// psDirective isn't one of them; else *pbResult is whether it was right.
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65BaseAssembler::DebugDirective(SAsm65Token* psDirective, bool* pbResult)
{
	if (psDirective->IsIdentifierIgnoreCase(".dfunction"))
	{
		*pbResult = DebugObject(psDirective) && DebugFunction(psDirective);
	}
	else if (psDirective->IsIdentifierIgnoreCase(".dframe"))
	{
		*pbResult = DebugObject(psDirective) && DebugFrame(psDirective);
	}
	else if (psDirective->IsIdentifierIgnoreCase(".dsaved"))
	{
		*pbResult = DebugObject(psDirective) && DebugSaved(psDirective);
	}
	else if (psDirective->IsIdentifierIgnoreCase(".dbase"))
	{
		*pbResult = DebugObject(psDirective) && DebugBase(psDirective);
	}
	else if (psDirective->IsIdentifierIgnoreCase(".dend"))
	{
		*pbResult = DebugObject(psDirective) && DebugEnd(psDirective);
	}
	else if (psDirective->IsIdentifierIgnoreCase(".dvariable"))
	{
		*pbResult = DebugObject(psDirective) && DebugVariable(psDirective);
	}
	else if (psDirective->IsIdentifierIgnoreCase(".drange"))
	{
		*pbResult = DebugObject(psDirective) && DebugRange(psDirective);
	}
	else if (psDirective->IsIdentifierIgnoreCase(".dinline"))
	{
		*pbResult = DebugObject(psDirective) && DebugInline(psDirective);
	}
	else if (psDirective->IsIdentifierIgnoreCase(".dinspan"))
	{
		*pbResult = DebugObject(psDirective) && DebugInlineSpan(psDirective);
	}
	else if (psDirective->IsIdentifierIgnoreCase(".dglobal"))
	{
		*pbResult = DebugObject(psDirective) && DebugGlobal(psDirective, false);
	}
	else if (psDirective->IsIdentifierIgnoreCase(".dstatic"))
	{
		*pbResult = DebugObject(psDirective) && DebugGlobal(psDirective, true);
	}
	else if (psDirective->IsIdentifierIgnoreCase(".dtype"))
	{
		*pbResult = DebugObject(psDirective) && DebugType(psDirective);
	}
	else if (psDirective->IsIdentifierIgnoreCase(".dmember"))
	{
		*pbResult = DebugObject(psDirective) && DebugMember(psDirective);
	}
	else
	{
		return false;
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65BaseAssembler::DebugObject(SAsm65Token* psDirective)
{
	if (!mbObject)
	{
		return Error(psDirective, "Debug directives are only allowed when assembling an object");
	}
	mpcObject->GetDebug()->SetProcessor(DebugProcessor());
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65BaseAssembler::DebugFunction(SAsm65Token* psDirective)
{
	CChars	szName;
	int		iEntry;
	uint32	uiResult;
	bool	bResult;

	iEntry = ADE_Long;
	uiResult = ASM65_DEBUG_NONE;
	szName.Init();
	bResult = DebugText(&szName, "a function's name");
	if (bResult && Accept(AP65_Comma))
	{
		bResult = DebugWord(gaszAsm65DebugEntries, &iEntry, "long, short, interrupt or userinterrupt");
		if (bResult && Accept(AP65_Comma))
		{
			bResult = DebugTypeIndex(&uiResult);
		}
	}
	if (bResult && mbFinal)
	{
		mpcObject->GetDebug()->AddFunction(szName.Text(), muiSection, muiAddress, (EAsm65DebugEntry)iEntry, uiResult);
	}
	szName.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65BaseAssembler::DebugFrame(SAsm65Token* psDirective)
{
	EAsm65DebugFrame	eFrame;
	uint8				uiRegister;
	int64				iOffset;
	int64				iAdjust;
	bool				bResult;

	uiRegister = ASM65_DEBUG_S;
	iOffset = 0;
	iAdjust = 0;
	if (Peek()->IsIdentifierIgnoreCase("none"))
	{
		Next();
		eFrame = ADF_FollowS;
		bResult = true;
	}
	else if (Accept(AP65_LeftSquare))
	{
		eFrame = ADF_Memory;
		bResult = DebugRegisterPlace(&uiRegister, &iOffset) && Expect(AP65_RightSquare, "']'") && Expect(AP65_Comma, "','") &&
				  DebugNumber(&iAdjust, -0x7FFFFFFFll, 0x7FFFFFFFll, "an adjustment");
	}
	else
	{
		eFrame = ADF_Register;
		bResult = DebugRegister(&uiRegister) && Expect(AP65_Comma, "','") && DebugNumber(&iOffset, -0x7FFFFFFFll, 0x7FFFFFFFll, "a frame offset");
	}
	if (bResult && mbFinal && !mpcObject->GetDebug()->SetFrame(muiAddress, eFrame, uiRegister, (int32)iOffset, (int32)iAdjust))
	{
		return Error(psDirective, ".dframe must follow a .dfunction");
	}
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// .dsaved: a register (or memory) the function saved for its caller.
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65BaseAssembler::DebugSaved(SAsm65Token* psDirective)
{
	uint8	uiRegister;
	int64	iAddress;
	int64	iBytes;
	int64	iOffset;
	bool	bMemory;
	bool	bResult;

	iAddress = 0;
	iBytes = 0;
	bMemory = Accept(AP65_LeftSquare);
	if (bMemory)
	{
		bResult = DebugNumber(&iAddress, 0, 0xFFFFFFFFll, "an address") && Expect(AP65_RightSquare, "']'") && Expect(AP65_Comma, "','") &&
				  DebugNumber(&iBytes, 1, 8, "the bytes saved");
	}
	else
	{
		bResult = DebugRegister(&uiRegister);
		iAddress = bResult ? uiRegister : 0;
	}
	bResult = bResult && Expect(AP65_Comma, "','") && DebugNumber(&iOffset, -0x7FFFFFFFll, 0x7FFFFFFFll, "a frame offset");
	if (bResult && mbFinal && !mpcObject->GetDebug()->AddSaved((uint32)iAddress, bMemory, (uint8)iBytes, (int32)iOffset))
	{
		return Error(psDirective, ".dsaved must follow a .dfunction");
	}
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// .dbase: what the function's variables' offsets are from.
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65BaseAssembler::DebugBase(SAsm65Token* psDirective)
{
	uint8	uiRegister;

	if (Peek()->IsIdentifierIgnoreCase("frame"))
	{
		Next();
		uiRegister = ASM65_DEBUG_FRAME;
	}
	else if (!DebugRegister(&uiRegister))
	{
		return false;
	}
	if (mbFinal && !mpcObject->GetDebug()->SetBase(uiRegister))
	{
		return Error(psDirective, ".dbase must follow a .dfunction");
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65BaseAssembler::DebugEnd(SAsm65Token* psDirective)
{
	if (mbFinal && !mpcObject->GetDebug()->EndFunction(muiAddress))
	{
		return Error(psDirective, ".dend must follow a .dfunction not yet ended");
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65BaseAssembler::DebugVariable(SAsm65Token* psDirective)
{
	CChars		szName;
	int			iKind;
	int			iWhere;
	uint32		uiType;
	int64		iOffset;
	int64		iFirst;
	int64		iLast;
	int64		iInline;
	bool		bResult;

	iKind = 0;
	iWhere = ADW_None;
	iInline = -1;
	szName.Init();
	bResult = DebugWord(gaszAsm65DebugVariableKinds, &iKind, "local or parameter") && Expect(AP65_Comma, "','") &&
			  DebugText(&szName, "a variable's name") && Expect(AP65_Comma, "','") &&
			  DebugTypeIndex(&uiType) && Expect(AP65_Comma, "','") &&
			  DebugWord(gaszAsm65DebugWheres, &iWhere, "none, frame, indirect or ranges") && Expect(AP65_Comma, "','") &&
			  DebugNumber(&iOffset, -0x7FFFFFFFll, 0x7FFFFFFFll, "a frame offset") && Expect(AP65_Comma, "','") &&
			  DebugNumber(&iFirst, 0, 0x7FFFFFFFll, "a line") && Expect(AP65_Comma, "','") &&
			  DebugNumber(&iLast, iFirst, 0x7FFFFFFFll, "a line no earlier than the first");
	if (bResult && Accept(AP65_Comma))
	{
		bResult = DebugNumber(&iInline, 0, 0x7FFFFFFFll, "an inline");
	}
	if (bResult && mbFinal && !mpcObject->GetDebug()->AddVariable(szName.Text(), uiType, (EAsm65DebugWhere)iWhere, iKind == 1, (int32)iOffset, (uint32)iFirst, (uint32)iLast, (int32)iInline))
	{
		bResult = Error(psDirective, ".dvariable must follow a .dfunction");
	}
	szName.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// .dglobal and .dstatic: a variable at a label in one of the object's
// sections.
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65BaseAssembler::DebugGlobal(SAsm65Token* psDirective, bool bStatic)
{
	CChars			szName;
	uint32			uiType;
	SAsm65Value		sValue;
	int64			iInline;
	bool			bResult;

	szName.Init();
	iInline = -1;
	bResult = DebugText(&szName, "a variable's name") && Expect(AP65_Comma, "','") && DebugTypeIndex(&uiType) && Expect(AP65_Comma, "','") &&
			  DebugLabel(&sValue, "A debug global");
	if (bResult && bStatic && Accept(AP65_Comma))
	{
		bResult = DebugNumber(&iInline, 0, 0x7FFFFFFFll, "an inline");
	}
	if (bResult && mbFinal && !mpcObject->GetDebug()->AddGlobal(szName.Text(), uiType, (uint32)sValue.iSection, (uint32)sValue.iValue, bStatic, (int32)iInline))
	{
		bResult = Error(psDirective, ".dstatic must follow a .dfunction");
	}
	szName.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// .drange: where the last variable (of ranges) is between two labels.
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65BaseAssembler::DebugRange(SAsm65Token* psDirective)
{
	SAsm65Value		sFrom;
	SAsm65Value		sTo;
	int				iPlace;
	uint8			uiRegister;
	int64			iOffset;
	int64			iValue;
	bool			bResult;

	iPlace = 0;
	uiRegister = 0;
	iOffset = 0;
	iValue = 0;
	bResult = DebugLabel(&sFrom, "A debug range") && Expect(AP65_Comma, "','") && DebugLabel(&sTo, "A debug range") && Expect(AP65_Comma, "','") &&
			  DebugWord(gaszAsm65DebugRangePlaces, &iPlace, "frame, register, memory, address, value or frameaddress") && Expect(AP65_Comma, "','");
	if (bResult && ((iPlace == 0) || (iPlace == 5)))
	{
		bResult = DebugNumber(&iOffset, -0x7FFFFFFFll, 0x7FFFFFFFll, "a frame offset");
	}
	else if (bResult && (iPlace == 3))
	{
		bResult = DebugNumber(&iOffset, 0, 0x7FFFFFFFll, "an address");
	}
	else if (bResult && (iPlace == 4))
	{
		bResult = DebugNumber(&iValue, -0x7FFFFFFFFFFFFFFFll - 1, 0x7FFFFFFFFFFFFFFFll, "a value");
	}
	else if (bResult)
	{
		bResult = DebugRegister(&uiRegister);
		if (bResult && (iPlace == 2))
		{
			bResult = Expect(AP65_Comma, "','") && DebugNumber(&iOffset, -0x7FFFFFFFll, 0x7FFFFFFFll, "an offset");
		}
	}
	if (bResult && mbFinal && ((sFrom.iSection != sTo.iSection) ||
		!mpcObject->GetDebug()->AddRange((uint32)sFrom.iSection, (uint32)sFrom.iValue, (uint32)sTo.iValue, gaeAsm65DebugRangePlaces[iPlace], uiRegister, (int32)iOffset, iValue)))
	{
		bResult = Error(psDirective, ".drange must follow a .dvariable of ranges, its labels in order in its function's section");
	}
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// .dinline: a call inlined into the function.
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65BaseAssembler::DebugInline(SAsm65Token* psDirective)
{
	CChars	szName;
	CChars	szFile;
	int64	iParent;
	int64	iLine;
	bool	bResult;

	szName.Init();
	szFile.Init();
	bResult = DebugNumber(&iParent, -1, 0x7FFFFFFFll, "an inline or -1") && Expect(AP65_Comma, "','") &&
			  DebugText(&szName, "a function's name") && Expect(AP65_Comma, "','") &&
			  DebugText(&szFile, "a file's name") && Expect(AP65_Comma, "','") &&
			  DebugNumber(&iLine, 0, 0x7FFFFFFFll, "a line");
	if (bResult && mbFinal && !mpcObject->GetDebug()->AddInline(szName.Text(), (int32)iParent, szFile.Text(), (uint32)iLine))
	{
		bResult = Error(psDirective, ".dinline must follow a .dfunction, its parent an inline before it");
	}
	szFile.Kill();
	szName.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// .dinspan: the last inline's code between two labels.
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65BaseAssembler::DebugInlineSpan(SAsm65Token* psDirective)
{
	SAsm65Value	sFrom;
	SAsm65Value	sTo;
	bool		bResult;

	bResult = DebugLabel(&sFrom, "An inline's span") && Expect(AP65_Comma, "','") && DebugLabel(&sTo, "An inline's span");
	if (bResult && mbFinal && ((sFrom.iSection != sTo.iSection) || !mpcObject->GetDebug()->AddInlineSpan((uint32)sFrom.iSection, (uint32)sFrom.iValue, (uint32)sTo.iValue)))
	{
		bResult = Error(psDirective, ".dinspan must follow a .dinline, its labels in order in its function's section");
	}
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// A label in one of the object's sections (in the final pass): its section
// and offset, which a debug record keeps as they are (no relocation).
// szWhat says what wants one when it isn't.
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65BaseAssembler::DebugLabel(SAsm65Value* psValue, const char* szWhat)
{
	SAsm65Token*	psLabel;
	CChars			szError;
	bool			bResult;

	psLabel = Peek();
	ClearValue(psValue);
	bResult = Expression(psValue);
	if (bResult && (psValue->iTerms > 0))
	{
		UseTerm();
	}
	if (bResult && mbFinal && (!psValue->bResolved || (psValue->iSection < 0)))
	{
		szError.Init(szWhat)->Append(" must be at a label in one of the object's sections");
		bResult = Error(psLabel, szError.Text());
		szError.Kill();
	}
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65BaseAssembler::DebugType(SAsm65Token* psDirective)
{
	CChars			szKey;
	CChars			szName;
	SAsm65Token*	psIndex;
	int64			iIndex;
	int				iKind;
	int64			iFlags;
	int64			iBytes;
	uint32			uiTarget;
	int64			iCount;
	bool			bResult;

	iKind = ADT_Void;
	szKey.Init();
	szName.Init();
	psIndex = Peek();
	bResult = DebugNumber(&iIndex, 0, 0x7FFFFFFFll, "a type's index") && Expect(AP65_Comma, "','") &&
			  DebugText(&szKey, "a type's key") && Expect(AP65_Comma, "','") &&
			  DebugText(&szName, "a type's name") && Expect(AP65_Comma, "','") &&
			  DebugWord(gaszAsm65DebugTypes, &iKind, "a kind of type") && Expect(AP65_Comma, "','") &&
			  DebugNumber(&iFlags, 0, 0xFF, "a type's flags") && Expect(AP65_Comma, "','") &&
			  DebugNumber(&iBytes, 0, 0xFFFFFFFFll, "a type's size") && Expect(AP65_Comma, "','") &&
			  DebugTypeIndex(&uiTarget) && Expect(AP65_Comma, "','") &&
			  DebugNumber(&iCount, 0, 0xFFFFFFFFll, "a count");
	if (bResult && mbFinal)
	{
		if ((uint32)iIndex != mpcObject->GetDebug()->NumTypes())
		{
			bResult = Error(psIndex, "A .dtype's index must be the number of types before it");
		}
		else if (!mpcObject->GetDebug()->AddType(szKey.Text(), szName.Text(), (EAsm65DebugType)iKind, (uint8)iFlags, (uint32)iBytes, uiTarget, (uint32)iCount))
		{
			bResult = Error(psDirective, "A type of that key is already described");
		}
	}
	szName.Kill();
	szKey.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65BaseAssembler::DebugMember(SAsm65Token* psDirective)
{
	CChars	szName;
	int		iKind;
	uint32	uiType;
	int64	iValue;
	int64	iBitOffset;
	int64	iBits;
	bool	bResult;

	iKind = ADM_Field;
	szName.Init();
	bResult = DebugWord(gaszAsm65DebugMembers, &iKind, "base, field, vtable or enumerator") && Expect(AP65_Comma, "','") &&
			  DebugText(&szName, "a member's name") && Expect(AP65_Comma, "','") &&
			  DebugTypeIndex(&uiType) && Expect(AP65_Comma, "','") &&
			  ExpressionValue(&iValue) && Expect(AP65_Comma, "','") &&
			  DebugNumber(&iBitOffset, 0, 0xFF, "a bit offset") && Expect(AP65_Comma, "','") &&
			  DebugNumber(&iBits, 0, 0xFF, "a number of bits");
	if (bResult && mbFinal && !mpcObject->GetDebug()->AddMember(szName.Text(), (EAsm65DebugMember)iKind, uiType, iValue, (uint8)iBitOffset, (uint8)iBits))
	{
		bResult = Error(psDirective, ".dmember must follow a .dtype");
	}
	szName.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65BaseAssembler::DebugText(CChars* pszDest, const char* szWhat)
{
	SAsm65Token*	psText;

	psText = Peek();
	if (psText->eType != AT65_String)
	{
		return ErrorExpected(psText, szWhat);
	}
	Next();
	pszDest->Set(mcTokeniser.GetString(psText));
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65BaseAssembler::DebugNumber(int64* piValue, int64 iMin, int64 iMax, const char* szWhat)
{
	SAsm65Token*	psNumber;

	psNumber = Peek();
	if (!ExpressionValue(piValue))
	{
		return false;
	}
	if ((*piValue < iMin) || (*piValue > iMax))
	{
		return ErrorExpected(psNumber, szWhat);
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// An identifier, one of aszWords (NULL ended): *piIndex is which.
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65BaseAssembler::DebugWord(const char** aszWords, int* piIndex, const char* szWhat)
{
	SAsm65Token*	psWord;
	int				i;

	psWord = Peek();
	for (i = 0; aszWords[i]; i++)
	{
		if (psWord->IsIdentifierIgnoreCase(aszWords[i]))
		{
			Next();
			*piIndex = i;
			return true;
		}
	}
	return ErrorExpected(psWord, szWhat);
}


//////////////////////////////////////////////////////////////////////////
//
// A type's index in the unit's types, -1 for none.
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65BaseAssembler::DebugRegister(uint8* puiRegister)
{
	const char*	aszNames[ASM65_DEBUG_REGISTERS + 1];
	size		ui;
	int			iRegister;

	for (ui = 0; ui < Asm65DebugNumRegisters(DebugProcessor()); ui++)
	{
		aszNames[ui] = Asm65DebugRegisterName(DebugProcessor(), ui);
	}
	aszNames[ui] = NULL;
	if (!DebugWord(aszNames, &iRegister, "one of the processor's registers"))
	{
		return false;
	}
	*puiRegister = (uint8)iRegister;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// A register and an offset from it, "R + offset" or "R - offset".
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65BaseAssembler::DebugRegisterPlace(uint8* puiRegister, int64* piOffset)
{
	bool	bMinus;

	if (!DebugRegister(puiRegister))
	{
		return false;
	}
	bMinus = Peek()->Is(AP65_Minus);
	if (!Accept(AP65_Plus) && !Accept(AP65_Minus))
	{
		return ErrorExpected(Peek(), "'+' or '-'");
	}
	if (!DebugNumber(piOffset, 0, 0x7FFFFFFFll, "an offset"))
	{
		return false;
	}
	*piOffset = bMinus ? -*piOffset : *piOffset;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65BaseAssembler::DebugTypeIndex(uint32* puiType)
{
	int64	iType;

	if (!DebugNumber(&iType, -1, 0x7FFFFFFFll, "a type's index or -1"))
	{
		return false;
	}
	*puiType = (iType < 0) ? ASM65_DEBUG_NONE : (uint32)iType;
	return true;
}
