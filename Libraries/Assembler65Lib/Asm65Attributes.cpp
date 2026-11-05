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
#include "Asm65Attributes.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void Asm65WriteHexAttribute(CXMLWriter* pcWriter, const char* szName, int64 iValue)
{
	char	szValue[24];

	snprintf(szValue, sizeof(szValue), "0x%llX", (unsigned long long)iValue);
	pcWriter->Attribute(szName, szValue);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool Asm65ReadNumberAttribute(CMarkupTag* pcTag, const char* szName, int64* piValue)
{
	char*		szValue;
	char*		szEnd;
	long long	lli;

	szValue = pcTag->GetStringAttribute(szName);
	if ((szValue == NULL) || (*szValue == '\0'))
	{
		return false;
	}
	lli = strtoll(szValue, &szEnd, 0);
	if (*szEnd != '\0')
	{
		return false;
	}
	*piValue = (int64)lli;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool Asm65ReadUint32Attribute(CMarkupTag* pcTag, const char* szName, uint32* puiValue)
{
	int64	iValue;

	if (!Asm65ReadNumberAttribute(pcTag, szName, &iValue) || (iValue < 0) || (iValue > 0xFFFFFFFFLL))
	{
		return false;
	}
	*puiValue = (uint32)iValue;
	return true;
}

