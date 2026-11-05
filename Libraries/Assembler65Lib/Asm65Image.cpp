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
#include "BaseLib/PointerFunctions.h"
#include "Asm65Attributes.h"
#include "Asm65Image.h"


#define ASM65_IMAGE_TAG				"Image"
#define ASM65_IMAGE_FILE			"File"
#define ASM65_IMAGE_SIZE			"Size"
#define ASM65_IMAGE_FILL			"Fill"
#define ASM65_IMAGE_LIMIT			"Limit"
#define ASM65_IMAGE_LOAD			"Load"
#define ASM65_IMAGE_REGION_TAG		"Region"
#define ASM65_IMAGE_REGION_START	"Start"
#define ASM65_IMAGE_REGION_END		"End"
#define ASM65_IMAGE_REGION_OFFSET	"Offset"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CAsm65Image::Init(void)
{
	Init("", 0, 0xFF, 0, 0);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CAsm65Image::Init(const char* szFile, uint32 uiSize, uint8 uiFill, uint32 uiLimit, uint32 uiLoad)
{
	mszFile.Init(szFile);
	muiSize = uiSize;
	muiFill = uiFill;
	muiLimit = uiLimit;
	muiLoad = uiLoad;
	masRegions.Init();
	mpuiImage = NULL;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CAsm65Image::Kill(void)
{
	SafeFree(mpuiImage);
	masRegions.Kill();
	mszFile.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CAsm65Image::AddRegion(uint32 uiStart, uint32 uiEnd, uint32 uiOffset)
{
	SAsm65ImageRegion*	psRegion;

	psRegion = masRegions.Add();
	psRegion->uiStart = uiStart;
	psRegion->uiEnd = uiEnd;
	psRegion->uiOffset = uiOffset;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CAsm65Image::SaveXML(CXMLWriter* pcWriter)
{
	size				i;
	SAsm65ImageRegion*	psRegion;

	pcWriter->Begin(ASM65_IMAGE_TAG);
	pcWriter->Attribute(ASM65_IMAGE_FILE, mszFile.Text());
	Asm65WriteHexAttribute(pcWriter, ASM65_IMAGE_SIZE, muiSize);
	Asm65WriteHexAttribute(pcWriter, ASM65_IMAGE_FILL, muiFill);
	Asm65WriteHexAttribute(pcWriter, ASM65_IMAGE_LIMIT, muiLimit);
	Asm65WriteHexAttribute(pcWriter, ASM65_IMAGE_LOAD, muiLoad);
	for (i = 0; i < masRegions.NumElements(); i++)
	{
		psRegion = masRegions.Get(i);
		pcWriter->Begin(ASM65_IMAGE_REGION_TAG);
		Asm65WriteHexAttribute(pcWriter, ASM65_IMAGE_REGION_START, psRegion->uiStart);
		Asm65WriteHexAttribute(pcWriter, ASM65_IMAGE_REGION_END, psRegion->uiEnd);
		Asm65WriteHexAttribute(pcWriter, ASM65_IMAGE_REGION_OFFSET, psRegion->uiOffset);
		pcWriter->End();
	}
	pcWriter->End();
}


//////////////////////////////////////////////////////////////////////////
//
// False if pcTag isn't an image with a file, a size and at least one
// region, each region inside the image.  Fill (default $FF), Limit
// (default none) and Load (default zero) may be left out.  Call it on an
// image just Init'ed.
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65Image::LoadXML(CMarkupTag* pcTag)
{
	char*			szFile;
	uint32			uiFill;
	uint32			uiStart;
	uint32			uiEnd;
	uint32			uiOffset;
	CMarkupTag*		pcRegion;
	STagIterator	sIterator;

	if (StringCompare(pcTag->GetName(), ASM65_IMAGE_TAG) != 0)
	{
		return false;
	}
	szFile = pcTag->GetStringAttribute(ASM65_IMAGE_FILE);
	if (!szFile || (szFile[0] == '\0') || !Asm65ReadUint32Attribute(pcTag, ASM65_IMAGE_SIZE, &muiSize) || (muiSize == 0))
	{
		return false;
	}
	mszFile.Set(szFile);

	uiFill = 0xFF;
	if (pcTag->GetStringAttribute(ASM65_IMAGE_FILL) && (!Asm65ReadUint32Attribute(pcTag, ASM65_IMAGE_FILL, &uiFill) || (uiFill > 0xFF)))
	{
		return false;
	}
	muiFill = (uint8)uiFill;
	muiLimit = 0;
	if (pcTag->GetStringAttribute(ASM65_IMAGE_LIMIT) && !Asm65ReadUint32Attribute(pcTag, ASM65_IMAGE_LIMIT, &muiLimit))
	{
		return false;
	}
	muiLoad = 0;
	if (pcTag->GetStringAttribute(ASM65_IMAGE_LOAD) && !Asm65ReadUint32Attribute(pcTag, ASM65_IMAGE_LOAD, &muiLoad))
	{
		return false;
	}

	masRegions.Kill();
	masRegions.Init();
	for (pcRegion = pcTag->GetTag(ASM65_IMAGE_REGION_TAG, &sIterator); pcRegion; pcRegion = pcTag->GetNextTag(&sIterator))
	{
		if (!Asm65ReadUint32Attribute(pcRegion, ASM65_IMAGE_REGION_START, &uiStart) || !Asm65ReadUint32Attribute(pcRegion, ASM65_IMAGE_REGION_END, &uiEnd) || !Asm65ReadUint32Attribute(pcRegion, ASM65_IMAGE_REGION_OFFSET, &uiOffset))
		{
			return false;
		}
		if ((uiEnd <= uiStart) || (uiOffset > muiSize) || (uiEnd - uiStart > muiSize - uiOffset))
		{
			return false;
		}
		AddRegion(uiStart, uiEnd, uiOffset);
	}
	return masRegions.NumElements() != 0;
}


//////////////////////////////////////////////////////////////////////////
//
// The region holding all of uiStart up to uiEnd, or NULL.
//
//////////////////////////////////////////////////////////////////////////
SAsm65ImageRegion* CAsm65Image::FindRegion(uint32 uiStart, uint32 uiEnd)
{
	size				i;
	SAsm65ImageRegion*	psRegion;

	for (i = 0; i < masRegions.NumElements(); i++)
	{
		psRegion = masRegions.Get(i);
		if ((uiStart >= psRegion->uiStart) && (uiEnd <= psRegion->uiEnd))
		{
			return psRegion;
		}
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The image of pcOutput's segments (GetImage).  False, with why appended
// to pszErrors, if a segment is outside the regions.
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65Image::Make(CAsm65Output* pcOutput, CChars* pszErrors)
{
	size				i;
	SAsm65Segment*		psSegment;
	SAsm65ImageRegion*	psRegion;
	uint32				uiStart;
	uint32				uiEnd;
	char				szAddress[16];

	SafeFree(mpuiImage);
	mpuiImage = (uint8*)malloc(muiSize);
	memset(mpuiImage, muiFill, muiSize);

	for (i = 0; i < pcOutput->NumSegments(); i++)
	{
		psSegment = pcOutput->GetSegment(i);
		uiStart = psSegment->uiAddress;
		uiEnd = uiStart + (uint32)psSegment->cBytes.NumElements();
		psRegion = FindRegion(uiStart, uiEnd);
		if (psRegion == NULL)
		{
			snprintf(szAddress, sizeof(szAddress), "0x%X", (unsigned int)uiStart);
			pszErrors->Append("A segment at ");
			pszErrors->Append(szAddress);
			pszErrors->Append(" is outside the image's regions.\n");
			return false;
		}
		memcpy(&mpuiImage[psRegion->uiOffset + (uiStart - psRegion->uiStart)], psSegment->cBytes.GetData(), psSegment->cBytes.NumElements());
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The image Made, to szFilename.
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65Image::Write(const char* szFilename)
{
	FILE*	pcFile;
	bool	bResult;

	if (!mpuiImage)
	{
		return false;
	}
	pcFile = fopen(szFilename, "wb");
	if (!pcFile)
	{
		return false;
	}
	bResult = fwrite(mpuiImage, 1, muiSize, pcFile) == muiSize;
	bResult &= fclose(pcFile) == 0;
	return bResult;
}


char* CAsm65Image::GetFile(void)							{ return mszFile.Text(); }
uint32 CAsm65Image::GetSize(void)							{ return muiSize; }
uint8 CAsm65Image::GetFill(void)							{ return muiFill; }
uint32 CAsm65Image::GetLimit(void)							{ return muiLimit; }
uint32 CAsm65Image::GetLoad(void)							{ return muiLoad; }
size CAsm65Image::NumRegions(void)							{ return masRegions.NumElements(); }
SAsm65ImageRegion* CAsm65Image::GetRegion(size uiIndex)		{ return masRegions.Get(uiIndex); }
uint8* CAsm65Image::GetImage(void)							{ return mpuiImage; }

