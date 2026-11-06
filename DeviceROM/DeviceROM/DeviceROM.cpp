#include <stdio.h>
#include "BaseLib/Define.h"
#include "BaseLib/TypeNames.h"
#include "BaseLib/TypeConverter.h"
#include "BaseLib/NumberControl.h"
#include "BaseLib/GlobalMemory.h"
#include "BaseLib/DataMemory.h"
#include "BaseLib/Chars.h"
#include "BaseLib/ArrayElementNotFound.h"
#include "BaseLib/FileUtil.h"
#include "BaseLib/XMLFile.h"
#include "DeviceRomLib/DeviceROM.h"


//////////////////////////////////////////////////////////////////////////
//
// szImageFile is szXMLFile with its extension changed to .bin.
//
//////////////////////////////////////////////////////////////////////////
static void DeviceROMImageName(const char* szXMLFile, CChars* pszImageFile)
{
	size	uiDot;
	size	uiSlash;
	size	uiBackslash;

	pszImageFile->Set(szXMLFile);
	uiDot = pszImageFile->FindFromEnd('.');
	uiSlash = pszImageFile->FindFromEnd('/');
	uiBackslash = pszImageFile->FindFromEnd('\\');
	if ((uiDot != ARRAY_ELEMENT_NOT_FOUND) &&
		((uiSlash == ARRAY_ELEMENT_NOT_FOUND) || (uiDot > uiSlash)) &&
		((uiBackslash == ARRAY_ELEMENT_NOT_FOUND) || (uiDot > uiBackslash)))
	{
		pszImageFile->SetLength(uiDot);
	}
	pszImageFile->Append(".bin");
}


//////////////////////////////////////////////////////////////////////////
//
// Reads the board's description from szXMLFile and writes its ROM image
// to szImageFile.  False, having said why, if it can't.
//
//////////////////////////////////////////////////////////////////////////
static bool DeviceROMMake(const char* szXMLFile, const char* szImageFile)
{
	CXMLFile		cFile;
	CFileUtil		cFileUtil;
	CChars			szName;
	CChars			szDirectory;
	CChars			szError;
	CMarkupTag*		pcRoot;
	CDeviceROM		cROM;
	FILE*			pFile;
	bool			bResult;
	char*			szROM;

	szName.Init();
	szDirectory.Init();
	szError.Init();
	cFileUtil.SplitPath(szXMLFile, &szName, &szDirectory);
	if (szDirectory.Empty())
	{
		szDirectory.Set(".");
	}

	cFile.Init();
	pcRoot = NULL;
	if (cFileUtil.Exists(szXMLFile) && cFile.Read(szName.Text(), szDirectory.Text()) && cFile.mcMarkup.mpcDoc)
	{
		pcRoot = cFile.mcMarkup.mpcDoc->GetRootTag();
	}
	if (!pcRoot)
	{
		fprintf(stderr, "DeviceROM: can't read %s.\n", szXMLFile);
		bResult = false;
	}
	else if (!cROM.LoadXML(pcRoot, &szError))
	{
		fprintf(stderr, "DeviceROM: %s: %s.\n", szXMLFile, szError.Text());
		bResult = false;
	}
	else
	{
		pFile = fopen(szImageFile, "wb");
		bResult = pFile && (fwrite(cROM.GetROM(), 1, DEVICE_ROM_SIZE, pFile) == DEVICE_ROM_SIZE);
		if (pFile)
		{
			bResult = (fclose(pFile) == 0) && bResult;
		}
		if (bResult)
		{
			szROM = (char*)cROM.GetROM();
			printf("%s: %s %s, revision %d, %d registers.\n", szImageFile, &szROM[DEVICE_ROM_MAKER_OFFSET], &szROM[DEVICE_ROM_NAME_OFFSET], cROM.GetROM()[DEVICE_ROM_REVISION_OFFSET], cROM.NumRegisters());
		}
		else
		{
			fprintf(stderr, "DeviceROM: can't write %s.\n", szImageFile);
		}
	}

	cFile.Kill();
	szError.Kill();
	szDirectory.Kill();
	szName.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The device ROM tool: makes the 1KB device ROM image that is flashed onto
// an HCP65 device board from the board's XML description (DeviceROM.h).
//
//	DeviceROM <board.xml> [<image.bin>]
//
// The image is written beside the description, as <board>.bin, unless it
// is named.
//
//////////////////////////////////////////////////////////////////////////
int __cdecl main(int argc, char* argv[])
{
	CChars	szImageFile;
	bool	bResult;

	if ((argc < 2) || (argc > 3))
	{
		fprintf(stderr, "Usage: DeviceROM <board.xml> [<image.bin>]\n");
		fprintf(stderr, "Makes a board's %d byte device ROM image (format %d) from its XML description.\n", DEVICE_ROM_SIZE, DEVICE_ROM_FORMAT);
		return 1;
	}

	TypesInit();
	TypeConverterInit();
	NumberInit();
	MemoryInit();
	DataMemoryInit();

	szImageFile.Init();
	if (argc == 3)
	{
		szImageFile.Set(argv[2]);
	}
	else
	{
		DeviceROMImageName(argv[1], &szImageFile);
	}
	bResult = DeviceROMMake(argv[1], szImageFile.Text());
	szImageFile.Kill();

	DataMemoryKill();
	MemoryKill();
	NumberKill();
	TypeConverterKill();
	TypesKill();
	return bResult ? 0 : 1;
}
