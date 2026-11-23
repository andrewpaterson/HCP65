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


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CAsm65Symbols::Init(void)
{
	mcSymbols.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CAsm65Symbols::Kill(void)
{
	mcSymbols.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
SAsm65Symbol* CAsm65Symbols::Get(const char* szName, size uiLength)
{
	return mcSymbols.Get(szName, uiLength);
}


//////////////////////////////////////////////////////////////////////////
//
// Returns the existing symbol or a new one that is not yet resolved.  The
// pointer lasts until the next Put.
//
//////////////////////////////////////////////////////////////////////////
SAsm65Symbol* CAsm65Symbols::Put(const char* szName, size uiLength)
{
	SAsm65Symbol*	psSymbol;
	bool			bAdded;

	psSymbol = mcSymbols.FindOrAdd(szName, uiLength, &bAdded);
	if (bAdded)
	{
		psSymbol->iValue = 0;
		psSymbol->uiPass = 0;
		psSymbol->bResolved = false;
		psSymbol->iSection = ASM65_SECTION_ABSOLUTE;
		psSymbol->uiTerm = 0;
		psSymbol->uiIndex = (size)(mcSymbols.NumElements() - 1);
		psSymbol->uiNamedLine = 0;
		psSymbol->uiNamedColumn = 0;
		psSymbol->bExternal = false;
		psSymbol->bGlobal = false;
		psSymbol->bWeak = false;
		psSymbol->bLabel = false;
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
const char* CAsm65Symbols::GetName(size uiIndex)
{
	return mcSymbols.GetKey(uiIndex);
}
