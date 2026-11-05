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
#ifndef __ASM65_IMAGE_H__
#define __ASM65_IMAGE_H__
#include "BaseLib/Chars.h"
#include "BaseLib/ArrayTemplate.h"
#include "BaseLib/XMLWriter.h"
#include "BaseLib/MarkupTag.h"
#include "Asm65Output.h"


//Where the code linked at uiStart up to uiEnd goes in the image: at uiOffset.
struct SAsm65ImageRegion
{
	uint32	uiStart;
	uint32	uiEnd;
	uint32	uiOffset;
};


typedef CArrayTemplate<SAsm65ImageRegion>	CArrayAsm65ImageRegions;


//A linked program as a file of a fixed size: a ROM, or anything else loaded whole.  Each segment the linker output
//goes where its region says; bytes nothing goes in are the fill (erased flash is $FF).  A segment not wholly inside
//one region is an error.  The limit is the highest address the linker may place anything at (CCPP65Builder::
//SetLimit, zero for none) and the load address is where a debugger writes the image.  A build configuration
//describes one as
//
//	<Image File="Kernal.rom" Size="0x80000" Fill="0xFF" Limit="0x7FC00" Load="0x0">
//		<Region Start="0x0" End="0x40000" Offset="0x0"/>
//	</Image>
//
//with numbers in hexadecimal (0x) or decimal.
class CAsm65Image
{
protected:
	CChars						mszFile;
	uint32						muiSize;
	uint8						muiFill;
	uint32						muiLimit;
	uint32						muiLoad;
	CArrayAsm65ImageRegions		masRegions;
	uint8*						mpuiImage;

public:
	void				Init(void);
	void				Init(const char* szFile, uint32 uiSize, uint8 uiFill, uint32 uiLimit, uint32 uiLoad);
	void				Kill(void);

	void				AddRegion(uint32 uiStart, uint32 uiEnd, uint32 uiOffset);

	void				SaveXML(CXMLWriter* pcWriter);
	bool				LoadXML(CMarkupTag* pcTag);

	bool				Make(CAsm65Output* pcOutput, CChars* pszErrors);
	bool				Write(const char* szFilename);

	char*				GetFile(void);
	uint32				GetSize(void);
	uint8				GetFill(void);
	uint32				GetLimit(void);
	uint32				GetLoad(void);
	size				NumRegions(void);
	SAsm65ImageRegion*	GetRegion(size uiIndex);
	uint8*				GetImage(void);

protected:
	SAsm65ImageRegion*	FindRegion(uint32 uiStart, uint32 uiEnd);
};


#endif // __ASM65_IMAGE_H__

