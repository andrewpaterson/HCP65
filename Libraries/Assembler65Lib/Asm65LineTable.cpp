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
#include "BaseLib/StringHelper.h"
#include "BaseLib/DiskFile.h"
#include "BaseLib/FileUtil.h"
#include "Asm65Object.h"
#include "Asm65LineTable.h"


//////////////////////////////////////////////////////////////////////////
//
// Lines by address; at one address the one added last (a later .line at
// the same place) last.
//
//////////////////////////////////////////////////////////////////////////
static int CompareAsm65Lines(const void* pvLeft, const void* pvRight)
{
	const SAsm65LineEntry*	psLeft;
	const SAsm65LineEntry*	psRight;

	psLeft = (const SAsm65LineEntry*)pvLeft;
	psRight = (const SAsm65LineEntry*)pvRight;
	if (psLeft->uiAddress != psRight->uiAddress)
	{
		return (psLeft->uiAddress < psRight->uiAddress) ? -1 : 1;
	}
	return (psLeft->uiOrder < psRight->uiOrder) ? -1 : ((psLeft->uiOrder > psRight->uiOrder) ? 1 : 0);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CAsm65LineTable::Init(void)
{
	maszFiles.Init();
	mcFileIndices.Init(true, true);
	masLines.Init();
	masFingerprints.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CAsm65LineTable::Kill(void)
{
	size	ui;

	for (ui = 0; ui < masFingerprints.NumElements(); ui++)
	{
		masFingerprints.Get(ui)->szLabel.Kill();
	}
	masFingerprints.Kill();
	masLines.Kill();
	mcFileIndices.Kill();
	maszFiles.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CAsm65LineTable::Clear(void)
{
	Kill();
	Init();
}


//////////////////////////////////////////////////////////////////////////
//
// The code at uiAddress is for line uiLine (1 based; 0 for none) of szFile.
//
//////////////////////////////////////////////////////////////////////////
void CAsm65LineTable::AddLine(uint32 uiAddress, const char* szFile, uint32 uiLine)
{
	SAsm65LineEntry*	psLine;

	psLine = masLines.Add();
	psLine->uiAddress = uiAddress;
	psLine->uiFile = FileIndex(szFile);
	psLine->uiLine = uiLine;
	psLine->uiOrder = (uint32)(masLines.NumElements() - 1);
}


//////////////////////////////////////////////////////////////////////////
//
// szFile's index in the files, added if it isn't one yet.
//
//////////////////////////////////////////////////////////////////////////
uint32 CAsm65LineTable::FileIndex(const char* szFile)
{
	uint32*	puiFile;
	uint32	uiFile;

	puiFile = mcFileIndices.Get(szFile);
	if (puiFile)
	{
		return *puiFile;
	}
	uiFile = (uint32)maszFiles.NumElements();
	maszFiles.Add(szFile);
	mcFileIndices.Put(szFile, &uiFile);
	return uiFile;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CAsm65LineTable::AddFingerprint(const char* szLabel, uint32 uiHash, int64 iTime)
{
	SAsm65LineFingerprint*	psFingerprint;

	psFingerprint = masFingerprints.Add();
	psFingerprint->szLabel.Init(szLabel);
	psFingerprint->uiHash = uiHash;
	psFingerprint->iTime = iTime;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CAsm65LineTable::Sort(void)
{
	masLines.Sort(CompareAsm65Lines);
}


//////////////////////////////////////////////////////////////////////////
//
// Every line's address moved by iOffset: a module's table from its link's
// addresses to its own (or to where it was loaded).
//
//////////////////////////////////////////////////////////////////////////
void CAsm65LineTable::Move(int64 iOffset)
{
	size	i;

	for (i = 0; i < masLines.NumElements(); i++)
	{
		masLines.Get(i)->uiAddress = (uint32)(masLines.Get(i)->uiAddress + iOffset);
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
size CAsm65LineTable::NumLines(void)
{
	return masLines.NumElements();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
SAsm65LineEntry* CAsm65LineTable::GetLine(size uiIndex)
{
	return masLines.SafeGet(uiIndex);
}


//////////////////////////////////////////////////////////////////////////
//
// The line the code at uiAddress is for: the last at or before it (after
// Sort).  NULL if there is none, or it is for no line.
//
//////////////////////////////////////////////////////////////////////////
SAsm65LineEntry* CAsm65LineTable::FindLine(uint32 uiAddress)
{
	size				uiLow;
	size				uiHigh;
	size				uiMiddle;
	SAsm65LineEntry*	psLine;

	uiLow = 0;
	uiHigh = masLines.NumElements();
	while (uiLow < uiHigh)
	{
		uiMiddle = (uiLow + uiHigh) / 2;
		if (masLines.Get(uiMiddle)->uiAddress <= uiAddress)
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
	psLine = masLines.Get(uiLow - 1);
	return (psLine->uiLine != 0) ? psLine : NULL;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
size CAsm65LineTable::NumFiles(void)
{
	return maszFiles.NumElements();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
char* CAsm65LineTable::GetFile(size uiIndex)
{
	return (uiIndex < maszFiles.NumElements()) ? maszFiles.Get(uiIndex)->Text() : NULL;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
size CAsm65LineTable::NumFingerprints(void)
{
	return masFingerprints.NumElements();
}


//////////////////////////////////////////////////////////////////////////
//
// Whether two paths name the same file: letters in either case, and '/'
// and '\' the same.
//
//////////////////////////////////////////////////////////////////////////
static bool Asm65SamePath(const char* szLeft, const char* szRight)
{
	char	cLeft;
	char	cRight;

	for (;; szLeft++, szRight++)
	{
		cLeft = (*szLeft == '\\') ? '/' : ToLower(*szLeft);
		cRight = (*szRight == '\\') ? '/' : ToLower(*szRight);
		if (cLeft != cRight)
		{
			return false;
		}
		if (cLeft == '\0')
		{
			return true;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The index of the file szFile names (Asm65SamePath).
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65LineTable::FindFile(const char* szFile, uint32* puiFile)
{
	size	ui;

	for (ui = 0; ui < maszFiles.NumElements(); ui++)
	{
		if (Asm65SamePath(maszFiles.GetText(ui), szFile))
		{
			*puiFile = (uint32)ui;
			return true;
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// Where the code for line uiLine of szFile starts, each place, added to
// pauiAddresses in address order: a line can have code in more than one
// place (a loop's test, a function inlined).  A line with no code is
// taken as the next line of the file that has some, as a debugger moves
// a breakpoint to it.  Returns the line found, 0 if there is none.
//
//////////////////////////////////////////////////////////////////////////
uint32 CAsm65LineTable::FindLineAddresses(const char* szFile, uint32 uiLine, CArrayUint32* pauiAddresses)
{
	uint32				uiFile;
	uint32				uiFound;
	size				ui;
	SAsm65LineEntry*	psLine;

	if (!FindFile(szFile, &uiFile))
	{
		return 0;
	}
	uiFound = 0;
	for (ui = 0; ui < masLines.NumElements(); ui++)
	{
		psLine = masLines.Get(ui);
		if ((psLine->uiFile == uiFile) && (psLine->uiLine >= uiLine) && ((uiFound == 0) || (psLine->uiLine < uiFound)))
		{
			uiFound = psLine->uiLine;
		}
	}
	if (uiFound == 0)
	{
		return 0;
	}
	for (ui = 0; ui < masLines.NumElements(); ui++)
	{
		psLine = masLines.Get(ui);
		if ((psLine->uiFile == uiFile) && (psLine->uiLine == uiFound))
		{
			pauiAddresses->Add(psLine->uiAddress);
		}
	}
	return uiFound;
}


//////////////////////////////////////////////////////////////////////////
//
// The code of the line the code at uiAddress is for, in every place it
// is: each from where it starts up to where the next line's starts, added
// to pauiRanges as a start and an end (exclusive), for a debugger to step
// the line.  False if the address has no line.
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65LineTable::FindLineRanges(uint32 uiAddress, CArrayUint32* pauiRanges)
{
	SAsm65LineEntry*	psFound;
	SAsm65LineEntry*	psLine;
	size				ui;
	size				uiNext;

	psFound = FindLine(uiAddress);
	if (psFound == NULL)
	{
		return false;
	}
	for (ui = 0; ui < masLines.NumElements(); ui++)
	{
		psLine = masLines.Get(ui);
		if ((psLine->uiFile != psFound->uiFile) || (psLine->uiLine != psFound->uiLine))
		{
			continue;
		}

		//Of lines at one address the last is the one there.
		for (uiNext = ui + 1; (uiNext < masLines.NumElements()) && (masLines.Get(uiNext)->uiAddress == psLine->uiAddress); uiNext++)
		{
		}
		if (uiNext != ui + 1)
		{
			psLine = masLines.Get(uiNext - 1);
			if ((psLine->uiFile != psFound->uiFile) || (psLine->uiLine != psFound->uiLine))
			{
				continue;
			}
		}
		pauiRanges->Add(psLine->uiAddress);
		pauiRanges->Add((uiNext < masLines.NumElements()) ? masLines.Get(uiNext)->uiAddress : 0xFFFFFFFF);
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Where the next line's code starts after uiAddress (a function's first
// line, after its entry code that has none).  False if no line is after
// it.
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65LineTable::FindNextLine(uint32 uiAddress, uint32* puiNext)
{
	size				uiLow;
	size				uiHigh;
	size				uiMiddle;
	SAsm65LineEntry*	psLine;

	uiLow = 0;
	uiHigh = masLines.NumElements();
	while (uiLow < uiHigh)
	{
		uiMiddle = (uiLow + uiHigh) / 2;
		if (masLines.Get(uiMiddle)->uiAddress <= uiAddress)
		{
			uiLow = uiMiddle + 1;
		}
		else
		{
			uiHigh = uiMiddle;
		}
	}
	for (; uiLow < masLines.NumElements(); uiLow++)
	{
		psLine = masLines.Get(uiLow);
		if (psLine->uiLine != 0)
		{
			*puiNext = psLine->uiAddress;
			return true;
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
SAsm65LineFingerprint* CAsm65LineTable::GetFingerprint(size uiIndex)
{
	return masFingerprints.SafeGet(uiIndex);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
SAsm65LineFingerprint* CAsm65LineTable::FindFingerprint(const char* szLabel)
{
	size	ui;

	for (ui = 0; ui < masFingerprints.NumElements(); ui++)
	{
		if (masFingerprints.Get(ui)->szLabel.Equals(szLabel))
		{
			return masFingerprints.Get(ui);
		}
	}
	return NULL;
}



//////////////////////////////////////////////////////////////////////////
//
// Writes the table as a chunk file.  pcFile is freed by the chunk file
// when it is a must free file (DiskFile, MemoryFile).
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65LineTable::Write(CAbstractFile* pcFile)
{
	CChunkFile	cChunkFile;
	bool		bResult;

	cChunkFile.Init(pcFile);
	bResult = cChunkFile.WriteOpen();
	if (bResult)
	{
		bResult = WriteChunks(&cChunkFile);
		bResult &= cChunkFile.WriteClose();
	}
	cChunkFile.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Reads a table Write wrote into an initialised, empty table.
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65LineTable::Read(CAbstractFile* pcFile)
{
	CChunkFile	cChunkFile;
	bool		bResult;

	//ReadClose frees what a failed ReadOpen allocated too.
	cChunkFile.Init(pcFile);
	bResult = cChunkFile.ReadOpen();
	bResult = bResult && ReadChunks(&cChunkFile);
	bResult &= cChunkFile.ReadClose();
	cChunkFile.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65LineTable::WriteFile(const char* szFilename)
{
	return Write(DiskFile(szFilename));
}


//////////////////////////////////////////////////////////////////////////
//
// False, with the table emptied, if the file can't be read or isn't a
// line table of this version.
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65LineTable::ReadFile(const char* szFilename)
{
	CFileUtil	cFileUtil;

	Clear();
	if (!cFileUtil.Exists(szFilename))
	{
		return false;
	}
	if (!Read(DiskFile(szFilename)))
	{
		Clear();
		return false;
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65LineTable::WriteChunks(CChunkFile* pcChunkFile)
{
	size					ui;
	SAsm65LineEntry*		psLine;
	SAsm65LineFingerprint*	psFingerprint;
	bool					bResult;

	bResult = pcChunkFile->WriteChunkBegin();
	bResult &= Asm65WriteMagic(pcChunkFile, ASM65_LINE_TABLE_MAGIC, ASM65_LINE_TABLE_VERSION);
	bResult &= pcChunkFile->WriteChunkEnd("Header");

	bResult &= pcChunkFile->WriteChunkBegin();
	bResult &= pcChunkFile->WriteInt32((uint32)maszFiles.NumElements());
	for (ui = 0; ui < maszFiles.NumElements(); ui++)
	{
		bResult &= Asm65WriteText(pcChunkFile, maszFiles.GetText(ui));
	}
	bResult &= pcChunkFile->WriteChunkEnd("Files");

	bResult &= pcChunkFile->WriteChunkBegin();
	bResult &= pcChunkFile->WriteInt32((uint32)masLines.NumElements());
	for (ui = 0; ui < masLines.NumElements(); ui++)
	{
		psLine = masLines.Get(ui);
		bResult &= pcChunkFile->WriteInt32(psLine->uiAddress);
		bResult &= pcChunkFile->WriteInt32(psLine->uiFile);
		bResult &= pcChunkFile->WriteInt32(psLine->uiLine);
		bResult &= pcChunkFile->WriteInt32(psLine->uiOrder);
	}
	bResult &= pcChunkFile->WriteChunkEnd("Lines");

	bResult &= pcChunkFile->WriteChunkBegin();
	bResult &= pcChunkFile->WriteInt32((uint32)masFingerprints.NumElements());
	for (ui = 0; ui < masFingerprints.NumElements(); ui++)
	{
		psFingerprint = masFingerprints.Get(ui);
		bResult &= psFingerprint->szLabel.WriteString(pcChunkFile);
		bResult &= pcChunkFile->WriteInt32(psFingerprint->uiHash);
		bResult &= pcChunkFile->WriteInt64(psFingerprint->iTime);
	}
	bResult &= pcChunkFile->WriteChunkEnd("Fingerprints");
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65LineTable::ReadChunks(CChunkFile* pcChunkFile)
{
	uint32					uiCount;
	uint32					ui;
	SAsm65LineEntry*		psLine;
	SAsm65LineFingerprint*	psFingerprint;
	CChars					sz;
	bool					bRead;

	if (!pcChunkFile->ReadChunkBegin("Header") || !Asm65ReadMagic(pcChunkFile, ASM65_LINE_TABLE_MAGIC, ASM65_LINE_TABLE_VERSION) || !pcChunkFile->ReadChunkEnd())
	{
		return false;
	}

	if (!pcChunkFile->ReadChunkBegin("Files") || !pcChunkFile->ReadInt32(&uiCount))
	{
		return false;
	}
	for (ui = 0; ui < uiCount; ui++)
	{
		sz.Init();
		bRead = Asm65ReadText(pcChunkFile, &sz);
		if (bRead)
		{
			FileIndex(sz.Text());
		}
		sz.Kill();
		if (!bRead)
		{
			return false;
		}
	}
	if (!pcChunkFile->ReadChunkEnd())
	{
		return false;
	}

	if (!pcChunkFile->ReadChunkBegin("Lines") || !pcChunkFile->ReadInt32(&uiCount))
	{
		return false;
	}
	for (ui = 0; ui < uiCount; ui++)
	{
		psLine = masLines.Add();
		if (!pcChunkFile->ReadInt32(&psLine->uiAddress) || !pcChunkFile->ReadInt32(&psLine->uiFile) ||
			!pcChunkFile->ReadInt32(&psLine->uiLine) || !pcChunkFile->ReadInt32(&psLine->uiOrder))
		{
			return false;
		}
		if (psLine->uiFile >= maszFiles.NumElements())
		{
			return false;
		}
	}
	if (!pcChunkFile->ReadChunkEnd())
	{
		return false;
	}

	if (!pcChunkFile->ReadChunkBegin("Fingerprints") || !pcChunkFile->ReadInt32(&uiCount))
	{
		return false;
	}
	for (ui = 0; ui < uiCount; ui++)
	{
		psFingerprint = masFingerprints.Add();
		psFingerprint->szLabel.Init();
		if (!Asm65ReadText(pcChunkFile, &psFingerprint->szLabel) || !pcChunkFile->ReadInt32(&psFingerprint->uiHash) ||
			!pcChunkFile->ReadInt64(&psFingerprint->iTime))
		{
			return false;
		}
	}
	return pcChunkFile->ReadChunkEnd();
}
