#include <stdio.h>
#include <string.h>
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
#include "DeviceRomLib/DeviceROMSchematic.h"


//////////////////////////////////////////////////////////////////////////
//
// pszDest is szFile with its extension changed to szExtension, without its
// directory if bHere.
//
//////////////////////////////////////////////////////////////////////////
static void DeviceROMFileName(const char* szFile, const char* szExtension, bool bHere, CChars* pszDest)
{
	size	uiDot;
	size	uiSlash;
	size	uiBackslash;
	size	uiStart;

	pszDest->Set(szFile);
	uiDot = pszDest->FindFromEnd('.');
	uiSlash = pszDest->FindFromEnd('/');
	uiBackslash = pszDest->FindFromEnd('\\');
	if ((uiDot != ARRAY_ELEMENT_NOT_FOUND) &&
		((uiSlash == ARRAY_ELEMENT_NOT_FOUND) || (uiDot > uiSlash)) &&
		((uiBackslash == ARRAY_ELEMENT_NOT_FOUND) || (uiDot > uiBackslash)))
	{
		pszDest->SetLength(uiDot);
	}
	pszDest->Append(szExtension);

	if (bHere)
	{
		uiStart = 0;
		if ((uiSlash != ARRAY_ELEMENT_NOT_FOUND) && ((uiBackslash == ARRAY_ELEMENT_NOT_FOUND) || (uiSlash > uiBackslash)))
		{
			uiStart = uiSlash + 1;
		}
		else if (uiBackslash != ARRAY_ELEMENT_NOT_FOUND)
		{
			uiStart = uiBackslash + 1;
		}
		pszDest->RemoveFromStart(uiStart);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Whether szFile is a KiCad schematic (.kicad_sch).
//
//////////////////////////////////////////////////////////////////////////
static bool DeviceROMIsSchematic(const char* szFile)
{
	size	uiLength;

	uiLength = strlen(szFile);
	return (uiLength > 10) && (StringInsensitiveCompare(&szFile[uiLength - 10], ".kicad_sch") == 0);
}


//////////////////////////////////////////////////////////////////////////
//
// Works out the board's description from szSchematic and writes it to
// szXMLFile.  False, having said why, if it can't.
//
//////////////////////////////////////////////////////////////////////////
static bool DeviceROMDescribe(const char* szSchematic, const char* szXMLFile)
{
	CDeviceROMSchematic	cSchematic;
	CChars				szError;
	bool				bResult;

	cSchematic.Init();
	szError.Init();
	bResult = cSchematic.Read(szSchematic, &szError);
	if (!bResult)
	{
		fprintf(stderr, "DeviceROM: %s: %s.\n", szSchematic, szError.Text());
	}
	else if (!cSchematic.WriteXML(szXMLFile))
	{
		fprintf(stderr, "DeviceROM: can't write %s.\n", szXMLFile);
		bResult = false;
	}
	else
	{
		printf("%s: %s %s from %s.\n", szXMLFile, cSchematic.GetManufacturer(), cSchematic.GetName(), szSchematic);
	}
	szError.Kill();
	cSchematic.Kill();
	return bResult;
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
//	DeviceROM <board.kicad_sch> [<board.xml> [<image.bin>]]
//
// The image is written beside the description, as <board>.bin, unless it
// is named.  From a schematic the description is worked out first
// (DeviceROMSchematic.h) and written, as <board>.xml in the current
// directory unless it is named, and the image is made from that file.
//
//////////////////////////////////////////////////////////////////////////
int __cdecl main(int argc, char* argv[])
{
	CChars	szXMLFile;
	CChars	szImageFile;
	bool	bSchematic;
	bool	bResult;

	bSchematic = (argc >= 2) && DeviceROMIsSchematic(argv[1]);
	if ((argc < 2) || (argc > (bSchematic ? 4 : 3)))
	{
		fprintf(stderr, "Usage: DeviceROM <board.xml> [<image.bin>]\n");
		fprintf(stderr, "       DeviceROM <board.kicad_sch> [<board.xml> [<image.bin>]]\n");
		fprintf(stderr, "Makes a board's %d byte device ROM image (format %d) from its XML description, or from its schematic\n", DEVICE_ROM_SIZE, DEVICE_ROM_FORMAT);
		fprintf(stderr, "through the description worked out from it.\n");
		return 1;
	}

	TypesInit();
	TypeConverterInit();
	NumberInit();
	MemoryInit();
	DataMemoryInit();

	szXMLFile.Init();
	szImageFile.Init();
	bResult = true;
	if (bSchematic)
	{
		if (argc >= 3)
		{
			szXMLFile.Set(argv[2]);
		}
		else
		{
			DeviceROMFileName(argv[1], ".xml", true, &szXMLFile);
		}
		bResult = DeviceROMDescribe(argv[1], szXMLFile.Text());
		if (argc == 4)
		{
			szImageFile.Set(argv[3]);
		}
	}
	else
	{
		szXMLFile.Set(argv[1]);
		if (argc == 3)
		{
			szImageFile.Set(argv[2]);
		}
	}
	if (szImageFile.Empty())
	{
		DeviceROMFileName(szXMLFile.Text(), ".bin", false, &szImageFile);
	}
	if (bResult)
	{
		bResult = DeviceROMMake(szXMLFile.Text(), szImageFile.Text());
	}
	szImageFile.Kill();
	szXMLFile.Kill();

	DataMemoryKill();
	MemoryKill();
	NumberKill();
	TypeConverterKill();
	TypesKill();
	return bResult ? 0 : 1;
}
