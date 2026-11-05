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
#ifndef __ASM65_ATTRIBUTES_H__
#define __ASM65_ATTRIBUTES_H__
#include "BaseLib/XMLWriter.h"
#include "BaseLib/MarkupTag.h"


//Addresses and sizes in XML (an <Image>'s, an <Executable>'s): written in hexadecimal (0x), read in hexadecimal or
//decimal.

void	Asm65WriteHexAttribute(CXMLWriter* pcWriter, const char* szName, int64 iValue);

//False if the attribute isn't there or isn't a number.
bool	Asm65ReadNumberAttribute(CMarkupTag* pcTag, const char* szName, int64* piValue);

//False if the attribute isn't there or isn't an unsigned 32 bit number.
bool	Asm65ReadUint32Attribute(CMarkupTag* pcTag, const char* szName, uint32* puiValue);


#endif // __ASM65_ATTRIBUTES_H__

