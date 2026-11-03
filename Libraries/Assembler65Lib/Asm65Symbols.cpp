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
#include "BaseLib/Chars.h"
#include "Asm65Object.h"
#include "Asm65Symbols.h"


//Names shorter than this are looked up from a copy on the stack.
#define ASM65_SYMBOLS_SHORT_NAME	256


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CAsm65Symbols::Init(void)
{
	mcSymbols.Init(true, true);
	maszNames.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CAsm65Symbols::Kill(void)
{
	maszNames.Kill();
	mcSymbols.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
SAsm65Symbol* CAsm65Symbols::Get(const char* szName, size uiLength)
{
	CChars			szKey;
	SAsm65Symbol*	psSymbol;
	char			szShort[ASM65_SYMBOLS_SHORT_NAME];

	//Most names fit on the stack (looking up allocates nothing).
	if (uiLength < ASM65_SYMBOLS_SHORT_NAME)
	{
		memcpy(szShort, szName, uiLength);
		szShort[uiLength] = '\0';
		return mcSymbols.Get(szShort);
	}
	szKey.Init();
	szKey.Append(szName, uiLength);
	psSymbol = mcSymbols.Get(szKey.Text());
	szKey.Kill();
	return psSymbol;
}


//////////////////////////////////////////////////////////////////////////
//
// Returns the existing symbol or a new one that is not yet resolved.
//
//////////////////////////////////////////////////////////////////////////
SAsm65Symbol* CAsm65Symbols::Put(const char* szName, size uiLength)
{
	CChars			szKey;
	SAsm65Symbol*	psSymbol;

	psSymbol = Get(szName, uiLength);
	if (psSymbol == NULL)
	{
		szKey.Init();
		szKey.Append(szName, uiLength);
		psSymbol = mcSymbols.Put(szKey.Text());
		psSymbol->iValue = 0;
		psSymbol->uiPass = 0;
		psSymbol->bResolved = false;
		psSymbol->iSection = ASM65_SECTION_ABSOLUTE;
		psSymbol->uiTerm = 0;
		psSymbol->uiIndex = maszNames.NumElements();
		psSymbol->uiNamedLine = 0;
		psSymbol->uiNamedColumn = 0;
		psSymbol->bExternal = false;
		psSymbol->bGlobal = false;
		psSymbol->bWeak = false;
		psSymbol->bLabel = false;
		maszNames.Add(szKey.Text());
		szKey.Kill();
	}
	return psSymbol;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
size CAsm65Symbols::NumSymbols(void)
{
	return mcSymbols.NumElements();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
char* CAsm65Symbols::GetName(size uiIndex)
{
	return maszNames.GetText(uiIndex);
}
